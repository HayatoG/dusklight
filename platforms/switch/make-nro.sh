#!/usr/bin/env bash
# Produce dusklight.nro from the built dusklight.elf. Run inside devkitpro/devkita64:
#   docker run --rm -v D:\Projects\dusklight:/dusklight -v D:\dusklight-build-nvk:/dusklight/build-switch \
#     devkitpro/devkita64 bash /dusklight/platforms/switch/make-nro.sh
# elf2nro packs only PT_LOAD segments, so it ignores the (huge) debug sections in the unstripped .elf
# -> no need to `strip` the 751MB .elf first (which OOM-kills in the container).
cd /dusklight/build-switch || exit 1
exec > make_nro.log 2>&1          # all output to a log file (avoids PowerShell buffering games)
set -x
echo "elf size: $(du -h dusklight.elf | cut -f1)"
nacptool --create "Dusklight" "TwilitRealm" "v1.4.1-opt" dusklight.nacp
echo "nacptool rc=$?"
ICON=/dusklight/platforms/switch/icon.jpg
# Bundle res/ as romfs so the launcher UI (fonts, .rcss stylesheets, logo.png)
# can fopen("romfs:/res/...") at runtime. Without this every SDL_IOFromFile()
# returns NULL and RmlUi falls back to fontless rendering.
# elf2nro --romfsdir packs DIR contents at romfs:/ — we need a `res/` subdir
# at the root, so stage romfs_stage/res -> /dusklight/res via symlink.
ROMFS_STAGE=/tmp/dusklight_romfs
rm -rf "$ROMFS_STAGE"
mkdir -p "$ROMFS_STAGE"
if [ -d /dusklight/res ]; then
  ln -s /dusklight/res "$ROMFS_STAGE/res"
fi
ROMFS_ARG=""
if [ -d "$ROMFS_STAGE/res" ]; then
  ROMFS_ARG="--romfsdir=$ROMFS_STAGE"
  echo "romfs staged at $ROMFS_STAGE:"
  ls -la "$ROMFS_STAGE/"
fi
if [ -f "$ICON" ]; then
  elf2nro dusklight.elf dusklight.nro --icon="$ICON" --nacp=dusklight.nacp $ROMFS_ARG
else
  elf2nro dusklight.elf dusklight.nro --nacp=dusklight.nacp $ROMFS_ARG
fi
echo "elf2nro rc=$?"
ls -la dusklight.nro
echo "DONE"
