// OSMutex.cpp - PC implementation of GameCube OSMutex/OSCond API
// Uses std::recursive_mutex and std::condition_variable_any behind the
// unchanged GameCube C API. The OSMutex struct layout is preserved so
// game code can read its fields.

#include <dolphin/dolphin.h>
#include <dolphin/os.h>

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <cstdlib>

#include "JSystem/JKernel/JKRHeap.h"

// ============================================================================
// Side-table: native mutex per OSMutex
// ============================================================================

struct PCMutexData {
    std::recursive_mutex nativeMutex;
};

// Lazy-initialized to avoid DLL static init crashes
static std::mutex& GetMutexMapMutex() {
    static std::mutex mtx;
    return mtx;
}
static std::unordered_map<OSMutex*, std::unique_ptr<PCMutexData>>& GetMutexMap() {
    static std::unordered_map<OSMutex*, std::unique_ptr<PCMutexData>> map;
    return map;
}

static PCMutexData& GetMutexData(OSMutex* mutex) {
    std::lock_guard<std::mutex> lock(GetMutexMapMutex());
    auto& map = GetMutexMap();
    auto it = map.find(mutex);
    if (it == map.end()) {
        auto result = map.emplace(mutex, std::make_unique<PCMutexData>());
        return *result.first->second;
    }
    return *it->second;
}

// ============================================================================
// Side-table: native condition variable per OSCond
// ============================================================================

struct PCCondData {
    std::condition_variable_any cv;
};

// Lazy-initialized to avoid DLL static init crashes
static std::mutex& GetCondMapMutex() {
    static std::mutex mtx;
    return mtx;
}
static std::unordered_map<OSCond*, std::unique_ptr<PCCondData>>& GetCondMap() {
    static std::unordered_map<OSCond*, std::unique_ptr<PCCondData>> map;
    return map;
}

static PCCondData& GetCondData(OSCond* cond) {
    std::lock_guard<std::mutex> lock(GetCondMapMutex());
    auto& map = GetCondMap();
    auto it = map.find(cond);
    if (it == map.end()) {
        auto result = map.emplace(cond, std::make_unique<PCCondData>());
        return *result.first->second;
    }
    return *it->second;
}

void ClearCondMap() {
    std::lock_guard<std::mutex> lock(GetCondMapMutex());
    auto& map = GetCondMap();
    for (auto& pair : map) {
        pair.second->cv.notify_all();
    }
    map.clear();
}

// ============================================================================
// C API functions
// ============================================================================

extern "C" {

void OSInitMutex(OSMutex* mutex) {
    if (!mutex) return;
    OSInitThreadQueue(&mutex->queue);
    mutex->thread = nullptr;
    mutex->count  = 0;

    // Create/reset side-table entry
    GetMutexData(mutex);
}

void OSLockMutex(OSMutex* mutex) {
    if (!mutex) return;

    PCMutexData& data = GetMutexData(mutex);
    data.nativeMutex.lock();

    // Update GC-visible fields
    OSThread* currentThread = OSGetCurrentThread();
    mutex->thread = currentThread;
    mutex->count++;
}

void OSUnlockMutex(OSMutex* mutex) {
    if (!mutex) return;

    OSThread* currentThread = OSGetCurrentThread();
    if (mutex->thread != currentThread) return;

    mutex->count--;
    if (mutex->count == 0) {
        mutex->thread = nullptr;
    }

    PCMutexData& data = GetMutexData(mutex);
    data.nativeMutex.unlock();
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    if (!mutex) return FALSE;

    PCMutexData& data = GetMutexData(mutex);
    if (data.nativeMutex.try_lock()) {
        OSThread* currentThread = OSGetCurrentThread();
        mutex->thread = currentThread;
        mutex->count++;
        return TRUE;
    }
    return FALSE;
}

// ============================================================================
// Internal: unlock all mutexes held by a thread (called on thread exit)
// ============================================================================

void __OSUnlockAllMutex(OSThread* thread) {
    // On GC this walks the thread's mutex queue.
    // On PC the native mutexes are cleaned up when threads exit.
    // Clear the GC-visible queue.
    if (!thread) return;
    thread->queueMutex.head = nullptr;
    thread->queueMutex.tail = nullptr;
}

int __OSCheckDeadLock(OSThread* thread) {
    // Simplified: native OS handles deadlock detection.
    return 0;
}

int __OSCheckMutexes(OSThread* thread) {
    return 1;
}

// ============================================================================
// Condition Variable API
// ============================================================================

void OSInitCond(OSCond* cond) {
    if (!cond) return;
    OSInitThreadQueue(&cond->queue);
    GetCondData(cond);
}

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
    if (!cond || !mutex) return;

    PCCondData& condData = GetCondData(cond);
    PCMutexData& mutexData = GetMutexData(mutex);

    // Save and clear the GC mutex state
    OSThread* currentThread = OSGetCurrentThread();
    s32 savedCount = mutex->count;
    mutex->count = 0;
    mutex->thread = nullptr;

    // LOST-WAKEUP FIX: the previous implementation unlocked the recursive
    // mutex `savedCount` times and only THEN re-acquired it to call
    // cv.wait(). That left a window where the mutex was fully free *before*
    // the thread was actually waiting on the condvar. A concurrent
    // OSSignalCond() landing in that window (e.g. mDoMemCd save() posting
    // COMM_STORE_e on the main thread while the memcard worker is between
    // the predicate check and cv.wait) is lost -> the worker sleeps forever
    // -> store() never runs -> save hangs. Keep ONE level of the recursive
    // lock held continuously so cv.wait() is what atomically releases it;
    // there is no longer any gap for a signal to slip through.
    if (savedCount >= 1) {
        // Drop the extra recursive levels, keep exactly one held.
        for (s32 i = 1; i < savedCount; i++) {
            mutexData.nativeMutex.unlock();
        }
        // Adopt the single held level; wait() releases it atomically.
        std::unique_lock<std::recursive_mutex> lock(mutexData.nativeMutex, std::adopt_lock);
        condData.cv.wait(lock);
        // wait() returned with the lock re-acquired once. Detach the
        // unique_lock without unlocking, then restore the recursion depth.
        lock.release();
        for (s32 i = 1; i < savedCount; i++) {
            mutexData.nativeMutex.lock();
        }
    } else {
        // Contract violation (mutex not held on entry). Preserve the old
        // behaviour rather than adopting a lock we don't own.
        std::unique_lock<std::recursive_mutex> lock(mutexData.nativeMutex);
        condData.cv.wait(lock);
    }

    // Restore GC mutex state
    mutex->thread = currentThread;
    mutex->count  = savedCount;
}

void OSSignalCond(OSCond* cond) {
    if (!cond) return;
    PCCondData& condData = GetCondData(cond);
    condData.cv.notify_all();
}

#ifdef __cplusplus
}
#endif
