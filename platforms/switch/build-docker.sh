#!/usr/bin/env bash
# Dusklight → Switch cross-build helper, runs inside devkitpro/devkita64 container.
#
# Usage (from project root, on host with Docker installed):
#   ./platforms/switch/build-docker.sh           # default: cmake configure
#   ./platforms/switch/build-docker.sh build     # configure + build
#   ./platforms/switch/build-docker.sh shell     # drop into interactive shell
#
# We mount the project at /dusklight inside the container so paths resolve
# the same on host and container.

set -e

IMAGE="${DUSK_DOCKER_IMAGE:-devkitpro/devkita64}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Build output lives on D: (C: is full; user authorized full D: access). The dir is
# mounted INTO the container at the SAME path (/dusklight/build-switch) so CMake's
# absolute paths stay valid and the Dawn/RmlUi build cache is preserved.
# Override with DUSK_BUILD_DIR (POSIX form, e.g. /d/dusklight-build).
HOST_BUILD_DIR="${DUSK_BUILD_DIR:-/d/dusklight-build}"
BUILD_DIR="$HOST_BUILD_DIR"
LOG="${BUILD_DIR}/configure.log"
MODE="${1:-configure}"

echo "=== Dusklight Switch build helper ==="
echo "Project: $PROJECT_DIR"
echo "Image:   $IMAGE"
echo "Mode:    $MODE"
echo ""

# Probe Docker
if ! docker version >/dev/null 2>&1; then
    echo "ERROR: docker daemon not reachable. Start Docker Desktop and retry." >&2
    exit 1
fi

# Pull image if missing (no-op if cached)
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Pulling $IMAGE (first run, ~2 GB)..."
    docker pull "$IMAGE"
fi

mkdir -p "$BUILD_DIR"

# On Windows MSYS bash, /c/foo gets translated weirdly when passed to docker.
# Use a Windows-style path for -v on Windows, POSIX-style on Linux/macOS.
# M-DV-2: the NVK package (libvulkan.a + headers) lives in the separate switch-nvk repo; mount it
# read-only into the build so DAWN_SWITCH_NVK_ROOT=/switch-nvk/nvk-switch resolves. Override with DUSK_SWITCH_NVK.
SWITCH_NVK_SRC="${DUSK_SWITCH_NVK:-/d/switch-nvk}"
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Convert /c/Users/... to C:\Users\... for docker on Windows
    HOST_PATH=$(cygpath -w "$PROJECT_DIR")
    HOST_BUILD_DIR_WIN=$(cygpath -w "$HOST_BUILD_DIR")
    SWITCH_NVK_WIN=$(cygpath -w "$SWITCH_NVK_SRC")
else
    HOST_PATH="$PROJECT_DIR"
    HOST_BUILD_DIR_WIN="$HOST_BUILD_DIR"
    SWITCH_NVK_WIN="$SWITCH_NVK_SRC"
fi

CONTAINER_BUILD_DIR="/dusklight/build-switch"

# CMake configure args (matches what we tried on the Windows host).
# Disabled features that pull large optional deps for the first probe.
CMAKE_ARGS=(
    -S /dusklight
    -B "$CONTAINER_BUILD_DIR"
    -G Ninja
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DDUSK_MOVIE_SUPPORT=OFF
    -DDUSK_ENABLE_UPDATE_CHECKER=OFF
    -DDUSK_ENABLE_DISCORD=OFF
    -DAURORA_PLATFORM_SWITCH=ON
    -DAURORA_ENABLE_RMLUI=${DUSK_RMLUI:-ON}
    -DAURORA_ENABLE_IMGUI=OFF
    -DAURORA_DAWN_PROVIDER=source
    -DAURORA_DAWN_SOURCE_DIR=/dusklight/platforms/switch/reference/dawn-switch
    -DDAWN_FETCH_DEPENDENCIES=ON
    # M-DV-2: our NVK package (mounted from D:\switch-nvk at /switch-nvk). Replaces the old stub-nvk.
    -DDAWN_SWITCH_NVK_ROOT=/switch-nvk/nvk-switch
    # NVK-Vulkan backend: our NVK driver as Dawn's Vulkan backend. deko3d/GLES are OFF on this
    # branch -> the else() arm of aurora_core.cmake (lib/webgpu/gpu.cpp, the real wgpu path).
    -DAURORA_BACKEND_DEKO3D=OFF
    -DAURORA_BACKEND_GLES=OFF
    -DAURORA_BACKEND_DAWN_GL=OFF
    # DAWN_PLATFORM_SWITCH=ON drives the dawn-switch fork's Switch Vulkan path (BackendVk loaderless
    # ICD via vk_icdGetInstanceProcAddr, VK_NN_vi_surface). Vulkan ON, GL OFF, Tint SPIR-V writer ON.
    -DDAWN_PLATFORM_SWITCH=ON
    -DDAWN_ENABLE_VULKAN=ON
    -DDAWN_ENABLE_OPENGLES=OFF
    -DDAWN_ENABLE_DESKTOP_GL=OFF
    -DTINT_BUILD_GLSL_WRITER=OFF
    -DTINT_BUILD_SPV_WRITER=ON
)

run_docker() {
    # Use -it only when stdin is a terminal (interactive shells).
    # Without this, automated runs (CI, scripted Claude Code) fail with
    # "cannot attach stdin to a TTY-enabled container".
    local TTY_FLAGS=""
    if [[ -t 0 && -t 1 ]]; then
        TTY_FLAGS="-it"
    fi
    # MSYS_NO_PATHCONV stops MSYS from converting /dusklight to C:/Program Files/Git/dusklight
    MSYS_NO_PATHCONV=1 docker run --rm $TTY_FLAGS \
        -v "$HOST_PATH:/dusklight" \
        -v "$HOST_BUILD_DIR_WIN:/dusklight/build-switch" \
        -v "$SWITCH_NVK_WIN:/switch-nvk:ro" \
        -w /dusklight \
        "$IMAGE" "$@"
}

case "$MODE" in
    configure)
        echo "=== Running cmake configure inside container ==="
        run_docker bash -lc "cmake ${CMAKE_ARGS[*]} 2>&1 | tee /dusklight/build-switch/configure.log; tail -1 /dusklight/build-switch/configure.log"
        ;;
    build)
        echo "=== Configure + build ==="
        run_docker bash -lc "cmake ${CMAKE_ARGS[*]} 2>&1 | tee /dusklight/build-switch/configure.log && cmake --build $CONTAINER_BUILD_DIR -j\$(nproc) 2>&1 | tee /dusklight/build-switch/build.log"
        ;;
    build-only)
        # Incremental build — SKIPS the ~4min cmake configure/generate (use after source-only
        # edits, when CMake flags are unchanged). Requires a prior `build`/`configure` to exist.
        echo "=== Build only (skip configure) ==="
        run_docker bash -lc "cmake --build $CONTAINER_BUILD_DIR -j\$(nproc) 2>&1 | tee /dusklight/build-switch/build.log"
        ;;
    shell)
        echo "=== Interactive shell inside container ==="
        echo "Hint: cmake ${CMAKE_ARGS[*]}"
        run_docker bash -l
        ;;
    *)
        echo "Unknown mode: $MODE"
        echo "Usage: $0 [configure|build|shell]"
        exit 1
        ;;
esac

if [[ -f "$LOG" ]]; then
    echo ""
    echo "Log saved to: $LOG"
    echo "Last 5 lines:"
    tail -5 "$LOG"
fi
