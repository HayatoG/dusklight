# RESUME — Dusklight Switch port, próxima sessão

> Última sessão: 2026-05-29. Tudo commitado e pushado em HayatoG private.

## 🏁 Estado atual (TUDO FUNCIONANDO)

| Etapa | Status |
|---|---|
| Boot do engine | ✅ |
| Launcher Dusk renderiza | ✅ |
| Joy-Con navega no launcher | ✅ |
| "Iniciar Jogo" → file_select | ✅ |
| Criar save (Aurora `.gci` em `sdmc:/aurora/USA/Card A/`) | ✅ |
| Save persiste entre reboots | ✅ |
| Cutscene inicial OPENING_SCENE | ✅ |
| Nomear Epona (NAME_SCENE sublayer) | ✅ |
| Pós-confirmação Epona → gameplay (Vila Ordon) | ✅ |
| FPS gameplay | ~17 fps |

**Fixes da última sessão:**
1. `config.json` agora persiste (libnx FsFs rename + remove fallback em `src/dusk/config.cpp`)
2. `SHOW_TV_SETTINGS_SCREEN=0` no Switch (`src/d/d_s_name.cpp`) — pula BrightCheck que não renderiza
3. Auto-Dusklight preset on first launch (`src/m_Do/m_Do_main.cpp` + `src/dusk/ui/preset.{cpp,hpp}`) — usuário nunca cai no preset "Classic" que reativaria o bug
4. Logs de instrumentação removidos pra ganhar fps

## 📦 Tudo commitado e em HayatoG (private)

| Repo | Branch | HEAD | URL |
|---|---|---|---|
| `dusklight` | `main` | `7d97ce0c8a` | `github.com/HayatoG/dusklight` |
| `aurora-switch` (submod) | `dusklight-switch-port` | `9197c9bfac` | `github.com/HayatoG/aurora-switch` |
| `dawn-switch` | `switch` | `5f3439f159` | `github.com/HayatoG/dawn-switch` |
| `switch-nvk` | `switch-port/nvk-wsi` (M-WSI-1) | `875663f` | `github.com/HayatoG/switch-nvk` |
| `switch-nvk` | `switch-port/wsi-zero-copy` (M-WSI-2 WIP) | `44fdcf4` | (mesma) |
| `dusklight-setup` (docs) | `main` | `94a699f` | `github.com/HayatoG/dusklight-setup` |
| **Docker image** | `:latest` + `:2026-05-29` | sha256:f28cc0c | `ghcr.io/hayatog/switch-nvk-build` |

## 🎯 Próximas frentes (ordem de ROI)

### 1. M-WSI-2: zero-copy WSI (🚧 branch `switch-port/wsi-zero-copy`)

**ROI:** 17 → 25-30 fps  
**Esforço:** 1-2 semanas  
**Onde:** `switch-nvk/mesa-25/src/vulkan/wsi/wsi_common_switch.c` + `winsys/drm_shim.c`  
**Receita exata:** [`PLAN_WSI_NWINDOW.md` §5](https://github.com/HayatoG/switch-nvk/blob/switch-port/wsi-zero-copy/PLAN_WSI_NWINDOW.md) — endereços Ghidra do Dan já decompilados  
**Tracking:** [issue #1](https://github.com/HayatoG/switch-nvk/issues/1)

Comando pra começar:
```bash
cd /d/switch-nvk
git checkout switch-port/wsi-zero-copy
less PLAN_WSI_NWINDOW.md   # §5 é o que importa
# Editar mesa-25/src/vulkan/wsi/wsi_common_switch.c
# Build: ghcr.io/hayatog/switch-nvk-build:latest
```

### 2. TARGET_PC carve-out (audit feito, falta aplicar)

**ROI:** elimina classe inteira de bugs latentes  
**Esforço:** dias-semanas (depende quais sites)  
**Doc:** `platforms/switch/TARGET_PC_AUDIT.md` + `platforms/switch/PLAN_TARGET_PC_AUDIT.md`

Sites de alta-prioridade conhecidos:
- `src/d/actor/d_a_movie_player.cpp` (45 hits) — relacionado ao bug de cutscenes
- `src/d/actor/d_a_alink.cpp` (30+8 hits) — Link player actor
- `src/m_Do/m_Do_graphic.cpp` (17 hits) — graphics layer

### 3. Cortar Dawn (Aurora → Vulkan direto)

**ROI:** +20-30% adicional após zero-copy  
**Esforço:** 3-5 semanas  
**Onde:** `aurora` — adicionar `lib/vulkan/` backend, deletar `lib/webgpu/` no consumo Switch  
Vê seção `PLAN_WSI_NWINDOW.md` §6.1.

### 4. Pipeline cache warm-boot

**ROI:** elimina spikes de 1-2s ao carregar cena nova  
**Esforço:** 2-3 dias  
**Onde:** NVK pipeline cache to/from `sdmc:/dusklight/pipeline_cache.bin`

### 5. Cutscenes (JStudio TParse — mais profundo)

**ROI:** cutscenes do jogo  
**Esforço:** investigação aberta  
**Estado:** OPENING_SCENE roda; pós-Epona-naming a cutscene de Hena devolvendo Epona quebra (engine vivo, render preto). Suspeita: STBWAIT esperando áudio que nunca toca (audio stubbed).

### 6. Audio

Hoje `DUSK_AUDIO_DISABLED`. Reativar requer porting de JAudio2 + driver libnx `audrenv2`. Trabalho de port grande.

## 🔧 Comandos úteis pro próximo dia

```bash
# Rebuild incremental
cd /d/Projects/dusklight
bash platforms/switch/build-docker.sh build-only
docker run --rm -v "D:\Projects\dusklight:/dusklight" \
    -v "D:\dusklight-build-nvk:/dusklight/build-switch" \
    devkitpro/devkita64 \
    bash /dusklight/platforms/switch/make-nro.sh

# Deploy (Sphaira em Netloader)
timeout 240 /c/devkitPro/tools/bin/nxlink.exe -s -a 192.168.1.6 \
    /d/dusklight-build-nvk/dusklight.nro

# Pull log
curl -sS "ftp://192.168.1.6:5000/sdmc:/dusklight.log" -o latest.log

# Crash report (se rolar)
LATEST=$(curl -sS "ftp://192.168.1.6:5000/sdmc:/atmosphere/crash_reports/" | tail -1 | awk '{print $NF}')
curl -sS "ftp://192.168.1.6:5000/sdmc:/atmosphere/crash_reports/$LATEST" -o crash.log
```

## 📚 Docs canônicos

- **Setup de zero:** `github.com/HayatoG/dusklight-setup` (PT-BR, 22 seções)
- **Save flow mapeado:** `platforms/switch/SAVE_FLOW_MAP.md`
- **TARGET_PC audit:** `platforms/switch/TARGET_PC_AUDIT.md`
- **Switch port heuristics:** memória em `~/.claude/projects/D--Projects-dusklight/memory/MEMORY.md`
- **WSI roadmap consolidado:** `https://github.com/HayatoG/switch-nvk/blob/switch-port/wsi-zero-copy/PLAN_WSI_NWINDOW.md`

---

*Parar por hoje, 2026-05-29. Próxima sessão: começar M-WSI-2 ou TARGET_PC migration — sua escolha.*
