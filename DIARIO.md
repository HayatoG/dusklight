# DIÁRIO — A saga do Dusklight no Switch

> Da investigação forense da .nro privada do Dan até o jogo rodando a 17 fps
> num Tegra X1. Narrativa em primeira pessoa de tudo que a gente fez —
> incluindo o pivô maluco de construir nosso próprio driver Vulkan pro
> Switch.
>
> Escrito em 2026-05-29 como material-fonte pra um vídeo de YouTube.
> Cobre 5+ meses de trabalho condensados em arcos narrativos.

---

## Ato 0 — A pergunta

Tudo começou com uma pergunta simples: **por que ninguém porta Twilight
Princess pro Switch homebrew?**

Aliás, alguém já portou. Tem uma `dusk.nro` flutuando pela internet —
release privada de um cara chamado **Dan** (aliases: `givethesourceplox`,
`dantiicu`, `Tiicu`). Roda. A galera viu. Mas Dan não compartilha o
código-fonte. O perfil dele no GitHub tem um fork `dusklight-NX` que é só
README — uma piada com o próprio username.

A premissa do projeto: **fazer a mesma coisa que o Dan fez, mas em código
aberto**. Pegar o decomp upstream `TwilitRealm/dusklight` (PC), o
`encounter/aurora` engine, e empacotar como NRO. Devia ser fácil. *Devia.*

> *Spoiler: não foi.*

---

## Ato 1 — Investigação forense (fevereiro a abril 2026)

### 1.1 O que tem dentro da `dusk.nro` do Dan

Primeira coisa que fizemos: dump de strings da binary do Dan.
`strings dusk.nro | grep -E "vk_|GL_|deko"`. Aparece um caos:

- `vkCreateInstance`, `vkCreateSwapchainKHR`, `VK_NN_vi_surface` —
  **Vulkan.** Não OpenGL, não deko3d.
- Strings de paths Mesa: `src/nouveau/vulkan/nvk_*`, `src/vulkan/wsi/*` —
  ele tá usando **NVK**, o driver Vulkan open-source pra GPUs NVIDIA.
- `nvkmd/switch/nvkmd_switch_dev.c`, `wsi_common_switch.c` — arquivos que
  NÃO existem no Mesa upstream. Dan tem patches custom.
- `vkCreateViSurfaceNN` — extensão proprietária da NVIDIA pro Tegra X1
  (`VK_NN_vi_surface`).

Conclusão: o Dan **portou o NVK pro Switch**. O Tegra X1 do Switch é GPU
Maxwell (GM20B), que o NVK suporta no Linux desktop. Ele cross-compilou o
Mesa pra aarch64-none-elf (devkitA64) e fez um winsys custom usando libnx
em vez de libdrm.

Isso é... ambicioso pra caralho.

### 1.2 O que é público vs privado

Audit completo do perfil `dantiicu/`:

| Projeto | Público? | O que é |
|---|---|---|
| `aurora-switch` | ✅ branch `switch f0f3511` | Aurora com Switch port (libnx em vez de SDL3) |
| `dawn-switch` | ✅ branch `switch 5f3439f159` | Dawn (WebGPU) com backend Switch |
| `vulkan-triangle-test-switch` | ✅ MIT | Sample Vulkan que prova o stack do Dan |
| `nvk-switch` | ❌ **PRIVADO** | A peça crítica — o driver NVK |

A pegadinha: **só o driver Vulkan é fechado**. Todo o resto da arquitetura
do Dan tá aberto. A pergunta vira: a gente recria o `nvk-switch` ou tenta
um caminho diferente?

### 1.3 Discord do Dan (descoberta de 30 de janeiro)

Achei um post do Dan no Discord dele de 30/01/2026 explicando a receita
dele de port:

> "Since libnx 1.4.0 switch supports open gl core (same from pc)" + link
> pro switch-mesa thread no fórum devkitPro.

Dan **literalmente diz** que o caminho recomendado é OpenGL ES via
switch-mesa. Vulkan/NVK é escolha de performance dele, não necessidade.

Isso muda o plano. Não precisa construir um driver Vulkan inteiro. Tem um
caminho público.

---

## Ato 2 — A primeira tentativa: deko3d (maio 2026)

### 2.1 Por que deko3d primeiro

`deko3d` é a API gráfica oficial do devkitPro pro Switch. Performance
máxima, design moderno tipo Vulkan, mas **NVIDIA-specific** e nada de
runtime shader compile.

A gente foi com deko3d pela performance. Plano:
1. Build base do Aurora com `AURORA_BACKEND_DEKO3D=ON`
2. Implementar `DekoRenderInterface` pra RmlUi (UI)
3. Implementar GX → deko replay pro game render
4. Profit

Phase 4 do plano (em `platforms/switch/PLAN_PHASE4.md`): GX → GLSL via uam,
cache de DKSH pré-compilado, shader hash → dispatch.

### 2.2 Primeira run no hardware real (CONT-9)

20 de maio. Pela primeira vez a gente roda o `.nro` num Switch real (não
emulador). E aí começa o teatro:

```
nxlink -s -a 192.168.1.7 dusklight.nro
Connection failed
```

**WTF.** Ping funciona. Switch tá na mesma rede. Por que não conecta?

Descoberta dolorosa: o **Windows Defender bloqueia o `nxlink.exe`
silenciosamente**. UDP inbound port 28280, sem erro visível, só "Connection
failed". Adiciona uma firewall rule:

```powershell
New-NetFirewallRule -DisplayName "nxlink inbound" -Direction Inbound \
    -Protocol UDP -Action Allow -Program "C:\devkitPro\tools\bin\nxlink.exe"
```

Funciona. Anota essa pra sempre.

### 2.3 O cascata de crashes do CPU

Run no hardware. deko3d inicializa. RmlUi mostra o prelaunch. **Crasha** em
`JFWSystem::firstInit()` na criação do root-heap.

Os emuladores (Eden, Yuzu) toleravam estado uninitialized que o Tegra real
NÃO tolera. Cada crash exige FTP do crash report do Atmosphère →
`addr2line` → patch. Lista:

| # | Crash | Causa | Fix |
|---|---|---|---|
| 1 | `firstInit` TLS fault | global-dynamic TLS quebra em libnx (sem `__tls_get_addr`) | `__attribute__((tls_model("initial-exec")))` no `sCurrentHeap` |
| 2 | `getCurrentHeap()` null | `#if TARGET_PC` matava o fallback (TARGET_PC tá definido no Switch!) | Mover pra .cpp out-of-line |
| 3 | audio `loadSeWave` null deref | `JAUSectionHeap` nullptr (audio stubbed) | `DUSK_AUDIO_DISABLED` guard |
| 4 | fade `fopOvlpM_SceneIsStart` null | `overlap_task` nullptr | guard per-site |
| 5 | vibration `m_gamePad[]` null | GC gamepad subsystem ausente | guard leaf accessor |

Reached `dScnPly_Create` + ~12 frames presenting blue clear na TV.

E aí... **gx=0**. Zero draws de geometria. A engine emite ~9-17KB/frame de
GX REGISTER state mas ZERO draw primitives. O drawlist tá vazio.

### 2.4 O deadend do deko3d

Problema descoberto: **deko3d não compila shader em runtime**. `uam` é só
CLI, na hora do build. Pra GX → deko replay, a gente teria que:

1. Capturar TODOS os shader configs possíveis do jogo
2. Pré-compilar como `.dksh` no host
3. Embedar como C header
4. Lookup por hash em runtime

Isso é **frágil pra caralho**. Qualquer state novo = miss no cache = stall.
E o decomp do TP tem milhares de configurações TEV diferentes.

Decisão: pivô.

---

## Ato 3 — O pivô GLES (final de maio)

### 3.1 Switch-Mesa via OpenGL ES

switch-mesa expõe libEGL/libGLESv2/libglapi em devkitPro. **Tem runtime
shader compile**. Pega o glsl, manda pro Mesa, ele compila pra Maxwell.

Plano novo (em `PLAN_GLES.md`): trocar o backend Aurora pra GLES, gerar GLSL
em runtime do GX, eliminar todo o overhead do DKSH cache.

CONT-7 (24 de maio): engine boota → TITLE → auto-advances → NAME scene,
**3000+ frames sem crash**. ✅

Mas... gx=0 ainda. Os actors da cena registram tags de draw
(`fopAcCt ret=COMPLEATE -> REGISTER ×3`) mas o `fpcM_DrawIterater` chega
apenas em 2 root-layer procs e nunca desce pros child layers.

Tentei mudar o iter pra usar a flat queue do EXECUTE — **regressão**: cenas
param de carregar. Reverti.

Mais frustrante: o **replay capture mostra `gx=1, [title] Draw`** nos logs
— mas ZERO pixel na TV. A engine ACHA que tá desenhando, o backend GLES
recebe os bytes, mas nada aparece.

> *"Verificar pela TV, não pelos logs"* — virou heurística #21.

### 3.2 A descoberta sobre o Dawn-on-GLES

25 de maio. Olhando o `extern/aurora/extern/dawn` (submodule), descubro que
é... `dantiicu/dawn-switch`. O fork PÚBLICO do Dan. Com backend GL
implementado. Com `robustness/fence_sync → glFinish`, com `static-EGL`, com
o `Tint::GlslWriter` real.

Caminho alternativo: usar o Dawn do Dan + reusar o COMPLETO renderer do
Aurora (que era WebGPU-target originalmente). Sem Vulkan, sem replay
custom.

Decisive test: configurar com `DAWN_ENABLE_OPENGLES=ON`. **Falha em
runtime**: Dawn cai pro backend Null silenciosamente. Tudo "funciona" mas
nada renderiza.

Heurística #3: `bool initialize() returned true` **NÃO** significa
funcionando. Sempre confere `g_backendType`. Se for `Null`, vc rendeu nada.

---

## Ato 4 — O pivô que ninguém viu vindo (final de maio)

### 4.1 "Vamos construir nosso próprio NVK"

26 de maio. Frustração alta. deko3d travado em gx=0. GLES caindo pro Null.
Dan tem um driver que funciona mas não compartilha.

Decisão **maluca**: vamos construir nosso próprio NVK pro Switch.

Isso é tipo decidir construir uma turbina a jato porque você quer voar pro
Rio. Existe. É feito. Mas o esforço...

Hints do Dan em outras conversas no Discord:

> "Understand NVK, understand nwindow, libnx, build a WSI — you are done."

O caminho dele:
1. Mesa 20.13 → vai subindo versão por versão até 25.3.6
2. Rust nightly + `rust-src` pro NAK shader compiler (target tier-3
   `aarch64-nintendo-switch-freestanding`)
3. NIL (image layout lib) via cbindgen
4. drm_shim que substitui libdrm por chamadas libnx
5. WSI custom (`VK_NN_vi_surface`)

Tempo dele: ~1.5 mês. Sabíamos que ia ser caro.

### 4.2 Setup do switch-nvk

Repo `D:\switch-nvk\`. Estrutura:

```
switch-nvk/
├── mesa-25/              # Mesa 25.0.7 (não 25.3.6 — versão estável anterior)
├── patches/              # Nossos patches Switch
├── apply-wsi-switch.sh   # Re-aplica idempotentemente
├── winsys/
│   ├── drm_shim.c        # libdrm → libnx ioctls
│   ├── smoke/            # Test apps (nvk_tri, nvk_logo, etc.)
│   └── wsi/              # Nosso WSI VI surface
└── Dockerfile            # Toolchain: devkitA64 + Rust nightly + meson + bindgen
```

Dockerfile é não-trivial:
- devkitpro/devkita64 base
- python3 + meson + mako
- Rust nightly (NAK precisa `-Zbuild-std` pro tier-3 target)
- bindgen + cbindgen
- rustfmt (Meson erra sem ele)

Image final: 1.49 GB. Hoje publicada em
`ghcr.io/hayatog/switch-nvk-build:latest` (private).

### 4.3 M1, M2 — o smoke test

**Milestone 1:** `vkCreateInstance` + `vkEnumeratePhysicalDevices` retornam
sem erro. Conseguimos. Fácil.

**Milestone 2:** smoke test completo — criar device, alocar buffer,
`vkCmdFillBuffer(0xCAFEBABE)`, submit, esperar, ler de volta.

Aqui começa o inferno.

### 4.4 O muro FECS

V22 do smoke test. Roda no Tegra real. Trava em
`vkQueueSubmit returns 0xd5c`. A gente READ ERRADO: `0xd5c` parecia
`InsufficientMemory`. Inventei uma teoria inteira sobre "memory pressure
ramp" e tentei warmup. **Erro completo**.

Decodificação correta (encontrei pesquisando libnx):
- `0xd5c` = `MAKERESULT(348, 6)` = `LibnxNvidiaError_Timeout`
- Timeout no Tegra = **channel reset**
- Channel reset = algum syscall feriu o nv driver do Horizon

GetErrorInfo retornou:
- type=2 GR (graphics)
- info[0]=0x80000 (FECS bit)
- info[1]=0x102310 (trapped method)
- info[4]=0xb197 (class — 3D)

GREP no Mesa: `0x2310` = `SET_FALCON04`. Procura: `nvk_mme_set_priv_reg` em
`nvk_cmd_draw.c`. **AÊ:**

```c
// NVK init writes privileged GR regs via the FECS falcon
// (clears bit3 of sm_disp_ctrl + bit14 of warp_esr_report_mask)
mme_set_priv_reg(...);
```

NVK escreve registros privilegiados via o falcon FECS na inicialização.
**Horizon (OS do Switch) BLOQUEIA escritas FECS de homebrew.** Resultado:
falcon fault → MMU notifier type=31 → channel reset → submit Timeout.

Os comments do próprio Mesa dizem que esses writes são "non-essential
robustness tweaks". A gente NÃO PRECISA deles.

**v28 fix:** no-op `nvk_mme_set_priv_reg`. Init pushbuf encolheu
`0x1d0c → 0x1cd0`. Cross-multi-runs no Tegra: **NO MORE ERRNOTIF type=31**.
Passamos o muro FECS.

### 4.5 O bug do timeout (33 minutos)

V28 passou FECS. Mas init EXEC roda e... **trava 3 minutos.** Black screen
silencioso. Achei que era infinite hang.

Olhei o código com olho fresco:

```c
nvFenceWait(&fence, 2000000000);  // 2e9
```

> *"2 segundos certo? 2000000000 nanosegundos."*

**NÃO.** `nvFenceWait` toma **microsegundos**. `2e9 µs = 2000s = 33min`.
Não era hang, era timeout absurdo.

V29: `2e9 → 2e6` (2 segundos reais). App TERMINA. Primeiro log COMPLETO.

Heurística #19: **sempre ler doc da libnx pra unidades de timeout**. Cada
API tem unidade diferente.

### 4.6 O fix do fence cmdlist (v32 — SMOKE PASSED)

Init agora completa. Mas o fill ainda falha — buffer não tem
`0xCAFEBABE` no readback.

Investigação profunda. Mesa 20 (devkitpro-mesa fork público) faz a mesma
coisa em desktop GL e funciona. Diff: depois de
`nvGpuChannelIncrFence`, ele **APPENDA o cmdlist de fence builtin** com:

```c
ch->cmdbuf_va, ch->fence_num_cmds, NOT_MAIN | NO_PREFETCH
```

`pushbuf.c:226` em `libdrm_nouveau`. A nossa removeu esse append na v22
(quando interpretei errado o 0xd5c).

O 3º dword desse cmdlist tem `bit20=syncpt incr` + `bit16=GPU L2 FLUSH`.
Um cmdlist conserta:
- **Completion** (syncpt fechado correto)
- **Coherency** (L2 flush → CPU vê 0xCAFEBABE)

V32 fix:

```
I VERIFY OK: all 1024 words == 0xcafebabe
SMOKE TEST PASSED — NVK rendered to memory on Tegra
```

🎉 Primeira mensagem de sucesso real. Trabalho de **dias** condensado em
uma linha.

### 4.7 M3 — Gráficos de verdade

Smoke test passou. Próximo: **renderizar pixels**.

`nvk_tri.c`: vertex+fragment shaders, pipeline, render pass.

Shaders são GLSL → glslangValidator → SPIR-V → embedados como C header
(`triangle.vert_spv`, `triangle.frag_spv`) → carregados via
`vkCreateShaderModule`. NAK (NVK shader compiler, em Rust) compila SPIR-V
→ Maxwell ISA.

**TRIÂNGULO AMARELO na TV.** Via libnx framebuffer blit (não WSI ainda).

```
center_pixel=0xff00ffff (yellow ARGB)
readback yellow=722 black=3374
```

🎉 Triangle de Vulkan rendado pelo NOSSO próprio driver.

Crescendo:
- `nvk_logo.c`: textured quad. Sampler, descriptor set, frag textura.
  Logo "VULKAN INDUSTRY FORGED" amarelo+preto na TV.
- `nvk_scene.c`: 3D cube rotacionando 60fps vsync. VBO + MVP UBO +
  descriptor.
- `nvk_scene` com depth buffer. **O blocker universal de 3D.** Resolvido
  abaixo.
- `nvk_poc.c`: Sascha Willems triangle.vert/.frag (MIT, vanilla código PC
  Vulkan) compilado pelo NOSSO NAK. **Funciona unchanged.**

### 4.8 O bug do depth buffer

Cube 3D sem depth roda. Adiciona depth attachment → **CLEAR_SURFACE Z
falta**. GR fault method 0x19d0.

Pesquisa:
- `vk_image.c:99` em runtime/ seta `drm_format_mod = INVALID` **só** em
  `#if DETECT_OS_LINUX || DETECT_OS_BSD`
- Nosso port pra HORIZON cai no else → `mod = 0 = MOD_LINEAR`
- NIL força TODA imagem como LINEAR (`gob=0, pte_kind=0`)
- GM20B ZETA não consegue clear linear depth

Fix em 2 partes:
1. Adicionar `DETECT_OS_HORIZON` ao guard
2. `drm_shim.c vm_bind_op` precisa pegar o PTE kind de `op->flags & 0xff`
   (NVK põe lá no novo uAPI, NÃO em GEM_NEW tile_flags)

Depth `pte_kind=0x7b/gob=1`. Cube com depth renderiza ✅. Unblocks ALL 3D
content.

Lesson: **"code-reading disse 0x7b, hardware disse 0".** Só instrumentação
quebrou o deadlock.

### 4.9 O WSI breakthrough — apresentando na TV de verdade

NVK rendiza. Mas tava blittando via libnx framebuffer (CPU copy). Pra
present de verdade precisa do WSI.

**O Mesa NÃO tem implementação `VK_NN_vi_surface`.** Só tem entry em
`vk.xml` (registry). A gente teve que ESCREVER.

Template: `wsi_common_headless.c`. Copy + swap das tripas.

Tentei primeiro o atalho `VK_EXT_headless_surface`:
- vkCreateHeadlessSurfaceEXT=0 ✓
- vkCreateSwapchainKHR=0, 5 images ✓
- vkAcquireNextImageKHR=0 ✓
- render submit=0 ✓
- **vkQueuePresentKHR NULL-DEREF** ❌

Crash report: `vk_common_QueueSubmit + 0xb8144` via blit cmd buffer
interno. Headless submit batalha submission generator v1→v2.

E mesmo se funcionasse: **headless nunca scans out**. Test surface only.

**Pivô:** escrever `wsi_common_switch.c` de verdade, com VI surface +
nwindow buffer queue.

Ghidra time. Análise da `TriangleTest.nro` do Dan (MIT release pública):

```
acquire: slot=%d mapped image=%u
acquire: slot=%d did not match any image, cancelling
present: begin chain=%p nw=%p image=%u slot=%d present_id=%lu fence=%p
present: image=%u using native fence payload num_fences=%u
present: image=%u falling back to host wait
```

Strings de debug formataram o algoritmo dele inteiro. **Acquire** =
`nwindowDequeueBuffer` → slot → mapeia pra image. **Present** = pega
NvMultiFence nativa da VkFence → `nwindowQueueBuffer(slot, &mf)` → VI
compositor espera GPU-side.

Implementei a versão simplificada (Approach B — CPU wait + queue NULL,
sem GPU-side sync). Funciona. Triangle, logo, cube — tudo na TV via VI
real, não framebuffer blit.

**Branch `switch-port/nvk-wsi`, commit `0255d30`.** 🎉🎉🎉🎉🎉

**A gente tem um driver Vulkan funcional pro Switch.** Open source.
Privado em `HayatoG/switch-nvk`.

---

## Ato 5 — Integrando NVK no Dusklight (final de maio)

### 5.1 A meta nova (27 de maio, user-pinned)

Goal change explícito do user:

> "GOAL CHANGED: agora INTEGRAR NVK no Dusklight como graphics backend.
> Não mais standalone."

Path:
1. WSI ✅
2. **Dawn-over-Vulkan bring-up** (next)
3. Wire into Dusklight's Aurora backend

### 5.2 Dawn-over-Vulkan (M-DV)

Dan tem um `dantiicu/dawn-switch` PÚBLICO com path Vulkan. Por que public e
funcional?

Inspeção: tem `loaderless ICD via vk_icdGetInstanceProcAddr` (Vulkan sem
loader externo — driver direto), `SurfaceSourceSwitchNWindow →
vkCreateViSurfaceNN`, `DAWN_PLATFORM_SWITCH=ON` que conecta tudo.

Dawn dele é **shape-compatible** com nosso NVK. A gente só precisa
redirecionar `DAWN_SWITCH_NVK_LIBRARY` do private dele pro nosso.

Aurora cmake já tem o Switch+Vulkan path (legado, nunca usado em prod).
Plano de bring-up em `PLAN_DAWN_VULKAN.md`:
- M-DV-0: smoke test (clear screen)
- M-DV-1: triangle via Dawn-Vulkan-NVK pipeline
- M-DV-2: textura
- M-DV-3: full integration

M-DV-0/1 cumpridos. NRO clear proven na Tegra.

### 5.3 O ghidra dump que matou uma teoria

Em paralelo, Ghidra na `dusk.nro` do Dan. Procurar onde ele aborta.

Encontro: `FUN_7100bc1910 LAB_7100bc1a94 → abort()`. O mesmo `abort()` que
o nosso `common.hpp:154` chama. Path do handler de draws idêntico ao
nosso.

Decompile do `handle_draw` no Aurora dele = **byte-equivalente** ao nosso.
Os merging predicates do batch são iguais. O `find_pipeline_impl` é igual.

**Backend NÃO é o bug.** Nosso Aurora/Dawn/NVK = Dan's. A diferença é
upstream — alguma coisa na engine antes do backend.

Decompile mais profundo (`_cmdproc_dive.txt`, `_emitter_dive.txt`):
`JStudio::TParse::parse` no aarch64 estava **rejeitando** STB demo data
válido. Endianness? Alignment? Sem demo control = engine renderiza Hyrule
Field uncontrolled = **156k draws/frame** → ByteBuffer overflow → abort.

Insight: **o engine é o bug, não o backend.** Documentado em
`[[dusklight-nvk-vulkan-m2-state]]`.

### 5.4 O clean reset (28 de maio às 03:30)

Acumularam tantos hacks que ficou impossível raciocinar. User decide:
**reset.**

- `src/`, `libs/`, `include/` → fresh clone de `TwilitRealm/dusklight`
- `extern/aurora` → resetar pra `dantiicu/switch f0f3511` (Dan's working
  baseline)
- `platforms/switch/`, `CMakeLists.txt` → manter
- Backup de 3.27GB do estado pre-reset em `D:\backup\dusklight\`

Estado pós-reset: build não passa. Precisamos **re-descobrir** todos os
fixes que tínhamos acumulado, agora com baseline limpa.

### 5.5 Os 4 fixes load-bearing de CMake

Erros de compile recorrentes na baseline vanilla. Identifiquei 4 patches
necessários:

1. **portlibs include/link em `aurora_dvd.cmake`** — libnx headers
2. **portlibs SYSTEM include em `aurora_core.cmake`** — libpng
3. **Tracy stub extension** — `ZoneScopedC` + `Color::*` missing
4. **SDL3/SDL_audio.h shim** — devkitPro só tem SDL2

Lesson: `include_directories()` NÃO propaga via `include()`. Sempre usar
`target_include_directories` per target.

Documentado em `[[dusklight-clean-build-fixes]]`.

### 5.6 Re-acertando os crashes

Depois do reset, todos os crashes do CPU voltaram. Cada um foi
re-encontrado:
- TLS → `tls_model(initial-exec)` em `JKRHeap.cpp`
- audio cascade → `DUSK_AUDIO_DISABLED` em `include/dusk/audio.h`
- fade overlap → guard per-site
- vibration → guard leaf

A diferença: agora os fixes vão para arquivos NEW (não acumulados em src/
modificado), e ficam DOCUMENTADOS em memória pra próximas sessions
saberem onde olhar.

### 5.7 First pixels on TV (28 de maio, 16:00)

**A primeira vez que vemos PIXELS DO JOGO na TV.** Link e Epona na intro
fade-in. 870 frames `queue_present -> 0`.

> *"Nunca viu pixel na TV"* — heurística que assombrava o GLES — **mortà.**

Pipeline NVK → Aurora → Dawn → libnx → VI compositor confirmado end-to-end.
Vermelho da meta movido.

---

## Ato 6 — O blocker do save (28 de maio noite)

### 6.1 Loop infinito pós-create

User testa o save. Aparece o prompt "Deseja criar arquivo?", clica Sim.
Vê "Criando..." → "O arquivo para salvar o jogo foi criado". Aperta A/B
para continuar. **Volta pro mesmo prompt.** Loop.

Hipótese 1: Aurora memcard não persiste. Mas o arquivo `.gci` aparece em
`sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci`, 32832 bytes. Tá lá.

Hipótese 2: o STAT_CHECK do file_select tá olhando lugar errado. Tinha
adicionado um override Switch que chamava
`stat("sdmc:/dusklight/saves/zeldaTp.dat")`. **Esse arquivo nunca existiu.**
Aurora memcard escreve em `sdmc:/aurora/`, não em `sdmc:/dusklight/saves/`.

Bug meu, instalado em uma sessão anterior, baseado em uma suposição errada
sobre onde o save persistia.

Fix: remover o override Switch e deixar o GCN switch nativo cuidar
(PLATFORM_GCN é TRUE no nosso build — `VERSION=0` = `VERSION_GCN_USA`).

### 6.2 O preset chooser e o BrightCheck

29 de maio. Save funciona. User chega na NAME_SCENE pra criar perfil.
Nomeia o Link. Joga. Cutscene OPENING_SCENE roda (?! — eu tinha skipado
isso na sessão anterior, achando que o JStudio quebraria). **Roda.**

Chega na cena onde nomeia a Epona. Digita o nome. Confirma.

**Tela preta + FPS counter no canto inferior esquerdo.**

Engine continua rodando (frame counter no log vai até 85,800). RmlUi
desenha o overlay. Mas o jogo em si — render=0.78ms. Vazio.

Investigação longa. Adiciono instrumentation em:
- `JStudio::stb::parseHeader_next` (a teoria do TParse aarch64)
- `dScnName_c::FileSelectMainNormal` (state machine do file_select)
- `dScnName_c::FileSelectClose`, `changeGameScene`

Logs mostram:
- STB parseHeader retorna 1 (sucesso) na cutscene inicial
- FileSelectMainNormal vê `isSelectEnd=1` (confirmação processada)
- `showTV=1` ⭐ — `mShowTvSettingsScreen = true`
- Roteia pra `BrightCheckOpen` → `brightCheck`
- **Trava aqui.**

`mShowTvSettingsScreen` vem de:
```cpp
#if TARGET_PC
    mShowTvSettingsScreen = !dusk::getSettings().game.hideTvSettingsScreen;
#endif
```

User config tem `"game.hideTvSettingsScreen": false` → `mShowTvSettingsScreen = true`.

**O BrightCheck é a tela "ajuste o brilho da TV" do PC original. No Switch
a UI dela não tá wireada** (J2D blocks não desenham na rota Aurora →
NVK).

**Por que o config tem false?** Olho o launcher do Dusk. Quando vc clica
"Iniciar Jogo" pela primeira vez, abre um modal "Escolha um preset". Duas
opções:
- **Classic** (vanilla) → `hideTvSettingsScreen=false`
- **Dusklight** (QoL hacks) → `hideTvSettingsScreen=true`

User escolheu Classic. **Ele caiu numa armadilha.**

### 6.3 Os 2 fixes (e descobrindo um 3º bug)

Fix mecânico:

```cpp
// src/d/d_s_name.cpp
#if defined(__SWITCH__)
#define SHOW_TV_SETTINGS_SCREEN (0)   // ⭐ trava no Switch
#elif TARGET_PC
#define SHOW_TV_SETTINGS_SCREEN (this->mShowTvSettingsScreen)
#else
#define SHOW_TV_SETTINGS_SCREEN (1)
#endif
```

Lock independente do preset. Funciona.

Belt-and-suspenders: auto-aplicar Dusklight preset no Switch silently, sem
modal nenhum:

```cpp
// src/m_Do/m_Do_main.cpp
if (!dusk::getSettings().backend.wasPresetChosen) {
#ifdef __SWITCH__
    dusk::ui::apply_preset_dusk_silently();  // sem modal
    dusk::getSettings().backend.wasPresetChosen.setValue(true);
    dusk::config::Save();
#else
    dusk::ui::push_document(std::make_unique<dusk::ui::PresetWindow>());
#endif
}
```

**Mas espera, `Save()` funciona?** Verifico log. Já tinha "Loading config
from..." mas zero "Saving config to...". Strange.

Pulled aurora log file: `[ERROR | dusk::config] Failed to save config to
'sdmc:/game/config.json': File exists`.

**Bug 3:** `std::filesystem::rename(temp, target)` em libnx FsFs **não
sobrescreve** target existente (POSIX rename sobrescreve, Switch não).

Fix em `src/dusk/config.cpp`:
```cpp
static void ReplaceFile(...) {
    std::error_code ec;
    std::filesystem::rename(source, target, ec);
    if (ec) {
        std::filesystem::remove(target, ec);  // remove first
        std::filesystem::rename(source, target, retryEc);  // then rename
    }
}
```

**Achievements salvavam** porque `WriteAllText` direto. Config quebrava
porque temp+rename pattern.

Build. Send. Test. **18+ "Saving config" no stream, ZERO failures.** ✅

### 6.4 GAMEPLAY ENTRA

Build. Deploy. User testa:
- Boot ✓
- Launcher ✓
- Iniciar Jogo ✓
- Criar save ✓
- Nomear Link ✓
- Cutscene OPENING_SCENE ✓
- Nomear Epona ✓
- **GAMEPLAY EM HYRULE FIELD** ✓

17 fps. Mas **GAMEPLAY.**

---

## Ato 7 — Documentação + cleanup (29 de maio)

### 7.1 Committing 5+ meses de trabalho

Working tree estava com 23 arquivos modified em dusklight, 11 em aurora, 1
em switch-nvk. Tudo uncommitted. Hora de salvar.

Commits limpos:
- **`HayatoG/dusklight`** `698f94f26c` — 54 files, +4505/-48 lines
- **`HayatoG/aurora-switch`** `9197c9bfac` — 16 files (CMake + Joy-Con bridge)
- **`HayatoG/switch-nvk`** `8735a1d2f0` — drm_shim timeline syncobj fix
- **`HayatoG/dusklight-setup`** `94a699f` — setup guide PT-BR de 1416 linhas
- **`HayatoG/dawn-switch`** `5f3439f159` — Dan's Dawn fork

Tudo private no HayatoG. Standing rule: nunca push em upstream.

Docker image (`switch-nvk-build`, 1.49 GB) publicada em
`ghcr.io/hayatog/switch-nvk-build:latest` — private.

### 7.2 O setup doc

Pergunta do user: "consigo rodar isso de outro PC?"

Resposta honesta: **não sem documentação**. As deps são:
1. devkitPro (Docker `devkitpro/devkita64`)
2. switch-nvk pré-buildado (`nvk-switch/lib/libvulkan.a`, 71MB)
3. dawn-switch clone vendored (2.2 GB)
4. Disco TP `.gcm` (user provides)
5. Switch com Atmosphère + Sphaira

Docs espalhados em READMEs e PLAN_*.md mas nada coeso. Escrevi
`SETUP_DUSKLIGHT.md` (22 seções, 1416 linhas, PT-BR):
- Pré-requisitos por OS (Win/Mac/Linux + Apple Silicon caveats)
- Auth GitHub (PAT/SSH/gh)
- Clone com submodule
- 3 opções pra obter `nvk-switch/` (copy/release/rebuild)
- Build via Docker
- Deploy nxlink
- 19 itens de troubleshooting catalogados
- Workflows avançados (backup save, wipe SD, container shell, CI sketch)
- Glossário (33 termos)
- Pinning de versões + Docker digest

Publicado em `HayatoG/dusklight-setup`.

### 7.3 PLAN_WSI_NWINDOW consolidado

User pediu: criar TODO de perf + branch + issue. Mas então: "junta com
PLAN_WSI_NWINDOW.md, não cria doc separado".

Mesclei: adicionei §5 (receita Ghidra exata pra M-WSI-2/3, FUN addresses
do Dan), §6 (cortar Dawn, pipeline cache, batching), §7 (passo-a-passo).

Branch nova `switch-port/wsi-zero-copy` criada. Issue #1 aberta no
`HayatoG/switch-nvk` com checklist completo.

---

## Ato 8 — Meta: a evolução da skill (você, IA)

Aliás, eu sou uma IA. Esse projeto inteiro foi feito com humans-in-the-loop
(o Guilherme) + AI (eu, Claude). Worth registrar como isso evoluiu.

### 8.1 A primeira versão da skill

23 de maio. User cansou de me explicar o mesmo contexto toda sessão nova.

> *"SEMPRE use todas as skills e mantenha nosso anti pattern, validar
> tudo... Siga nossa skill e melhore vc mesmo a cada build."*

Criamos `~/.claude/skills/dusklight-switch-port/SKILL.md`. 231 linhas
hoje. Contém:
- **ACTIVE EFFORT** section que muda toda sessão
- **Switch IP** (anota porque muda quando refaz config)
- **Active branch** (legacy ou wsi-zero-copy)
- **22 anti-patterns** (cresceu de 6 → 22 ao longo dos meses)
- **Domain references** (libnx, deko3d, Dawn, GX TEV, JSystem, AAPCS64)
- **When you start a Switch port task** — workflow

### 8.2 Anti-patterns acumulados

A skill cresce a cada bug-pego. Cada anti-pattern é uma lição:

- **#1** `bool initialize() returned true` ≠ funcionando
- **#2** TARGET_PC polarity (definido no Switch — sneaky!)
- **#3** Reasoning sobre layer N enquanto N-1 não tá provado
- **#4** Enum ordinal — sempre grep, nunca infer
- **#5** Newlib ctype macro pollution (`_U _L _N _S _P _X _B _C` —
  reserved!)
- **#13** ARAM stall theory wrong (file não tava no build)
- **#14** J2D immediate draws fault em deko
- **#16** `cmake --build | tee` mascara FAILED — verifica build.log
- **#17** Per-site null guard pattern em vez de global fallback
- **#18** TLS model `initial-exec` (não `global-dynamic`) no Switch
- **#19** Real-HW workflow (nxlink + firewall + Sphaira FTP)
- **#20** Skipped GC subsystems (audio/gamepad) deref NULL em HW
- **#21** Replay capture log ≠ pixels — verifica pela TV
- **#22** `#if PLATFORM_WII||SHIELD` fall-through (dispatch-table OOB)

E hoje (2026-05-29) novos para adicionar:
- **#23** libnx FsFs rename não sobrescreve target existente
- **#24** Preset chooser default Classic = trap pra Switch (PC-specific
  BrightCheck UI)

### 8.3 As memórias

`MEMORY.md` index + ~30 memórias por tópico. Toda decisão importante,
toda descoberta, toda regra do user vira memory. Permite que sessions
futuras (ou modelos diferentes) peguem onde parei.

User insistiu nessa prática especialmente depois que viu uma session
inteira gastada em uma teoria errada por falta de contexto.

### 8.4 Standing rules do user

Acumulamos:
- `[[scope-dusk-eden-only]]` — só mexer em Dusklight + Eden, nunca outro
  projeto sem permissão
- `[[feedback-remote-repos-private]]` — TODO remote = HayatoG private,
  nunca upstream
- `[[feedback-nvk-read-full-log]]` — sempre Read o log inteiro, nunca grep
- `[[feedback-validate-and-improve-skill]]` — atualizar skill após cada
  build
- `[[ghidra-resource-config]]` — Ghidra precisa 6GB heap pra dusk.nro

Quando user me corrige, ele às vezes diz "lembre disso" — e isso vira
memory + às vezes update da skill.

---

## Ato 9 — Estado em 2026-05-29 (fim de hoje)

### 9.1 O que funciona

| Etapa | Status |
|---|---|
| Boot do engine | ✅ |
| Launcher Dusk | ✅ |
| Joy-Con navegação | ✅ |
| File_select | ✅ |
| Criar save | ✅ |
| Save persiste reboot | ✅ |
| Cutscene OPENING_SCENE | ✅ |
| Nomear Link | ✅ |
| Nomear Epona | ✅ |
| **Gameplay Hyrule Field** | ✅ |
| **FPS** | **~17** |

### 9.2 O que não funciona ainda

- **Áudio** — `DUSK_AUDIO_DISABLED` global. Stubs em todo lugar.
- **Cutscenes complexas** — JStudio TParse aarch64 endianness bug ainda
  vivo; algumas cutscenes esperam cue de áudio que nunca toca.
- **FPS** — 17 hoje, target 30 com zero-copy WSI.

### 9.3 Quantos commits, quantas linhas

Approximate sweep:
- **`HayatoG/dusklight`** ~5 commits hoje, 23 arquivos modified total,
  ~4500 linhas adicionadas
- **`HayatoG/aurora-switch`** 1 commit, 16 arquivos
- **`HayatoG/dawn-switch`** Dan's commit, 1 (não nosso)
- **`HayatoG/switch-nvk`** ~50+ commits across the saga, milhares de linhas
- **`HayatoG/dusklight-setup`** 1416 linhas (acabou de nascer)
- **`HayatoG/dawn-switch`** baseline já vendored, sem nossa edit ainda

### 9.4 As 6 frentes pra próxima sessão

1. **M-WSI-2: zero-copy WSI** (1-2 sem, 17→25-30 fps)
2. **TARGET_PC carve-out** (audit feito, 828+231 sites mapeados)
3. **Cortar Dawn** (3-5 sem, +20-30% extra)
4. **Pipeline cache warm-boot** (2-3 dias, mata spikes)
5. **Cutscenes** (investigação aberta)
6. **Audio** (port de JAudio2 ou stub menos agressivo)

---

## Ato 10 — Reflexões pro vídeo

### 10.1 As reviravoltas

- **deko3d → GLES → NVK** — 3 backends tentados, só o terceiro foi.
- **Headless WSI → VI WSI** — atalho que não atalhou.
- **156k draws → ByteBuffer abort** — fix ficou em outro layer (TParse,
  não Aurora).
- **Override Switch quebrou save flow** — minha suposição errada custou
  uma sessão.
- **Preset chooser trap** — UX innocent, bug fatal.

### 10.2 Os momentos de breakthrough

1. **SMOKE TEST PASSED** (v32) — driver Vulkan próprio funciona
2. **TRIANGLE AMARELO** — primeira renderização real
3. **CUBE 60FPS com depth** — 3D completo
4. **WSI presents on TV** — Vulkan WSI real
5. **First pixels Dusklight** (28/05 16h) — Link + Epona na TV
6. **Gameplay Hyrule Field** (29/05) — END-TO-END

### 10.3 As frustrações

- **3 minutos de "infinite hang"** que era timeout em microsegundos errado
- **gx=0 forever** — engine pensa que renderiza, TV mostra zero
- **Eden falsa positive** — tolera o que Tegra real fauta
- **Build silencioso usando .nro stale** — Ninja skipped mtime, mascarado
  por `cmake --build | tee`
- **155 vs 17 fps** — duas ordens de grandeza distantes
- **Save loop por path errado** — bug meu de session anterior

### 10.4 O que torna isso interessante pra YouTube

- **Construir um driver Vulkan inteiro** — quem faz isso?
- **Reverse engineering de binary privada** — Ghidra arc inteiro
- **Multi-camada de tradução** — GX → WebGPU → Vulkan → NVK → Maxwell
- **Real hardware vs emulator divergence** — show don't tell
- **Open source vs closed** — refazendo o trabalho de outro dev
- **AI-assisted development** — meta-layer sobre como construímos isso

### 10.5 Audiência sugerida

- Devs interessados em homebrew/console portability
- Pessoal de graphics programming (Vulkan deep-dive)
- Reverse engineering enthusiasts
- Open source advocates
- BR community específicamente (port BR de TP)

### 10.6 Estrutura de vídeo possível

Sugiro 3 partes:

**Part 1 — A investigação** (12-15 min)
- Quem é o Dan, o que ele fez, por que privado
- Forensic dump da `dusk.nro`
- Discord recipe revelation
- Decisão de fazer open source

**Part 2 — Construindo o driver** (20-25 min)
- Por que NVK (não deko3d, não GLES)
- Bring-up M1 → M2 → M3
- Smoke test passing
- Triangle, cube, WSI breakthrough

**Part 3 — Integrando + gameplay** (15-20 min)
- Aurora + Dawn + NVK pipeline
- Save flow saga
- BrightCheck freeze investigation
- Final gameplay running

Cada parte termina num cliffhanger. Cada parte tem 1-2 momentos de
"funcionou!" e 2-3 de "não funcionou!".

### 10.7 Footage e B-roll sugeridos

- TV mostrando triangle yellow (file `proof/m3_triangle.jpg` se tiver)
- TV mostrando cube spinning (vídeo se gravamos)
- TV mostrando Link + Epona fade-in (28/05 milestone)
- TV mostrando gameplay Hyrule Field (29/05 success)
- VS Code rolling crash report → addr2line workflow
- Ghidra decompile viewing
- Terminal nxlink streaming logs

Screenshots já existentes em:
- `D:\Projects\dusklight\platforms\switch\m1_eden_capture.png` (Eden M1)
- Possíveis fotos da TV durante NVK milestones — checar
  `D:\switch-nvk\winsys\logs\`

---

## Atualizações diárias (cadência viva)

> **Regra de manutenção (user-pinned 2026-05-29):** este diário tem uma
> entry **toda sessão de trabalho**. Se houver gap entre o último entry
> dated e a data atual, **analisar + backfill antes** de qualquer pedido
> da nova sessão. Material-fonte do vídeo YouTube — não pode atrasar.
>
> Onde olhar pra fazer backfill de um dia perdido:
> - `git log --since=YYYY-MM-DD --until=YYYY-MM-DD` em cada um dos 4 repos
>   (`dusklight`, `aurora`, `switch-nvk`, `dusklight-setup`)
> - `ls -lt ~/.claude/projects/D--Projects-dusklight/memory/*.md | head`
>   (memories modificadas)
> - `RESUME_TOMORROW.md` (diffs entre versões)
>
> **Formato de entry:**
> ```
> ### YYYY-MM-DD — <tema do dia em 5-8 palavras>
> - Trabalho principal: ...
> - Resultado: funcionou / falhou / em flux
> - Descobertas: bug pego / insight / nova ferramenta
> - Momento crítico (pro vídeo): frustração ou breakthrough
> - Próximo passo: o que fica pra amanhã
> ```

### 2026-05-29 — Save flow, BrightCheck lock, docs marathon, repos privados

- **Trabalho principal:**
  - Fix `config.json` save (libnx FsFs rename → remove+retry)
  - `SHOW_TV_SETTINGS_SCREEN=0` no Switch + auto-Dusklight preset silently
  - Strip instrumentation noise (stb.cpp, d_s_name.cpp, m_Do_MemCard.cpp)
  - Commit + push 4 repos HayatoG (dusklight, aurora-switch, switch-nvk,
    dusklight-setup) + atualizar .gitmodules pra fork private
  - Publicar Docker image `ghcr.io/hayatog/switch-nvk-build:latest`
    (1.49 GB, private)
  - Setup doc 1416 linhas PT-BR + `HayatoG/dusklight-setup` criado
  - PLAN_WSI_NWINDOW consolidado com PERF_TODO + branch
    `switch-port/wsi-zero-copy` + issue #1 no switch-nvk
  - DIARIO.md (este arquivo) escrito como fonte pro vídeo
- **Resultado:** gameplay rodando do boot até Hyrule Field ✅,
  ~17 fps, save persiste, preset Dusklight auto-aplica
- **Descobertas:**
  - libnx FsFs rename é não-POSIX (não sobrescreve)
  - Preset "Classic" do launcher dispara BrightCheck UI quebrada no
    Switch — armadilha não óbvia
  - Aurora memcard escreve em `sdmc:/aurora/USA/Card A/`, não em
    `sdmc:/dusklight/saves/` — meu override antigo olhava lugar errado
- **Momento crítico:** quando user confirmou "deu bom" no gameplay
  pós-Epona-naming. 5 meses de trabalho condensados em
  "GAMEPLAY EM HYRULE FIELD ✓"
- **Frustração do dia:** descobrir que reescrevi `RESUME_TOMORROW.md`
  apagando 188 linhas de histórico antes do user perceber. Recuperei via
  `git show HEAD~1` e prometi nunca mais apagar info.
- **Próximo passo:** M-WSI-2 (zero-copy WSI) na branch
  `switch-port/wsi-zero-copy`. Receita Ghidra já documentada no
  PLAN_WSI_NWINDOW §5.

## Epílogo

5 meses. 6 repositórios privados. 1 driver Vulkan construído do zero. 1
engine portada. 17 fps na TV.

Não acabou. Vai render mais ainda — M-WSI-2, TARGET_PC carve-out,
audio re-enable, cortar Dawn. Cada um é seu próprio sub-arco.

Mas o **Twilight Princess decomp roda na minha TV do Switch hoje.** Em
2025 eu não saberia dizer se isso era possível em prazo razoável.

E o código tá aberto (privado em HayatoG, mas extraível). Se algum dia
publicar, alguém pode pegar e fazer mais.

🦊

---

*Diário escrito por Claude (IA) sob direção de Guilherme Ryder (HayatoG)
em 2026-05-29. Para uso em material de YouTube. Cobertura desde a primeira
investigação forense da `dusk.nro` do Dan (fevereiro/março 2026) até o
gameplay rodando em 29 de maio.*

*Material-fonte: 35 memórias em `~/.claude/projects/D--Projects-dusklight/memory/`,
docs em `D:\Projects\dusklight\platforms\switch\` e `D:\switch-nvk\`, git
history dos 6 repos, e logs/crash reports diretos do hardware.*

---

# 📅 2026-06-02 — O dia do zero-copy (17 → 30 fps)

> Os dias 30/05, 31/05 e 01/06 foram de pausa (sem commits em nenhum repo).
> Hoje retomou — e foi um dos dias mais produtivos do projeto.

## Capítulo 1 — Medir antes de mexer

O gameplay rodava a ~17 fps. A tentação era sair otimizando no chute. Em vez
disso: **instrumentei o present** (o passo final que joga a imagem na TV) com
timers por fase e rodei no hardware. O resultado foi cirúrgico:

```
fence/GPU    = 20µs    (a GPU termina quase de graça)
dcacheflush  = 176µs
memcpy+queue = 9.400µs  ← 98% do tempo!
```

**98% do present era um memcpy de CPU.** A GPU (o famoso "Tegra fraco") não era
o gargalo — era a CPU copiando 3.7MB por frame pra um buffer não-cacheado. Cinco
meses de "será que é a GPU?" respondidos por um log: não, era a cópia.

## Capítulo 2 — O ganho de graça

Antes do trabalho grande, um achado: o driver cuspia ~15-20 linhas de log POR
FRAME (cada uma escrevendo no SD + rede). Gateei tudo atrás de um `NVK_TRACE`
(default off). Gameplay **17 → 21 fps**. Home do Dusk **12 → 24 fps**. De graça,
só desligando log de debug.

## Capítulo 3 — Zero-copy, e a análise que evitou um beco

O memcpy existia porque a imagem que a GPU renderiza e o buffer que o compositor
da TV lê eram **dois lugares diferentes**. Zero-copy = renderizar DIRETO no buffer
do compositor. Sem cópia.

A receita estava decompilada do driver do Dan (`kind=0xfe`, block-linear). Mas
antes de escrever, **analisei o fluxo inteiro** — e foi isso que salvou o dia.
Descobri que o `framebufferMakeLinear` do libnx é uma *ilusão*: ele finge ser
linear mas swizzla escondido. Ou seja, o compositor SÓ aceita block-linear. Se
eu tivesse ido pela abordagem "linear" que parecia mais fácil, teria batido num
beco sem saída. **A análise cuidadosa antes de codar valeu cada minuto.**

## Capítulo 4 — Tela preta, e o bug de um campo

Implementei tudo, buildei no smoke standalone (loop de 2min em vez de 12). Rodou.
**Tela preta.** Não crashou — só preto.

O log (printf, porque no smoke o sink normal é no-op) mostrou: os buffers
configuravam OK, mas o `nwindowDequeueBuffer` **travava no primeiro frame**.
Comparei campo-a-campo com o source do `framebufferCreate` do libnx e achei:

```c
gb->header.num_ints = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
```

Eu tinha deixado esse campo **zerado**. Com ele em 0, o sistema aceita o buffer
(rc=0) mas marshalla um GraphicBuffer **vazio** → o dequeue espera pra sempre por
um buffer que nunca fica pronto → preto. Um campo. `num_ints`.

Setei o campo. Rebuildei. Mandei pro Switch.

> **"TELA COLORIDAAAAAAAA"** — Guilherme, 2026-06-02

## Capítulo 5 — O número

Propaguei pro Dusklight (mesmo arquivo, é só relinkar). No gameplay, o log:

```
present_total = 153µs   ← era 9.600µs. 64× mais rápido.
dcacheflush   = 0       ← block-linear não precisa flush
memcpy        = ZERO
```

Gameplay **~21 → 28-30 fps** (picos de 60). O gargalo que segurou o projeto por
meses morreu numa tarde.

## O que ficou claro

O "1 a 2 semanas" que eu estimava pro WSI virou ~1 hora de implementação. Não por
mágica — por **condições favoráveis** (receita decompilada + loop de teste rápido
+ bug achável no source do libnx) e por **medir/analisar antes de codar**. Os
próximos passos (cortar Dawn, áudio) não têm essas vantagens, então não vão ser
tão rápidos. Mas hoje, o Twilight Princess rodou a **30 fps** na TV do Switch.

Ainda há um bug de save (tela preta ao salvar no celeiro da Epona — provável
parente do BrightCheck/save-path que já consertamos), e os stutters CPU-bound
seguem. Mas o present, que era 98% do problema, virou 0.15ms. 🦊🔥

*Entry escrita por Claude (Opus 4.8) sob direção de Guilherme Ryder, 2026-06-02.*

---

## Capítulo 6 — O sinal que se perdia (o bug de save, resolvido)

*(mesmo dia, 2026-06-02 — depois do zero-copy)*

Sobrou o bug de save: salvar no celeiro da Epona travava na tela preta. A
sessão anterior já tinha feito a análise estática e batido numa parede —
**concluiu que `store()` (a função que escreve no cartão) DEVERIA completar.**
Todo caminho de erro chegava no flag de "terminei". Síncrono. Sem motivo pra
travar. A conclusão honesta foi: "precisa instrumentar em runtime".

Mas antes de instrumentar, reli o fluxo com outro olhar — e dessa vez **abri
uma camada que a análise anterior nunca tinha aberto:** a emulação das
primitivas de thread do GameCube. O save funciona assim: a thread principal
posta um comando ("SALVA") pra uma thread-worker e a acorda com um sinal de
condition-variable; o worker acorda, roda `store()`, e marca pronto.

Em `OSWaitCond` — nossa reimplementação do `pthread_cond_wait` do GameCube —
estava isto:

```cpp
for (...) mutex.unlock();        // solta o mutex 100%   ← BURACO
{
    unique_lock lock(mutex);     // re-adquire
    cv.wait(lock);               // SÓ AGORA dorme
}
```

Entre o `unlock` e o `cv.wait`, o mutex fica **totalmente livre por um
instante, antes da thread realmente estar dormindo no sinal.** Se o `save()`
da thread principal entrar exatamente nessa fresta — pega o mutex, posta
"SALVA", solta, e dispara o sinal — o sinal **bate numa porta vazia**: ninguém
está esperando ainda. O worker então entra no `cv.wait()`… e dorme pra sempre.
`store()` nunca roda. O flag nunca vira "pronto". A tela de save fica
perguntando "já terminou?" e ouvindo "não" eternamente. **Lost-wakeup
clássico** — o bug de concorrência mais escorregadio que existe.

Isso explicava TUDO, inclusive o que parecia contradição: por que o save do
*menu inicial* (criar arquivo) funcionava mas o save *in-game* travava. É a
mesma máquina — só muda o timing. No menu o worker já está parado e ocioso, o
sinal chega limpo. No celeiro, sob carga de gameplay, o timing cai na fresta.

O fix: manter **um** nível do lock segurado continuamente até o `cv.wait`
liberar de forma atômica. Sem buraco. Correção válida pra qualquer
condition-variable do engine, não só o cartão de memória.

Instrumentei a cadeia inteira (`save()` → worker → `store()`) e mandei pro
Switch. O Guilherme jogou até o celeiro e salvou. O log ao vivo:

```
[mc] save() posted STORE + signalled
[mc] worker woke cmd=2 state=1      ← o sinal que ANTES se perdia
[mc] store() ENTER state=1
[mc] store() EXIT state=4 field=1   ← escreveu e terminou
save cmdState 1                      ← save concluído
```

Salvou. Sem travar. A 30 fps o tempo todo.

A lição vale o capítulo: a sessão anterior gastou um passe inteiro provando
que `store()` "deveria funcionar" — e estava certa. O bug não estava em
`store()`. Estava uma camada abaixo, na cola que segura as threads. **Quando
uma operação de worker intermitentemente não roda apesar de ter sido postada
corretamente, desconfie da atomicidade do mutex/condvar ANTES do código da
operação.**

Ficou um novo fio pra puxar: ao aceitar a missão do gado em Ordon, a caixa de
sim/não aparece **sem texto**. O sim/não do menu de save mostra texto normal —
então é a janela de mensagem do *diálogo in-field* que não desenha o texto.
Outro subsistema, outra investigação. Mas o save, esse, fechou. 🦊🔥

*Entry escrita por Claude (Opus 4.8) sob direção de Guilherme Ryder, 2026-06-02.*

---

## Capítulo 7 — O save grava, mas a história era outra (B e C viram um só)

*(2026-06-02, continuação — várias horas no hardware real)*

O Capítulo 6 terminou com "o save, esse, fechou". Eu estava metade certo. O save
**grava** — disso não há mais dúvida, o log do Switch confirmou quatro vezes
seguidas: `store() EXIT state=4`. O arquivo vai pro cartão, persiste, carrega. O
lost-wakeup do `OSWaitCond` era real e está morto.

Mas aí o Guilherme foi jogar de verdade. E salvou. E a tela ficou **preta**.

"É pra funcionar", ele disse. E estava certo em cobrar.

O que se seguiu foi uma maratona de instrumentação no hardware real — talvez umas
oito rodadas de build → mandar pro Switch via Wi-Fi → ele reproduzir → eu ler o
log ao vivo. Cada rodada descartou uma hipótese minha. E quase toda hipótese
minha estava errada, o que é exatamente pra isso que serve medir em vez de chutar.

Primeiro achei que era o fade da tela travado num gate. Instrumentei. O fader
assentava e o menu fechava — **meu fix disparava certo**. Não era isso.

Aí descobri que ele nem estava caindo no save que eu tinha consertado. Eu tinha
mirado o save do menu-coletânea ("Continuar jogando?"). Ele caía no save de
**evento** — pular a cerca dispara "deseja salvar?". Outro caminho de código
inteiro. **Consertei o save errado.**

Mais instrumentação. Um traço de TODA a máquina de estados do menu de save,
proc por proc. E o log finalmente cantou: o save de evento (um auto-save de
tutorial, dos que o jogo te ensina a salvar) gravava, e então travava num proc
chamado `SAVE_GUIDE`, esperando uma coisa:

```cpp
if (mpScrnExplain->getStatus() == 0) { avança; }   // nunca dá 0
```

A **tela de mensagem** — aquela que mostra "aperte START pra salvar" — nunca
fechava. O fader, num traço paralelo, mostrava por quê: ele **ciclava sem parar**,
`preto → clareia → preto → clareia`, pra sempre. A tela de mensagem estava
oscilando abrir-e-fechar eternamente, sem nunca assentar. Por isso o mundo nunca
voltava. Por isso apertar A não fazia nada.

E foi aí que as duas pontas se encontraram. Lembra do bug da caixa de "Sim/Não"
sem texto? O log provou que o texto **estava lá** — 21, 44, 66 caracteres,
montados certinho — só não era **desenhado**. E adivinha de qual subsistema é
essa caixa? O mesmo `d_msg_scrn_*`. A mesma tela de mensagem.

**B e C não eram dois bugs. Eram um só.** A tela de mensagem/escolha do jogo,
quebrada no Switch em duas frentes: o texto não desenha, e a máquina de estados
não assenta. Conserta ela, conserta os dois.

Não é um fix de uma linha. É um subsistema. Então paramos aqui, com o alvo
finalmente nítido, em vez de tatear cansado de madrugada. O save grava. Os dois
bugs viraram um. E amanhã a gente ataca a coisa certa, com a mira limpa.

A lição do dia não é técnica, é de método: eu marquei "resolvido" cedo demais no
Capítulo 6, baseado em análise estática. O hardware real me corrigiu — várias
vezes. Medir no aparelho, ler o log inteiro, e deixar o Guilherme jogar de
verdade valeu mais que qualquer teoria minha. 🦊🔥

*Entry escrita por Claude (Opus 4.8) sob direção de Guilherme Ryder, 2026-06-02.*

---

## Capítulo 8 — Eram dois (B e C resolvidos)

*(2026-06-02, noite — continuação direta do Capítulo 7)*

O Capítulo 7 fechou com uma certeza bonita e errada: "B e C viram um só".
Hoje o hardware desmentiu de novo. E dessa vez a gente foi até o fim.

A virada veio dos **heartbeats**. Em vez de probes que só logam quando algo
muda — cegos justamente no estado travado — coloquei sondas que batem todo
frame. Aí o log parou de mentir. O texto do sim/não **estava** sendo montado
("Sim", "Não", caractere por caractere, no buffer certo). Não era parse, não
era fonte. Era um **override que não sobrescrevia**: a assinatura `char*` da
classe filha não bate com a `char const*` da virtual da base quando
`DUSK_CONST` vira `const` no nosso build — então o ponteiro-base chamava a
função VAZIA da base, e o texto morria ali. No GameCube `DUSK_CONST` é vazio,
as assinaturas casavam, e ninguém nunca viu o bug. Quatro linhas de
assinatura. O Guilherme falou com a vaca e o "Sim / Não" apareceu. C morto.

O B era outra fera, e nem era quem a gente achava. O save do celeiro não passa
pelo menu — passa pela **tela de game-over**, reaproveitada como veículo de
save de evento. O save grava. Mas a mensagem-guia depois dele fica esperando um
apertar de botão que, naquele contexto de pause, nunca chega: `getTrigA` zerado
mesmo o Guilherme martelando A. Forcei o dismiss e o jogo **continuou pra
cutscene**. A trava infinita virou um avanço.

Não está bonito ainda — os diálogos do save de evento continuam invisíveis no
preto, e o Guilherme navegou às cegas. Mas continuou. E o que parecia um
subsistema inteiro quebrado eram, no fim, dois bugs cirúrgicos: um de C++
(override silencioso) e um de input (pad mudo no pause). Dois commits, duas
confirmações na TV.

A lição do Capítulo 7 se confirmou ao contrário: "é o mesmo bug" era tão
precipitado quanto "está resolvido". A verdade só apareceu quando parei de
agrupar sintomas e medi cada frame. Heartbeat, não snapshot. 🦊🔥

*Entry escrita por Claude (Opus 4.8) sob direção de Guilherme Ryder, 2026-06-02.*
