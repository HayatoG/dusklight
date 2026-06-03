# RESUME — Dusklight Switch port

> Documento vivo. Snapshots cronológicos. **Nunca apagar info anterior** —
> adicionar nova seção em cima e arquivar a antiga em baixo.

---

## ✅ 2026-06-02 (madrugada) — STUTTER DE LOG resolvido + análise de FPS + Release do NRO

**Stutter/spikes do gameplay = LOG.** No Switch não há args de CLI, então `startupLogLevel`
ficava no default `LOG_DEBUG(0)` → o filtro a montante (`aurora/lib/logging.hpp:22`,
`g_config.logLevel > level → return`) **não dropava nada**, e cada linha `[DEBUG|dusk]`
per-frame (`fapGm_Execute`, `fpc*`, `Loading Resource`…) batia em `aurora_log_callback`, que
faz **DUPLO `fflush`** (stdout + arquivo SD) por linha → stutter constante + spikes nos loads.
**Fix:** `m_Do_main.cpp` força `config.logLevel = LOG_WARNING` no Switch → o filtro dropa o
DEBUG antes de formatar/flushar. **Confirmado na TV: spikes sumiram.** (DEBUG é o único volume
pesado; INFO/WARNING/FATAL têm a MESMA perf — WARNING mantém avisos/erros úteis.)

**NRO publicado** (Release prerelease, privado HayatoG):
`github.com/HayatoG/dusklight/releases/tag/switch-2026-06-02-msgfix` — esse build tem C + B-hang
+ anti-stutter.

### 🎯 FPS — análise do breakdown (`[SwitchProfile][aurora_end]`, steady ~30fps)
| fase | ms | nota |
|---|---|---|
| **submit** | **5–12** | ⬅️ DOMINA (50-70%). Submissão de cmds do Dawn. |
| render | 1–2 | escala com resolução (por isso 360p >> 720p) |
| present | ~2 | já otimizado (zero-copy) |
| rml | ~1 | RmlUi desenhado todo frame, mesmo in-game |

TP é 30fps nativo; cenas leves batem 30, pesadas caem qdo `submit+render+sim > 33ms`.
**Levers (ordem de impacto):** (1) **resolução interna** (`internalResolutionScale` — ganho
fácil/grande, user já comprovou 360p; achar sweet-spot ~480p + tornar setting); (2) **cortar
Dawn** (submit, Aurora→NVK direto — semanas, é o teto do baseline); (3) **reduzir draw calls**
(submit escala c/ draws); (4) **RmlUi in-game** (~1ms, desligar se não precisa).

### Frentes abertas (escolha do user)
1. **FPS** — começar pela resolução (sweet-spot + setting), ou profiling limpo, ou cortar Dawn.
2. **Merge do upstream** `TwilitRealm/dusklight` (remote `upstream` já configurado) — traz
   features novas; merge cuidadoso pra não perder os carve-outs `#ifdef __SWITCH__`.
3. **Overlay ImGui in-game** (settings/debug): suíte em `src/dusk/imgui/` (excluída no Switch via
   `CMakeLists.txt:498`, `imgui.h` stubado). A Aurora JÁ tem hook ImGui (`imGuiInitCallback`,
   render pelo backend) → fundação existe; falta re-incluir + ImGui real + input Joy-Con.

---

## ✅ 2026-06-02 (noite) — BUGS C e B-HANG RESOLVIDOS na TV (commit `f451660e79`)

> ⚠️ CORREÇÃO da seção abaixo (que dizia "B e C são o MESMO bug"): **estava ERRADA.**
> Os heartbeats por-frame no HW provaram que são DOIS bugs independentes, com causas
> totalmente diferentes. Ambos consertados e confirmados na TV/HW hoje.

**Bug C — sim/não do gameplay sem texto: RESOLVIDO ✅ (confirmado TV+log).**
Não era parse nem fonte — a extração da mensagem enchia `mSelText` com "Sim"/"Não"
certinho. Era um **override quebrado**: `dMsgScrnTalk_c`/`dMsgScrnItem_c::setSelectString`
declaravam `char*`, mas no Dusk `DUSK_CONST=const` torna a virtual da base
`setSelectString(char DUSK_CONST*, ...)` — assinatura DIFERENTE → não sobrescrevia →
`mpScrnDraw->setSelectString` (ponteiro base) rodava a virtual VAZIA da base → o texto
nunca chegava aos painéis. No GC `DUSK_CONST` é vazio, casava, funcionava. Fix: usar
`char DUSK_CONST*` nas 4 assinaturas (talk + item, .h + .cpp).

**Bug B — trava preta/cinza pós-save: RESOLVIDO ✅ (HW, continua pra cutscene).**
O save de evento (celeiro) é `gameOverType==2` → `dGameover_c` (a TELA DE GAME-OVER é o
veículo do save de evento; NÃO é o menu collect — por isso `[mw]` nunca disparou e o
"B-FIX" antigo do collect era irrelevante). O save GRAVA certo. A trava: a mensagem-guia
pós-save (msg 0x4E4) fica em `STATUS_MOVE` esperando um trigger de pad pra fechar, mas no
contexto game-over/pause o input **não chega** ao `move_proc` (`getTrigA(PAD_1)`=0 sempre,
mesmo apertando A) → espera pra sempre. Fix: `saveGuide()` força o dismiss no Switch
(`onForceSelect`, já que `field_0x58==true` pra essa msg) → o fluxo completa e o jogo resume.

### 🔴 ISSUES QUE FALTAM (próxima sessão)
1. **B-visibilidade** (a "tela preta/cinza" que sobra): os diálogos do save de evento
   (caixa SIM/NÃO + mensagem-guia) estão **invisíveis** durante o game-over — os dados
   estão certos, mas o draw fica ATRÁS do backdrop preto opaco do game-over
   (`dgo_screen_c`, `setBackAlpha(1.0)`) e/ou do JUTFader (None=alpha 0xFF=preto). É um
   bug de **ordem/camada/alpha de draw 2D**, não de dados. O usuário passa, mas às cegas.
2. **Input morto no pause/game-over:** `getTrigA(PAD_1)`=0 no `move_proc` do explain
   durante o game-over (contornado no B via `onForceSelect`, mas a raiz — pad não
   entregando trigger no contexto pause/game-over — segue latente; pode afetar outros
   diálogos de pause). `read()` roda (sim ativo), então a causa é mais sutil (trigger
   consumido/limpo antes do explain ler? input routing no pause?). Investigar
   `mDoCPd_c`/JUTGamePad no contexto pause.
3. **Purgar probes obsoletos** do commit `b08b5fa` (diagnóstico do save-hang, já resolvido):
   `[ms]`/`[talk]`/`[mwf]`/`[mc]`/`[mw]`/`[cof]` em m_Do_graphic, m_Do_MemCard,
   d_menu_window, d_msg_out_font, d_msg_scrn_talk, m_Do_main. Contribuem p/ a queda de FPS
   na tela de mensagem.
4. **Bug latente** (fiel ao GC, inativo): `d_gameover.cpp:322` `getGameOverType()==1 && ==2`
   é sempre-falso (deveria ser `||`). Não é a trava atual (vem DEPOIS do save fechar).
5. **Auditar DUSK_CONST/override:** o mesmo padrão `char*` vs `char DUSK_CONST*` pode
   quebrar OUTROS overrides de virtuals de `dMsgScrnBase_c` (e similares). Vale uma varredura.

---

## ⏰ PRÓXIMA SESSÃO — COMEÇAR POR AQUI (lembrar o Guilherme)

> ⚠️ CORREÇÃO de um snapshot anterior de hoje que dizia "save RESOLVIDO": o save
> **GRAVA** certo (isso está resolvido), mas o **pós-save trava** — e essa parte
> NÃO estava resolvida. Estado real consolidado abaixo.

**🎯 ALVO ÚNICO DA PRÓXIMA SESSÃO: o subsistema de TELA DE MENSAGEM (`d_msg_scrn_*`)
no Switch.** A longa sessão de HW de 2026-06-02 (Switch=.10) provou que **dois bugs
são o MESMO**:
- **B — preto/cinza pós-save:** o save grava (`[mc] store() EXIT state=4` confirmado
  4×), mas o menu trava num proc que **espera a tela de mensagem fechar** e ela nunca
  fecha. Ex.: BLACK_EVENT auto-save (useType=4) → `PROC_SAVE_GUIDE`(5):
  `if (mpScrnExplain->getStatus()==0) avança;` mas **`getStatus()` nunca dá 0**
  (STATUS_WAIT). O `[mwf]` mostrou o **JUTFader CICLANDO** `None→FadeIn→Wait→FadeOut→None`
  pra sempre = a `dMsgScrnExplain_c` **oscila abrir↔fechar, nunca assenta em WAIT** →
  o mundo nunca aparece → preto/cinza. (JUTFader status 0=None=alpha 0xFF=preto opaco
  por design.)
- **C — sim/não sem texto:** `[talk] drawSelf len=21..66` prova que os glifos ESTÃO
  montados (contagem certa) — o texto só **não é DESENHADO**. Texto principal =
  `mpScreen->draw` (panes J2DTextBox, font=`mDoExt_getMesgFont` que ESTÁ carregado),
  **não** o `COutFont` (`mpOutFont->draw(NULL)` = só glifos especiais → por isso meu
  probe `[cof]`=0).

⇒ **Consertar o `d_msg_scrn_*` (render do glifo J2DTextBox + state machine do explain)
resolve B E C juntos.** Arquivos: `src/d/d_msg_scrn_explain.cpp` (state machine
`move_process[mStatus]` — por que mStatus não assenta em STATUS_WAIT + o input/keyWait
de dismiss), `src/d/d_msg_scrn_talk.cpp`/`d_msg_scrn_base.cpp` (o `mpScreen->draw`
J2DTextBox — por que o texto setado+com-font não aparece), `src/d/d_msg_out_font.cpp`.
Comparar com QUALQUER texto J2D que renderize no Switch (o launcher é RmlUi, não J2D!).

**✅ JÁ FEITO E SÓLIDO (commitado nesta sessão):**
- **Save WRITE corrigido** = lost-wakeup no `OSWaitCond` (`src/dusk/OSMutex.cpp`):
  soltava o mutex 100% antes do `cv.wait()` → o `OSSignalCond(COMM_STORE_e)` se perdia.
  Fix = manter 1 nível do lock recursivo até o `cv.wait()` (adopt_lock + `lock.release()`).
  Confirmado 4× no HW. (Load respawnando na casa do Link sem Epona = vanilla TP, não bug.)
- **switch_stubs `dusk_switch_log` força flush** em linhas `[m`/`[t`/`[c` → log FTP
  confiável mesmo travado (antes o lazy-flush escondia o save). Lição: capturar via
  `nxlink -s` ao vivo OU não relançar antes de puxar FTP (o log trunca a cada boot).
- Instrumentação `dusk_switch_log` em árvore p/ a próxima sessão: `[mc]` (cadeia save),
  `[mw]` (save-close), `[ms]` (proc-trace do `_move`), `[mwf]` (JUTFader on-change),
  `[talk]` (len do texto), `[cof]` (camada errada, pode tirar). O `dMw_fade_in()` que
  adicionei no collect_save_close DISPARA certo mas não é o fix (user cai em saves de
  EVENTO, não no collect). Switch IP via DHCP muda (hoje .10; ping/arp p/ OUI Nintendo).

**A — travadas de FPS (deferido, ordem após B/C):** present estável 136µs; gargalo CPU
no frame. 1ª alavanca barata = cortar log DEBUG (`startupLogLevel`→INFO/WARN; `report()`
filtra antes de formatar). Depois A3 (pipeline cache, spikes de compile) e A2 (cortar
Dawn, sobe baseline ~13ms/frame do submit). Precisa run limpo de profiling (SD, sem
nxlink, timers por fase) — os números medidos via nxlink-s estão inflados pelo fflush.

---

# 🟢 ESTADO ATUAL — 2026-06-02 (zero-copy WSI working)

**🎉 A1 zero-copy WSI ENTREGUE e pushado.** O present (gargalo de 98%) caiu de
**9.6ms → ~150µs (64×)**; gameplay **~21 → 28-30 fps** (picos 60). Tudo commitado
e pushado nos 3 repos privados HayatoG (zero-copy na **branch principal** de cada).

## O que foi feito hoje
1. **Profiling do present** → achou que 98% era memcpy de CPU (GPU = 20µs).
2. **Logs verbosos off** (`NVK_TRACE` gate) → gameplay 17→21, home 12→24, de graça.
3. **Zero-copy WSI** (block-linear `kind=0xfe`, render direto no buffer do nwindow,
   `present=nwindowQueueBuffer`, sem memcpy/swizzle). Provado no smoke + Dusklight.
   - Bug-chave: `NvGraphicBuffer.header.num_ints` tem que ser setado ou o
     `nwindowDequeueBuffer` trava (tela preta). + `nvFenceInit()` antes do dequeue.

## Commits/branches (todos pushados, privado HayatoG)
| Repo | Branch | Commit |
|---|---|---|
| `switch-nvk` | `master` (FF) + `switch-port/wsi-zero-copy` | `81bfc43` |
| `aurora-switch` | `dusklight-switch-port` | `bbdc576` |
| `dusklight` | `main` | (este commit) |

Durabilidade mesa: patch + `winsys/mesa-edits/` (overrides verbatim) restaurados por
`apply-wsi-switch.sh`. Setup-do-zero documentado em `switch-nvk/BUILD_AND_RUN.md §2`.

## 🐛 BUG ABERTO — save hang (celeiro da Epona) — ANÁLISE 2026-06-02

**Sintoma:** no save in-game (animais no celeiro) → tela preta + diálogo de save;
**"Sim" trava** na tela preta (jogo segue PRESENTANDO ~12fps, não é freeze total —
o `[wsi-prof]` continua), **"Não"** → "salva depois" → cutscene OK.

**Call chain (mapeado):** `saveYesNoSelect` → `yesnoSelectStart` ("Salvando…") →
`saveMoveDisp` → `dataWrite`→`dataSave` → `g_mDoMemCd_control.save()` (posta
`COMM_STORE_e` + `OSSignalCond`) → worker `mDoMemCd_Ctrl_c::main()` →
`store()` → seta `field_0x1fc8=1`. A tela de save fica em
`PROC_MEMCARD_DATA_SAVE_WAIT` chamando `SaveSync()` todo frame; `SaveSync()`
retorna 0 **enquanto `field_0x1fc8==0`**. **Hang = `field_0x1fc8` nunca vira 1.**

**O que a análise ESTÁTICA descartou (importante, não repetir erro):**
- ❌ NÃO é "COMM_STORE_e cai no no-op do SHIELD". **`VERSION=0` ⇒ `PLATFORM_GCN=TRUE`**
  no Switch (`include/global.h`: VERSION_GCN_USA=0; VERSION 0..2 = GCN). Então a
  branch `#if PLATFORM_GCN||PLATFORM_WII` compila e **`store()` É chamado** (linha
  ~156). (O comentário no `m_Do_MemCard.cpp:86` sobre "falls through" foi escrito
  com a premissa errada de GCN=false — ignorar.)
- ❌ NÃO é CARDWrite async/callback: a Aurora `CARDWrite` (`extern/aurora/lib/dolphin/
  card.cpp:735`) é **SÍNCRONA**; `store()` usa `CARDWrite`/`CARDRead` síncronos.
- ❌ NÃO é erro de card: todo caminho de erro em `store()` ainda chega no
  `field_0x1fc8=1` (linha 326). Erro ≠ hang.

**⇒ Conclusão:** estaticamente `store()` DEVERIA completar. Como trava, a causa real
exige **instrumentação em runtime**. Hipóteses a testar (em ordem):
1. **Worker não processa `COMM_STORE_e`** — `OSSignalCond`/`OSWaitCond` (emulação
   Aurora) não acordam o worker, OU o worker ficou preso num comando anterior
   (ex: `COMM_ATTACH_e`/mount). ⇒ `store()` nunca roda.
2. **`store()` roda mas BLOQUEIA** num `CARD_OPEN`/`CARDCreate`/`CARDWrite` da Aurora
   (ex: card não montado e a emulação fica em estado ruim) antes da linha 326.
3. **Hang DEPOIS do write** — `store()` completa, mas o estado seguinte
   `PROC_MEMCARD_DATA_SAVE_WAIT2` (ou um fade/anim de "save completo") trava.

**PLANO próxima sessão (1 ciclo de build):**
1. Re-instrumentar com `dusk_switch_log`: entrada/saída de `store()`, transições de
   `field_0x1fc8`, retorno de `SaveSync()`, e `mMenuProc` em `memCardDataSaveWait`
   + estados seguintes. (Arquivos: `src/m_Do/m_Do_MemCard.cpp`, `src/d/d_menu_save.cpp`.)
2. Build (relink rápido) → reproduzir o save do celeiro → puxar `sdmc:/dusklight.log`.
3. O log aponta a hipótese certa (store não chamado / store bloqueia / pós-store).
4. Carve-out `__SWITCH__` cirúrgico no ponto exato (como os fixes anteriores de save).

## 🎯 Próximas frentes de perf (ordem de impacto)
1. **Cortar Dawn (A2)** — semanas, MAS é o único que sobe o baseline (o `submit`
   do Dawn ~13ms/frame segura o gameplay em 20-30). Sem receita decompilada.
2. **Pipeline cache (A3)** — horas, ganho pequeno (só os spikes raros de compile).
3. Asset streaming — descartado (log de gameplay estável tem 0 loads).
TARGET_PC carve-out (B1): deixar reativo/crash-driven, NÃO proativo.

---

# 🟢 ESTADO — 2026-05-29 (fim do dia)

**Tudo funcionando, tudo commitado e pushado em HayatoG private.**

## Status visual no gameplay

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

## Fixes da sessão 2026-05-29

1. **`config.json` persiste** — libnx FsFs `rename()` não sobrescreve;
   `src/dusk/config.cpp::ReplaceFile()` agora faz `remove(target)` + retry-rename.
2. **`SHOW_TV_SETTINGS_SCREEN=0` no Switch** (`src/d/d_s_name.cpp`) — neutraliza
   o BrightCheck UI que não renderiza no nosso path → travava em tela preta
   após nomear Epona.
3. **Auto-Dusklight preset on first launch** (`src/m_Do/m_Do_main.cpp` +
   `src/dusk/ui/preset.{cpp,hpp}::apply_preset_dusk_silently`) — usuário nunca
   cai no preset "Classic" que reativaria o bug do BrightCheck.
4. **Logs de instrumentação removidos** pra ganhar fps (logs em `stb.cpp`,
   `d_s_name.cpp`, `d_file_select.cpp`, `m_Do_MemCard.cpp` revertidos pro
   original; as descobertas que eles produziram estão em
   `SAVE_FLOW_MAP.md` e `PLAN_WSI_NWINDOW.md`).

## Tudo committed nos repos HayatoG (private)

| Repo | Branch | HEAD | URL |
|---|---|---|---|
| `dusklight` | `main` | `454b23b338` | github.com/HayatoG/dusklight |
| `aurora-switch` (submod) | `dusklight-switch-port` | `9197c9bfac` | github.com/HayatoG/aurora-switch |
| `dawn-switch` | `switch` | `5f3439f159` | github.com/HayatoG/dawn-switch |
| `switch-nvk` | `switch-port/nvk-wsi` (M-WSI-1) | `875663f` | github.com/HayatoG/switch-nvk |
| `switch-nvk` | `switch-port/wsi-zero-copy` (M-WSI-2 WIP) | `44fdcf4` | (mesma) |
| `dusklight-setup` (docs) | `main` | `94a699f` | github.com/HayatoG/dusklight-setup |
| **Docker image** | `:latest` + `:2026-05-29` | sha256:f28cc0c | ghcr.io/hayatog/switch-nvk-build |

## 🎯 Próximas frentes (ordem de ROI)

### 1. M-WSI-2: zero-copy WSI (🚧 branch `switch-port/wsi-zero-copy`)

- **ROI:** 17 → 25-30 fps
- **Esforço:** 1-2 semanas
- **Onde:** `switch-nvk/mesa-25/src/vulkan/wsi/wsi_common_switch.c` + `winsys/drm_shim.c`
- **Receita exata:** [PLAN_WSI_NWINDOW.md §5](https://github.com/HayatoG/switch-nvk/blob/switch-port/wsi-zero-copy/PLAN_WSI_NWINDOW.md) — endereços Ghidra do Dan já decompilados
- **Tracking:** [issue #1](https://github.com/HayatoG/switch-nvk/issues/1)

Comando pra começar:
```bash
cd /d/switch-nvk
git checkout switch-port/wsi-zero-copy
less PLAN_WSI_NWINDOW.md   # §5 é o que importa
# Editar mesa-25/src/vulkan/wsi/wsi_common_switch.c
# Build: ghcr.io/hayatog/switch-nvk-build:latest
```

### 2. TARGET_PC carve-out (audit feito, falta aplicar)

- **ROI:** elimina classe inteira de bugs latentes
- **Doc:** `platforms/switch/TARGET_PC_AUDIT.md` + `platforms/switch/PLAN_TARGET_PC_AUDIT.md`
- Sites de alta-prioridade: `d_a_movie_player.cpp` (45 hits), `d_a_alink.cpp` (30+8), `m_Do_graphic.cpp` (17)

### 3. Cortar Dawn (Aurora → Vulkan direto)

- **ROI:** +20-30% adicional após zero-copy
- **Esforço:** 3-5 semanas
- Veja `PLAN_WSI_NWINDOW.md` §6.1

### 4. Pipeline cache warm-boot

- **ROI:** elimina spikes de 1-2s ao carregar cena nova
- **Esforço:** 2-3 dias

### 5. Cutscenes (JStudio TParse — mais profundo)

- OPENING_SCENE roda. Cutscenes que esperam cue de áudio travam (audio stubbed).

### 6. Audio

- Hoje `DUSK_AUDIO_DISABLED`. Reativar requer porting de JAudio2 + driver libnx `audrenv2`.

## 🔧 Comandos úteis

```bash
# Rebuild incremental
cd /d/Projects/dusklight
bash platforms/switch/build-docker.sh build-only
MSYS_NO_PATHCONV=1 docker run --rm \
    -v "D:\Projects\dusklight:/dusklight" \
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

# Compute load base para addr2line
#   nm dusklight.elf | grep " dusk_switch_log$"  -> offset estático
#   runtime_log "dusk_switch_log@=<addr>"        -> endereço runtime
#   base = runtime - static_offset
# Then subtract base from every ReturnAddress no crash report:
aarch64-none-elf-addr2line -e dusklight.elf -i -f -C <hex>
```

## 📚 Docs canônicos

- **Setup de zero (qualquer PC):** github.com/HayatoG/dusklight-setup (PT-BR, 22 seções)
- **Save flow mapeado:** `platforms/switch/SAVE_FLOW_MAP.md`
- **TARGET_PC audit:** `platforms/switch/TARGET_PC_AUDIT.md`
- **Switch port heuristics:** memória em `~/.claude/projects/D--Projects-dusklight/memory/MEMORY.md`
- **WSI roadmap consolidado:** github.com/HayatoG/switch-nvk/blob/switch-port/wsi-zero-copy/PLAN_WSI_NWINDOW.md

---

# 📜 Histórico — fim de sessão 2026-05-28 (RESOLVIDO, mantido como referência)

> Tudo nessa seção foi resolvido em 2026-05-29 mas preservado como histórico
> útil (paths, fixes específicos, recipes que ainda valem).

## Estado em 2026-05-28 (loop pós-save-create)

Build is GREEN, game boots through `mDoMch_Create`, launcher works, "Iniciar Jogo"
is unblocked. **Blocker (na época):** post-save-create LOOP — RESOLVIDO em
2026-05-29 com a mudança em `MemCardStatCheck` (removeu o override Switch
defeituoso; PLATFORM_GCN=true no nosso build, switch GCN nativo cuida).

### Loop sintomático (descrito em 2026-05-28)

User clicks **Sim** at "Deseja criar um arquivo para salvar este jogo no cartão de memória?":
1. Sees "Criando…" → "O arquivo para salvar o jogo foi criado" ✓
2. Presses A/B → loops back to the same "Deseja criar?" prompt

**Root cause descoberto em 2026-05-29:** nosso override Switch em
`MemCardStatCheck` checava `stat("sdmc:/dusklight/saves/zeldaTp.dat")` —
mas Aurora memcard emulator escreve em
`sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci` (path DIFERENTE). O save SEMPRE
existia, só estávamos olhando o lugar errado. Fix: remover o override Switch e
deixar o GCN switch nativo lidar (PLATFORM_GCN é true no nosso build).

## What's working at 2026-05-28 (state at end of that day)

1. Engine boots fully through `mDoMch_Create` + `mDoGph_Create` + all init.
2. Dusk launcher renders on TV with real fonts/CSS/logo (romfs bundled).
3. Joy-Con navigation works in the launcher (A=confirm/SOUTH, B=cancel/EAST).
4. "Iniciar Jogo" button transitions to the game without crashing
   (after the `mDoMemCd_ThdInit` idempotency guard).
5. Game scene NAME_SCENE / file-select renders (after JKRAram null-heap guard).
6. Save creation prompt appears, user can pick Yes/No, "Criando…" message shows.
   ✗ Then looped (RESOLVIDO em 2026-05-29).

## Recent fixes da sessão 2026-05-28

Cada um é `__SWITCH__`-guarded ou `__SWITCH__`-equivalent.

| File | Fix | Why |
|---|---|---|
| `src/m_Do/m_Do_MemCard.cpp::ThdInit` | `static bool sAlreadyInited` idempotency guard | Launcher Play button + mDoMch_Create both called it → 2nd OSCreateThread on live thread → std::thread destruct joinable → std::terminate |
| `src/m_Do/m_Do_machine.cpp` | Reverted the (broken) `mCardCommand == COMM_NONE_e` guard I added earlier; kept `is_prelaunch_open()` original | mCardCommand gets reset by worker, so guard was always passing |
| `src/m_Do/m_Do_machine.cpp` | Reverted 256MB heap bump → 32MB original | 256MB > MEM1 available → JKRExpHeap::create returned NULL → null deref |
| `libs/JSystem/src/JKernel/JKRAram.cpp::changeGroupIdIfNeed` | NULL check on `JKRGetCurrentHeap()` return | TLS `sCurrentHeap` is NULL on threads that didn't set it; per-site guard pattern per [[dusklight-debugging-heuristics]] #17 |
| `src/d/d_file_select.cpp::MemCardStatCheck` | (depois revertido em 2026-05-29 — vide acima) | Detect that save was created and exit the create-prompt loop |
| `src/m_Do/m_Do_MemCard.cpp::LoadSyncNAND` | (depois revertido em 2026-05-29 — código dentro de `#if PLATFORM_WII\|\|SHIELD` que não compila no Switch) | Wii NAND worker path is gated `#if PLATFORM_WII || PLATFORM_SHIELD` and never fires |

## Docs / memory updated 2026-05-28

- `[[dusklight-debugging-heuristics]]` anti-pattern #2 expanded with the
  concrete double-`mDoMemCd_ThdInit` example.
- `[[dusklight-debugging-heuristics]]` new anti-pattern #22 for
  `#if PLATFORM_WII || PLATFORM_SHIELD` fall-through (dispatch-table OOB
  and worker-switch no-op).
- `[[dusklight-tv-video-reached]]` "Progress 2026-05-28" section added.
- `CMakeLists.txt:317` — FIXME comment explaining the TARGET_PC trap.
- `platforms/switch/PLAN_TARGET_PC_AUDIT.md` — NEW doc tracking the
  TARGET_PC / WII-SHIELD migration plan (phases 0-4).
- Skill `dusklight-switch-port` anti-pattern #2 expanded.
- `MEMORY.md` index entry refreshed.

## Working-tree state 2026-05-28 (já commitado em 2026-05-29)

`D:\Projects\dusklight` (estava dirty no dia 2026-05-28, commitado em 698f94f26c):
- M `.gitignore` `CMakeLists.txt`
- M `include/d/d_event_lib.h` `include/dusk/audio.h`
- M `libs/JSystem/src/JKernel/JKRAram.cpp` `JKRHeap.cpp`
- M `src/Z2AudioLib/Z2AudioMgr.cpp` `Z2SceneMgr.cpp`
- M `src/d/d_event_lib.cpp` `d_file_select.cpp` `d_s_logo.cpp`
- M `src/dusk/ui/graphics_tuner.cpp` `graphics_tuner.hpp` `prelaunch.cpp` `settings.cpp`
- M `src/m_Do/m_Do_MemCard.cpp` `m_Do_machine.cpp` `m_Do_main.cpp`
- M `extern/aurora` (pointer)
- ?? `platforms/switch/` (Switch build infra)
- ?? `RESUME_TOMORROW.md`

`D:\Projects\dusklight\extern\aurora` (commitado em aurora 9197c9b):
- M `CMakeLists.txt` `cmake/aurora_core.cmake` `cmake/aurora_dvd.cmake`
- M `include/SDL3/SDL.h` `include/aurora/aurora.h`
- M `lib/aurora.cpp` `lib/imgui.hpp` `lib/window_switch.cpp`
- M `lib/rmlui/WebGPURenderInterface.cpp` `lib/webgpu/gpu.cpp`
- M `lib/switch/tracy_stub/tracy/Tracy.hpp`
- ?? `include/SDL3/SDL_audio.h` `include/imgui.h` `include/misc/`
- ?? `lib/switch/tracy_stub/client/` `lib/switch/tracy_stub/common/`

`D:\switch-nvk` (commitado em 8735a1d):
- M `winsys/drm_shim.c` (timeline syncobj B-fix from M-DV-1)
- ?? `dan-re/` Ghidra workspace + scripts

## Cleanup Switch SD (paths que o engine toca, referência permanente)

- `sdmc:/game/config.json` — Dusklight settings (via `SDL_GetPrefPath()` stub que retorna `sdmc:/game/`).
- `sdmc:/game/achievements.json` — achievements.
- `sdmc:/game/texture_replacements/` e `texture_dumps/` — asset override dirs.
- `sdmc:/game/USA/`, `sdmc:/game/logs/` — Dusklight-internal.
- `sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci` — **NOSSO save real** (Aurora memcard emu — descoberto 2026-05-29).
- `sdmc:/switch/dusk/TLoZ - Princesa do Crepusculo (BR).gcm` — disc image.
- `sdmc:/dusklight.log` e `sdmc:/dusk_stderr.log` — nossos logs.
- `sdmc:/atmosphere/crash_reports/` — Atmosphere crashes.
- `romfs:/res/...` — dentro do NRO, fonts/CSS/logo.

**Safe to wipe (testes):**
- `sdmc:/game/` entirely (regenerates with defaults)
- `sdmc:/aurora/USA/Card A/*.gci` (apaga save, prompt aparece de novo)
- `sdmc:/dusklight.log`, `sdmc:/dusk_stderr.log`

**KEEP:**
- `sdmc:/switch/dusklight.nro` (build atual)
- `sdmc:/switch/dusk/TLoZ ...gcm` (disco)
- `sdmc:/atmosphere/` (CFW)

## Key memories to read

- `[[dusklight-tv-video-reached]]` — overall milestone state + checklist.
- `[[dusklight-debugging-heuristics]]` — 22 anti-patterns; #2, #17, #18, #20, #22
  most relevant to file_select scene.
- `[[dusklight-clean-build-fixes]]` — the 6 load-bearing CMake fixes since
  the 2026-05-28 reset.
- `[[dusklight-switch-target-pc-convention]]` — `TARGET_PC` semantics.
- `platforms/switch/PLAN_TARGET_PC_AUDIT.md` — long-term plan for the
  TARGET_PC carve-out.

---

*Última atualização: 2026-05-29. Próxima sessão: começar M-WSI-2 ou TARGET_PC
migration — sua escolha. Histórico anterior preservado em baixo.*
