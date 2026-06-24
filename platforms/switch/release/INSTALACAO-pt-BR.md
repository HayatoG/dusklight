# Dusklight — Nintendo Switch — Instruções de Instalação (pt-BR)

Build homebrew do **Dusklight** (reimplementação open-source de *The Legend of Zelda: Twilight
Princess*) rodando em Switch via nossa stack gráfica NVK (Vulkan). O `.nro` é **auto-suficiente**:
ele já vem com um **cache de shaders pré-aquecido** embutido e escreve tudo na pasta de dados no
primeiro boot — então praticamente não há "pop-in" de texturas/modelos e a instalação é quase só o
arquivo.

## Requisitos
1. Um Switch com **CFW (Atmosphère)** e um **homebrew launcher** (hbmenu) ou Sphaira.
2. Sua **própria cópia dumpada** do disco de GameCube do Twilight Princess (NÃO incluída — você
   precisa fornecer). Versão US (GZ2E01), ~1,46 GB.
   - **Qualquer formato/nome serve:** `.gcm`, `.iso`, `.rvz`, `.ciso`, `.gcz`, `.wbfs`... — não
     precisa renomear. O Dusklight detecta sozinho.
   - Dica: `.gcm`/`.iso` cru é o mais rápido; comprimidos (ex. `.rvz`) funcionam mas têm custo de
     descompressão durante o jogo.

## Instalação — basicamente só o `.nro`
1. Copie `dusklight.nro` para `sdmc:/switch/`.
2. Abra o `dusklight.nro` pelo hbmenu **uma vez**. Ele cria a pasta `sdmc:/TwilitRealm/Dusklight/`
   e grava o cache de shaders pré-aquecido lá dentro **automaticamente**.
3. Coloque o seu dump do TP (qualquer nome/formato) em `sdmc:/TwilitRealm/Dusklight/`.
4. Abra de novo — o Dusklight detecta o disco sozinho e roda.

> **Não precisa** montar pasta nenhuma à mão, nem `data_location.json`, nem pasta `game/`. O NRO já
> vem com tudo embutido e se vira no primeiro boot.

> **Tudo numa pasta só:** configurações, saves, caches e logs vivem todos em
> `sdmc:/TwilitRealm/Dusklight/`. As únicas exceções são o próprio `.nro` em `sdmc:/switch/` (exigido
> pelo hbmenu) e os crash reports da CFW em `sdmc:/atmosphere/`.

## O que funciona neste build
- **Áudio** do jogo (JAudio2/DSP → audren do libnx).
- **Giroscópio ligado por padrão** (mira de arco/funda/clawshot/etc. e no modo *look*). Ajuste a
  sensibilidade ou desligue em **Settings → Gyro**.
- **Menu in-game**: aperte **MINUS (−)** durante o jogo (warp, configurações, etc.).
- **Detecção automática do disco** na pasta (qualquer nome/formato).
- **Notificação de compilação de pipeline** + toasts de conquistas.
- **Trocar Resolução Interna / proporção (4:3)** no menu **não trava** (launcher e in-game).
- **Cache de shaders pré-aquecido** → pouco ou nenhum pop-in na 1ª vez.

## Atualizando de uma versão antiga
- Seu **save é preservado**: ele já ficava em `sdmc:/TwilitRealm/Dusklight/USA/...`.
- Se você jogou um build BEM antigo (que salvava em `sdmc:/aurora/USA/Card A/`), o Dusklight
  **importa automaticamente** esse save pra pasta nova no primeiro boot (faz uma cópia, não apaga o
  antigo).
- O `sdmc:/game/data_location.json` de pacotes antigos vira inútil e pode ser apagado (é ignorado).

## Observações
- Launcher/menus rodam a 60fps. In-game fica ~30fps (taxa nativa do GameCube). A interpolação de
  quadros existe mas ainda não gera quadros extras no Switch (work-in-progress).
- Um contador de FPS vem ligado por padrão.
- **Auto-reparo por arquivo:** no boot, o NRO restaura do romfs **qualquer arquivo de seed que
  estiver faltando** — `config.json`, `dawn_cache.db` ou `pipeline_cache.db` — individualmente. Não
  precisa a pasta estar vazia: se você apagar só o `config.json`, ele volta; se apagar só um `.db`,
  só aquele volta. Arquivos que você já tem **nunca** são sobrescritos.
- O cache é leitura-e-escrita: shaders faltantes compilam uma vez e ficam salvos — as próximas
  sessões ficam ainda mais suaves. Apagar os `.db` não quebra nada (recompila sob demanda, com
  pop-in até reaquecer; e o seed é restaurado no próximo boot como acima).
- Os saves usam o formato de memory-card do GameCube (`.gci`).

## Aviso legal
Este pacote NÃO contém o jogo. Você precisa dumpar sua própria cópia do Twilight Princess de
GameCube que você possui legalmente.
