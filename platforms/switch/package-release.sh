#!/usr/bin/env bash
# Assemble the Dusklight Switch release — a single self-provisioning .nro (HayatoG/dusklight#5,
# Option B). The NRO bundles a default config + warm shader caches in its romfs (romfs:/seed/, staged
# by make-nro.sh) and writes them to sdmc:/TwilitRealm/Dusklight/ on first boot, so the package is
# essentially just the .nro plus the install guides — no data folder or data_location.json shipped.
#
# Run on the Windows host (Git Bash), AFTER build-docker.sh + make-nro.sh have produced dusklight.nro:
#   bash platforms/switch/package-release.sh [OUT_DIR]
# Env overrides:
#   NRO        path to dusklight.nro   (default: /d/dusklight-build/dusklight.nro)
#   OUT_DIR    staging dir             (default: /d/dusklight-build/Dusklight-Release)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NRO="${NRO:-/d/dusklight-build/dusklight.nro}"
OUT_DIR="${1:-${OUT_DIR:-/d/dusklight-build/Dusklight-Release}}"
REL_SRC="$REPO/platforms/switch/release"

echo "Repo: $REPO"
echo "NRO:  $NRO"
echo "Out:  $OUT_DIR"
[ -f "$NRO" ] || { echo "ERROR: NRO not found: $NRO (build it first)"; exit 1; }

# Clean staging (throwaway build artifact) and lay out the single-folder package.
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/switch"
cp "$NRO" "$OUT_DIR/switch/dusklight.nro"
cp "$REL_SRC/INSTALACAO-pt-BR.md"   "$OUT_DIR/INSTALACAO-pt-BR.md"
cp "$REL_SRC/INSTALLATION-en-US.md" "$OUT_DIR/INSTALLATION-en-US.md"

# README.txt with provenance stamped from git (| delimiter — branch names contain '/').
DUSK_REV="$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo unknown)"
DUSK_BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
AUR_REV="$(git -C "$REPO/extern/aurora" rev-parse --short HEAD 2>/dev/null || echo unknown)"
AUR_BRANCH="$(git -C "$REPO/extern/aurora" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
sed -e "s|@DUSK_REV@|$DUSK_REV|"       -e "s|@DUSK_BRANCH@|$DUSK_BRANCH|" \
    -e "s|@AURORA_REV@|$AUR_REV|"      -e "s|@AURORA_BRANCH@|$AUR_BRANCH|" \
    "$REL_SRC/README.txt.in" > "$OUT_DIR/README.txt"

# NOTE: no sdmc:/game/ folder and no TwilitRealm/Dusklight data folder are shipped — the NRO seeds
# them itself on first boot. The user only adds their own disc image into the data folder.

ZIP="$(dirname "$OUT_DIR")/Dusklight-Switch-Release.zip"
rm -f "$ZIP"
if command -v zip >/dev/null 2>&1; then
  ( cd "$OUT_DIR" && zip -r -q "$ZIP" . )
  echo "Zip:  $ZIP"
else
  echo "NOTE: 'zip' not on PATH. Staging is ready; zip it from PowerShell with:"
  echo "  Compress-Archive -Path '$(cygpath -w "$OUT_DIR")\\*' -DestinationPath '$(cygpath -w "$ZIP")' -Force"
fi

echo "Done. Staging contents:"
find "$OUT_DIR" -type f -printf '  %P (%s bytes)\n' 2>/dev/null || ls -laR "$OUT_DIR"
