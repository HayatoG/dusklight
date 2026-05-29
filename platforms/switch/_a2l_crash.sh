#!/usr/bin/env bash
cd /dusklight/build-switch || exit 1
A2L=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line
ELF=dusklight.elf
exec > a2l_crash.log 2>&1
echo "ELF: $ELF ($(du -h $ELF | cut -f1))"
echo "=== LR (call site -> null) ==="
$A2L -f -C -e "$ELF" 0x42958c
echo "=== stack (return addresses) ==="
for off in 0x429ae8 0x43ef70 0x42a184 0x42bdcc 0x42c0f4 0x469bb8 0x469fe0 0x12e1280 0x12e1c08 0x1280c5c 0x1265370 0x123ae48 0x123b11c 0xe34dc4 0xe36260 0xe1cd18 0xe56084; do
  echo "--- +$off ---"
  $A2L -f -C -e "$ELF" $off
done
echo "DONE"
