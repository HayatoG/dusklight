// Switch audio backend over libnx audren.
//
// dusk/audio/DuskAudioSystem.cpp targets SDL3: it opens an SDL_AudioStream (interleaved float32
// stereo @ 32 kHz) with a "needs more data" callback, and from inside that callback PUSHES the
// rendered PCM via SDL_PutAudioStreamData. The reference build (confirmed by RE of its NRO —
// 05_audio_backend.c) uses SDL3's own libnx/audren audio driver; here we implement just the SDL
// surface DuskAudioSystem uses, backed by audren.
//
// audren is a buffer-queue (not callback) API, so a dedicated thread pulls the PCM the game pushed
// into a ring and feeds it to audren wavebufs. float32 is fed directly (PcmFormat_Float) — no
// conversion. Important details:
//   - SDL semantics: SDL_OpenAudioDeviceStream returns a PAUSED stream; the game renders DspInit()
//     AFTER opening, then calls SDL_ResumeAudioStreamDevice. So we must NOT render until resumed.
//   - The DSP buffer memory handed to audren must be flushed from CPU cache (armDCacheFlush).

#include <switch.h>

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <pthread.h>

// Completes the opaque type forward-declared by the SDL3 shim. DuskAudioSystem only ever holds a
// pointer to it; the global audren state below is the real backing.
struct SDL_AudioStream {
  int _unused;
};

namespace {

constexpr int kChannels = 2;
constexpr int kBytesPerFrame = kChannels * static_cast<int>(sizeof(float)); // interleaved L/R f32 = 8
constexpr int kNumSlots = 4;
constexpr int kSlotFrames = 1024;                         // per-channel frames per wavebuf
constexpr int kSlotBytes = kSlotFrames * kBytesPerFrame;  // 8 KiB
constexpr int kRingBytes = 64 * 1024;
constexpr int kTargetFill = kSlotBytes * 2;               // keep ~2 slots buffered ahead

const AudioRendererConfig kArConfig = {
    /* output_rate     */ AudioRendererOutputRate_48kHz,
    /* num_voices      */ 4,
    /* num_effects     */ 0,
    /* num_sinks       */ 1,
    /* num_mix_objs    */ 1,
    /* num_mix_buffers */ 2,
};

alignas(AUDREN_MEMPOOL_ALIGNMENT) u8 g_pool[(kNumSlots * kSlotBytes + 0xFFF) & ~0xFFF];

AudioDriver g_drv;
AudioDriverWaveBuf g_wavebufs[kNumSlots];
bool g_ready = false;
std::atomic_bool g_running{false};
std::atomic_bool g_paused{true}; // SDL opens paused; resumed after DspInit
pthread_t g_thread{};
bool g_threadValid = false;

SDL_AudioStreamCallback g_callback = nullptr;
void* g_userdata = nullptr;
int g_freq = 32000;

// Byte ring for pushed float32 PCM (single producer = game/audio thread via the callback, single
// consumer = audio thread drain). Monotonic counters; index = counter % kRingBytes.
u8 g_ring[kRingBytes];
u64 g_ringHead = 0; // read counter
u64 g_ringTail = 0; // write counter
std::mutex g_ringMutex;

inline u64 ring_used_locked() { return g_ringTail - g_ringHead; }

void ring_push(const u8* src, int len) {
  std::lock_guard<std::mutex> lk(g_ringMutex);
  if (static_cast<u64>(len) > kRingBytes) { // keep only the freshest kRingBytes
    src += len - kRingBytes;
    len = kRingBytes;
  }
  const u64 freeBytes = kRingBytes - ring_used_locked();
  if (static_cast<u64>(len) > freeBytes) { // drop oldest to make room (underrun-safe, no stall)
    g_ringHead += static_cast<u64>(len) - freeBytes;
  }
  for (int i = 0; i < len; ++i) {
    g_ring[(g_ringTail + i) % kRingBytes] = src[i];
  }
  g_ringTail += len;
}

int ring_drain(u8* dst, int max) {
  std::lock_guard<std::mutex> lk(g_ringMutex);
  int n = static_cast<int>(ring_used_locked());
  if (n > max) {
    n = max;
  }
  for (int i = 0; i < n; ++i) {
    dst[i] = g_ring[(g_ringHead + i) % kRingBytes];
  }
  g_ringHead += n;
  return n;
}

void* audio_thread(void*) {
  while (g_running.load()) {
    if (g_paused.load() || !g_ready) {
      svcSleepThread(5'000'000ULL); // 5 ms
      continue;
    }

    // Top up the ring by asking the game to render more (the callback pushes via
    // SDL_PutAudioStreamData). Do NOT hold g_ringMutex across the callback — it locks the ring.
    u64 used;
    {
      std::lock_guard<std::mutex> lk(g_ringMutex);
      used = ring_used_locked();
    }
    if (used < static_cast<u64>(kTargetFill) && g_callback != nullptr) {
      const int framesWanted = static_cast<int>((kTargetFill - used) / kBytesPerFrame);
      g_callback(g_userdata, nullptr, framesWanted, framesWanted);
    }

    audrvUpdate(&g_drv);

    for (int i = 0; i < kNumSlots; ++i) {
      AudioDriverWaveBuf& wb = g_wavebufs[i];
      if (wb.state != AudioDriverWaveBufState_Free && wb.state != AudioDriverWaveBufState_Done) {
        continue; // still in flight
      }
      u8* slot = g_pool + i * kSlotBytes;
      const int n = ring_drain(slot, kSlotBytes);
      if (n < kBytesPerFrame) {
        continue; // nothing (or partial frame) ready
      }
      armDCacheFlush(slot, n);
      wb = {};
      wb.data_raw = slot;
      wb.size = static_cast<u64>(n);
      wb.start_sample_offset = 0;
      wb.end_sample_offset = n / kBytesPerFrame;
      audrvVoiceAddWaveBuf(&g_drv, 0, &wb);
    }

    audrvUpdate(&g_drv);
    audrenWaitFrame();
  }
  return nullptr;
}

bool audren_setup(int freq) {
  Result rc = audrenInitialize(&kArConfig);
  if (R_FAILED(rc)) {
    std::printf("[audio] audrenInitialize failed: 0x%x\n", rc);
    return false;
  }
  rc = audrvCreate(&g_drv, &kArConfig, kChannels);
  if (R_FAILED(rc)) {
    std::printf("[audio] audrvCreate failed: 0x%x\n", rc);
    return false;
  }

  const int mpid = audrvMemPoolAdd(&g_drv, g_pool, sizeof(g_pool));
  audrvMemPoolAttach(&g_drv, mpid);

  static const u8 sinkChannels[] = {0, 1};
  audrvDeviceSinkAdd(&g_drv, AUDREN_DEFAULT_DEVICE_NAME, kChannels, sinkChannels);

  rc = audrenStartAudioRenderer();
  if (R_FAILED(rc)) {
    std::printf("[audio] audrenStartAudioRenderer failed: 0x%x\n", rc);
    return false;
  }

  if (!audrvVoiceInit(&g_drv, 0, kChannels, PcmFormat_Float, freq)) {
    std::printf("[audio] audrvVoiceInit failed\n");
    return false;
  }
  audrvVoiceSetDestinationMix(&g_drv, 0, AUDREN_FINAL_MIX_ID);
  audrvVoiceSetMixFactor(&g_drv, 0, 1.0f, 0, 0); // src L -> dst L
  audrvVoiceSetMixFactor(&g_drv, 0, 1.0f, 1, 1); // src R -> dst R
  audrvVoiceStart(&g_drv, 0);

  for (auto& wb : g_wavebufs) {
    wb = {};
    wb.state = AudioDriverWaveBufState_Free;
  }
  return true;
}

SDL_AudioStream g_stream{}; // sentinel handed back to DuskAudioSystem

} // namespace

bool SDL_Init(Uint32 /*flags*/) { return true; }

SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID /*devid*/, const SDL_AudioSpec* spec,
                                           SDL_AudioStreamCallback callback, void* userdata) {
  g_callback = callback;
  g_userdata = userdata;
  g_freq = (spec != nullptr && spec->freq > 0) ? spec->freq : 32000;

  g_ready = audren_setup(g_freq);
  if (!g_ready) {
    std::printf("[audio] audren setup failed; running silent\n");
    return &g_stream; // non-null so DuskAudioSystem keeps running (just silent)
  }

  // Start the worker, but leave it PAUSED — DuskAudioSystem renders DspInit() AFTER this and only
  // then calls SDL_ResumeAudioStreamDevice. Rendering before DspInit would touch uninitialised DSP.
  g_running.store(true);
  g_paused.store(true);
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) == 0) {
    pthread_attr_setstacksize(&attr, 256u * 1024u);
    if (pthread_create(&g_thread, &attr, audio_thread, nullptr) == 0) {
      g_threadValid = true;
    } else {
      std::printf("[audio] failed to start audio thread; running silent\n");
      g_running.store(false);
      g_ready = false;
    }
    pthread_attr_destroy(&attr);
  }
  std::printf("[audio] audren ready: %d Hz, %d ch, float32\n", g_freq, kChannels);
  return &g_stream;
}

bool SDL_PutAudioStreamData(SDL_AudioStream* /*stream*/, const void* buf, int len) {
  if (g_ready && buf != nullptr && len > 0) {
    ring_push(static_cast<const u8*>(buf), len);
  }
  return true;
}

bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* /*stream*/) {
  g_paused.store(false);
  return true;
}

bool SDL_PauseAudioStreamDevice(SDL_AudioStream* /*stream*/) {
  g_paused.store(true);
  return true;
}
