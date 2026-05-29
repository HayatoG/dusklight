#!/usr/bin/env bash
# Overnight orchestrator: wait for build.log to link, make .nro, ping Switch, FTP if online.
# Emits one line per state transition (Monitor consumes each as an event).
set -uo pipefail

BUILD_DIR=/d/dusklight-build-nvk
BUILD_LOG="$BUILD_DIR/build.log"
ELF="$BUILD_DIR/dusklight.elf"
NRO="$BUILD_DIR/dusklight.nro"
SWITCH_IP=192.168.1.11
THRESHOLD=$(date +%s)
LAST_STEP=""
# Baseline of FAILED: lines that already exist BEFORE we start monitoring (old run residue).
# We only treat NEW failures (count > baseline) as real errors. Also wait for build.log to be
# rewritten by ninja's fresh run (mtime > THRESHOLD) before counting at all.
LAST_ERR_COUNT=$(awk 'BEGIN{c=0} /^FAILED:/{c++} END{print c}' "$BUILD_LOG" 2>/dev/null || echo 0)
echo "[$(ts)] baseline: $LAST_ERR_COUNT pre-existing FAILED: lines in build.log (ignoring)"

ts() { date +%H:%M:%S; }

# ───── Phase 1: wait for build to finish (link OK or first new FAILED) ─────
while true; do
  if [ -f "$BUILD_LOG" ]; then
    step=$(grep -aE "^\[[0-9]+/" "$BUILD_LOG" 2>/dev/null | tail -1 | tr -d '\r' | cut -c1-110)
    if [ -n "$step" ] && [ "$step" != "$LAST_STEP" ]; then
      echo "[$(ts)] $step"
      LAST_STEP="$step"
    fi
    bl_ts=$(stat -c %Y "$BUILD_LOG" 2>/dev/null || echo 0)
    if [ "$bl_ts" -gt "$THRESHOLD" ]; then
      # Check link success
      if grep -qaE "Linking CXX executable dusklight" "$BUILD_LOG" 2>/dev/null \
        && [ -f "$ELF" ] \
        && [ $(($(date +%s) - $(stat -c %Y "$ELF" 2>/dev/null || echo 0))) -lt 180 ]; then
        echo "[$(ts)] [LINKED] elf=$(du -h "$ELF" | cut -f1)"
        break
      fi
      # Check NEW failure (since threshold)
      fcount=$(awk 'BEGIN{c=0} /^FAILED:/{c++} END{print c}' "$BUILD_LOG" 2>/dev/null || echo 0)
      if [ "$fcount" -gt "$LAST_ERR_COUNT" ]; then
        echo "[$(ts)] [ERR] new build failure detected:"
        grep -aE "FAILED:|error:" "$BUILD_LOG" 2>/dev/null | grep -v "note:" | tail -8
        echo "[$(ts)] aborting orchestrator; .nro NOT produced"
        exit 1
      fi
      LAST_ERR_COUNT=$fcount
    fi
  fi
  sleep 30
done

# ───── Phase 2: produce .nro ─────
echo "[$(ts)] producing .nro via make-nro.sh inside docker…"
MSYS_NO_PATHCONV=1 docker run --rm \
  -v "D:\Projects\dusklight:/dusklight" \
  -v "D:\dusklight-build-nvk:/dusklight/build-switch" \
  devkitpro/devkita64 bash /dusklight/platforms/switch/make-nro.sh > /tmp/make_nro.out 2>&1
if [ -f "$NRO" ] && [ $(($(date +%s) - $(stat -c %Y "$NRO"))) -lt 120 ]; then
  echo "[$(ts)] [NRO OK] $(du -h "$NRO" | cut -f1) at $NRO"
else
  echo "[$(ts)] [NRO FAILED] make-nro.sh output:"
  tail -10 /tmp/make_nro.out
  exit 2
fi

# ───── Phase 3: ping Switch, FTP deploy if online ─────
echo "[$(ts)] pinging Switch at $SWITCH_IP…"
if ping -n 1 -w 3000 "$SWITCH_IP" >/dev/null 2>&1 \
   || ping -c 1 -W 3 "$SWITCH_IP" >/dev/null 2>&1; then
  echo "[$(ts)] Switch ALIVE — FTP uploading…"
  if curl -s -T "$NRO" "ftp://$SWITCH_IP:5000/sdmc:/switch/" 2>&1; then
    echo "[$(ts)] [DEPLOYED] dusklight.nro on Switch sdmc:/switch/ — ready to launch from hbmenu"
  else
    echo "[$(ts)] [FTP FAILED] (Switch reachable but Sphaira FTP not responding on :5000)"
  fi
else
  echo "[$(ts)] Switch OFFLINE — .nro stays at $NRO, deploy manually tomorrow"
fi

echo "[$(ts)] orchestrator DONE"
