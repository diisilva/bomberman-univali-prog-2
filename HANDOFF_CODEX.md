## Organizacao e publicacao - 2026-10-09

Usuario autorizou commit/push e pediu incluir os executaveis.
- Mantidos campanha, Insano, testes-fonte, documentos academicos, scripts de
  build/recorte, 35 assets usados e atlas compacto do estudo.
- 97 itens pessoais/temporarios/antigos ou sprites nao usados foram movidos,
  sem apagar, para pasta externa bomberman_arquivo_20261009_191258 em
  D:\algoritmos_prog_2. A pasta tem manifesto dos movimentos concluidos.
- .gitignore admite somente bomberman.exe e modo_insano.exe entre os binarios.
- Build principal agora usa -static, como o Insano: bibliotecas MinGW
  incorporadas. Imports verificados; restam DLLs do Windows.
- Remoto main tinha historico M1 divergente. A versao M2 atual substitui os
  arquivos antigos; a correcao remota de bomba bloqueando reentrada foi
  incorporada e testada. Historicos preservados por merge, sem force push.
- Verificacoes de colisoes e polimento passaram apos a correcao e build;
  assets requeridos conferidos antes de arquivar. Nenhuma janela automatizada.

## Facoes de Metal Slug incorporados

Golem alterna facoes e machados a cada ataque. Cinco poses originais de arma
isolada da folha Ajirabian Soldier, recortadas em memoria com fundo transparente
e centralizadas em quadros 40x40; animacao usa as poses e meia-volta adicional.
Conservados velocidade, preparacao, colisao e impacto do arremesso. Testes cobrem
alternancia e transparencia; previews de ataque da fase 2 agora mostram facoes.

## Atualizacao - Poderes com sprites, projeteis e investida (2026-10-08)

Substitui os ataques antigos por casas coloridas nas fases 2, 4 e 6.
- Golem: lanca machados recortados da arma de FFV, com rotacao, sombra, voo
  direcionado a 2.8 casas/s e impacto animado ao colidir.
- Egg-HVC: energia azul animada a 3.6 casas/s e investida ortogonal de ate
  quatro casas a 4.8 casas/s. Preparacao de 1 s, setas na rota e pose de carga
  da folha original; durante a corrida reserva somente seu corpo atual para
  permitir esquiva. Uma investida pendente aguarda terminar o passo normal.
- Hidra: tres projeteis de veneno por alvo, espalhados em leque, a 3 casas/s;
  impacto deixa poca de 2.2 s com sprites verdes animados e colisao circular.
- Novos disparos mostram preparacao de 700 ms, rastro e impactos em sprites.
  Colisao em subpassos respeita paredes, corpos atuais, P2 e Mario. Bonus de
  caixas atingidas continuam virando icones; bloco secreto do Mario e preservado.
- Ataques usam recortes compactos das folhas FFV, EGG-HVC e BombsExplosionsv2.
  Nao ha novos assets gerados por IA nem dependencia de pastas externas.
- Fogo da fase 8 e fonte do Modo Insano conservados. Mario aliado conservado.
- Testes incluem voo/aviso/impacto, paredes, itens, poca, carga/esquiva, P2,
  Mario e regressao de dano prematuro. Previews offscreen em
  testes/preview_ataque_fase_{2,4,6}_{0..4}.png e preview_veneno_poca.png.

## Atualizacao 2026-10-08 - Chefes a cada duas fases e Mario surpresa

Esta secao substitui a regra antiga de boss apenas na fase final.
- Fases 2/4/6/8: Golem (terremoto, 48 HP), Egg-HVC (laser em cruz, 64 HP),
  Hidra (veneno, 80 HP), Guardiao (fogo original do Insano, 100 HP).
- Todos aparecem apos eliminar os inimigos da fase; o portal aguarda sua morte.
- Corpo 3x2 com reserva do percurso, avanco ortogonal a 1.5 casas/s e pausa 4 s;
  abre passagem interior, preserva bordas, evita jogadores, aliado e bombas.
- Fase 8: primeiro disparo apos 5 s, seguintes a cada 4.5 s; fogo direcionado
  a cada participante vivo, velocidade 3.2 casas/s, duracao 4 s, fire_2.png.
- Bombas causam 4 de dano com imunidade 700 ms; macas causam 3, como no estudo.
- Recortes nativos compactos das folhas FFV e EGG-HVC fornecidas pelo usuario.
- Mario escondido em bloco fragil comum, so nas fases pares, sem pista visual.
  Libertacao por explosao; chefe e fogos preservam esse bloco ate o resgate.
- Mario aliado autonomo adaptado do Actor do estudo: velocidade 4.2, decisao
  40 ms, animacao por distancia, 3 HP, invulnerabilidade inicial 2 s e apos
  dano 1.6 s. Macas em arco, cooldown 1.05 s, voo limitado 0.65-1.45 s, pose
  0.28 s. Mata inimigos, quebra caixas, ativa bombas e acerta boss. Nao ataca
  P1/P2. Tres ferimentos derrotam a equipe, como a derrota do Mario no estudo.
- Fases comuns nao receberam chefes nem aliado. Fonte do Insano permanece intacto.
- Verificacao sem janelas, teclado simulado, musica ou ranking: suites de
  colisoes, boss, Mario, polimento (PNG offscreen) e original Insano.
- Executavel atualizado: bomberman.exe. Nao houve commit.

# Handoff para o Codex — Bomberman M2

## ESTADO MAIS RECENTE — Oito arenas, morte na arena e Modo Insano

Esta secao substitui as pendencias de arenas, tamanho e morte das secoes antigas.
Usuario pediu concluir o planejamento e acrescentou um Modo Insano que fosse
exatamente o jogo completo do estudo, incluindo toda a mecanica do boss.

- Campanha ampliada para oito fases. Ordem preserva as antigas: Casino Azul,
  Masmorra, Fabrica Neon; depois Ruinas de Pedra, Canion Ceedrun, Selva Viva,
  Laboratorio e Templo Arcano. Novos temas usam `assets/arenas_estudo.png`, copia
  do atlas compacto do estudo; originais anteriores continuam nas tres primeiras.
- Fluxo manual/automatico: dificuldade > personagem > galeria de fases > iniciar.
  Galeria permite escolher fase inicial (1-8 ou setas, Enter inicia); a campanha
  segue ate a oitava, com inimigo especial e boss academico nela. Menu 7 abre
  personagens, 8 abre fases. Selecao por mouse tambem implementada.
- Personagens e inimigos agora 45x68 para celulas 52, equivalentes a 40x60 em
  celulas 46 do estudo. Pes ancorados, sombra e ordenacao por profundidade;
  boss visual ampliado para 110. Caixa de contato permanece pequena e independente
  do sprite. A correcao de morte a distancia continua coberta por testes.
- Morte completa de nove poses em todas as skins, visivel no local atingido na
  arena por 1170 ms. So entao aparece painel de continuar/game over. P2 so anima
  se foi ele atingido; R aguarda a sequencia. Script de assets adapta paletas do
  atlas v2 de morte, em vez do Knock-Out curto que existia antes.
- Fluidez: ultima direcao pressionada tem prioridade; curva bloqueada permite
  continuar pela outra tecla segurada; sem pausa de 280 ms ao bater em parede.
  Caminhada nao pisca pose parada na chegada de cada passo. Itens e portais so
  sao consumidos ao chegar, e o bot busca portal aberto e considera o boss alvo.

### Modo Insano — original completo

- `D:\algoritmos_prog_2\batalha.cpp` informado pelo usuario NAO existe. Fonte usado:
  `D:\algoritmos_prog_2\estudos_sprite\batalha.cpp`.
- `modo_insano/batalha_referencia.cpp` e copia byte a byte, SHA256
  `FCB858CA97A9B3CF09E32587A7E9163E40C3310E7F9DC9F577C73B8717F065D2`.
- Build gera `batalha_portavel.cpp`, alterando APENAS a raiz dos assets para a
  pasta do executavel. O jogo inteiro, boss original 100 HP, Mario, macas,
  Ceedrun, sentinelas, ataques, controles, ritmo e arenas sao os do estudo.
- Menu 9 inicia `modo_insano.exe` via CreateProcess, oculta a janela da campanha
  e para a musica. Timer detecta encerramento, mostra menu e retoma musica.
  Campanha conserva regras academicas; Insano e independente, sem ranking CSV.
- `compilar.ps1` agora gera os dois executaveis. `assets_otimizados/arenas.png`
  acompanha o Insano. Nao mover assets apenas para pasta de estudo externa.
- Testes do estudo copiados em `testes/verificar_insano.cpp`, mudando so include.
  Passaram: boss movimento/colisao/dano, combate, ritmo, controle, reinicio, Mario.
- Compilacao separada do fonte original intacto e Insano portavel produziram
  snapshots IDENTICOS pixel a pixel (RGB), 8 segundos/Fabrica Neon. Hash do fonte
  tambem identico. Ver `modo_insano/README.md`.

### Validacao e limites

Suites de colisoes e polimento passaram apos as mudancas; compilacoes com avisos
habilitados limpas. Previews isolados conferidos: menu, galeria, skins, oito
arenas, morte, vitoria e Insano. `testes/preview_animacao_morte.gif` mostra a
sequencia; previews sao ignorados no git. Nenhuma janela ao vivo, SendKeys ou
captura da sessao do usuario foi usada. A abertura/retorno real do Insano pelo
menu, mouse, maximizar e sensacao de controle precisam da avaliacao jogando.
README atualizado; documentos antigos de defesa ainda precisam de revisao M2.
Nenhum commit. Alteracoes locais anteriores foram preservadas.

## Retomada — Correcao prioritaria de morte a distancia

Usuario retomou relatando morte com inimigo a duas casas horizontais/verticais
ou uma casa em cada eixo. Reproduzido antes da correcao em teste isolado:
dois personagens caminhando para o mesmo destino eram considerados em contato
antes de seus sprites se encontrarem.

Corrigido em `bomberman.cpp`: a posicao inteira continua reservando destinos para
matriz/BFS/obstaculos; contato e dano usam as posicoes atuais interpoladas.
Caixa corporal central de meia celula (`MEIA_CAIXA_PERSONAGEM=.25f`); nao depende
do zoom, tamanho do sprite, sombra ou capacete. Foram removidos gatilhos de morte
ao apenas escolher a casa do jogador. Contato final e verificado depois do
deslocamento, inclusive em cruzamentos com destinos diferentes. Chamas e boss
tambem seguem a posicao atual. P2 so causa dano quando ativo no manual cooperativo.

`testes/verificar_colisoes.cpp` reproduzia a falha antes do patch e passou depois;
cobre separacao horizontal/vertical/diagonal, casas vizinhas, cruzamentos, boss,
chamas, imunidade, P2 e uma unica vida descontada por morte. Suite anterior de
polimento tambem passou. Build final limpo, sem warnings. Nenhum commit nem
automacao de teclado/janela. A avaliacao manual do usuario continua importante.

Os pedidos de aumentar os bonecos e importar arenas/seletor seguem pendentes;
a morte a distancia foi tratada primeiro por ser um erro grave de gameplay.

## LEIA PRIMEIRO — Pausa solicitada para continuar amanha (2026-10-08)

Esta secao e o estado mais recente. As secoes antigas abaixo sao historico, nao
uma lista atual de pendencias. O usuario pediu expressamente um handoff e depois
acrescentou uma prioridade visual. Nao houve commit; nao descarte mudancas locais.

### Pedido prioritario: tamanho dos personagens

Mensagem do usuario: "coloque pra verificar no handoof pq os boniqueinhos daqui
sao pequenninhos e os de la do estudo sprite estao no tamanho perfeito".

**Investigar e corrigir antes de declarar o polimento concluido.** Comparar
diretamente o codigo atualizado e os prints de `D:\algoritmos_prog_2\estudos_sprite`
com nosso jogo. Nao basta aumentar a janela: precisamos aproximar a proporcao
PERSONAGEM / CELULA do estudo.

- Aqui: `TAMANHO_CELULA=52`; `desenharQuadroNaCelula()` reserva margem 4 dos dois
  lados e ajusta recorte 16x24 a uma caixa de 44x44. Resultado nominal aproximado
  29x44, com a figura visivel potencialmente menor por margens do proprio atlas.
- No estudo: `draw()` desenha atores com dimensoes e ancoragem diferentes; revisar
  todo o trecho depois de `int step=walkFrame(a)` e a funcao `sprite()`, alem da
  constante `T`. As sentinelas usam altura 56, Mario 48 e o boss 110, conforme
  trechos ja lidos. Conferir os valores exatos dos Bombermans, nao inferir.
- Verificar caixa real dos pixels opacos, escala pixel art, alinhamento pelos
  pes, sombra, margem, sobreposicao entre casas e ordenacao por profundidade.
  Ajustar a renderizacao sem aumentar hitbox nem mudar colisoes da matriz.
- Se a melhor aparencia exigir personagem ultrapassando visualmente a celula,
  explicar e adaptar a diretriz antiga de conter o sprite: a preferencia mais
  recente do usuario e o tamanho/proporcao do estudo, mantendo gameplay.
- Validar P1, P2, quatro skins, inimigos e boss; nao so a montagem grande do menu.
  Gerar previews isolados comparaveis em escala equivalente; nao roubar foco,
  nao usar SendKeys nem capturar a sessao ao vivo do usuario.

### Proximo trabalho autorizado: todas as arenas e seletor de fases

Pedido literal: "mantem todas as fase sdaqui e tras todas de la e no menu depois
que se esolhe o eprsonagenm adiciona seletor de fases, dai fica top".

**AINDA NAO IMPLEMENTADO.** Manter as tres fases atuais (Casino Azul, Masmorra,
Fabrica Neon) e trazer todas as arenas restantes do estudo, com galeria/seletor
de fases depois da escolha de personagem. Nao apagar temas atuais nem duplicar
os mesmos temas sem necessidade.

O estudo tem oito temas (`themeNames` em `batalha.cpp`):
1. Fabrica Neon
2. Casino Azul
3. Ruinas de Pedra
4. Canion Ceedrun
5. Masmorra
6. Selva Viva
7. Laboratorio
8. Templo Arcano

Fontes uteis: `sourceTiles()`, `loadCompactAtlas()`, `prepareArenas()` e
`assets_otimizados/arenas.png` no estudo. O atlas compacto ja evita o custo das
folhas gigantes; conferir equivalencia visual antes de copiar/adaptar.

Foi enviada uma pergunta assincrona, SEM RESPOSTA ate a pausa: o seletor deve
escolher onde comecar uma campanha com todas as fases, ou apenas o cenario de
uma campanha que continua com tres fases? Nao considerar o tempo decorrido uma
resposta. Retomar essa decisao com o usuario se continuar indefinida, enquanto
se adianta a importacao dos assets. Em particular, nao mudar `TOTAL_FASES`, boss
final, progressao e ranking por suposicao silenciosa.

### O que ja foi concluido nesta sessao

- Polimento descrito em "Retomada Codex" de `PROGRESSO_M2.md`: passos ortogonais
  completos antes de virar, ciclo de caminhada por distancia, velocidade menor,
  Facil menos agressivo, bomba pulsante com sombra/arco, brilho de explosao,
  menu/HUD/painel lateral e animacoes de derrota/vitoria.
- Recompensas agora usam `assets/itens_especiais.png`, oito icones distintos
  recortados de `icons_blocos.png`, com complementos pequenos para vida,
  bomba-relogio e atravessar. So o fallback sem asset continua usando letras.
  `preparar_personagens_itens.py` reconstrui esses assets e as quatro skins.
- Menu tem opcao **7 — Escolher personagens**, acessivel por teclado/mouse.
  Quatro skins: branco, preto, azul e vermelho. 1–4 selecionam; TAB troca alvo
  P1/P2; Enter/M/Esc volta. Escolhas permanecem ao iniciar/reiniciar a partida.
  P2 e distinto por padrao (preto); poses finais seguem a skin escolhida.
- Janela agora usa `WS_OVERLAPPEDWINDOW`: maximizar e redimensionar habilitados.
  `ajustarAreaDesenho()` preserva proporcao, centraliza com margens e invalida
  caches quando a escala muda. Mouse desconta as margens. Minimo 640x520.
  A area de desenho de referencia continua 1080x852, mapa 13x15 com celulas 52.
- Selecao de personagem ainda e uma tela independente que volta ao menu;
  conectar ao futuro seletor de fases conforme o pedido mais recente.
- Nao implementamos Mario. Ele continua depois desses ajustes.

### Build e verificacoes mais recentes

Ultimo `bomberman.exe` recompilado com sucesso apos icones, skins e maximizar:
`powershell -ExecutionPolicy Bypass -File .\compilar.ps1`, sem warnings.

Teste isolado (sem janela, musica, teclado ou escrita no ranking):
```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_polimento.cpp -o verificacao_polimento.exe -lgdiplus -lwinmm -lgdi32
.\verificacao_polimento.exe
```
Passou: curvas, espera dos inimigos, caminhada, modo automatico/reinicio,
assets, selecao P1/P2 por teclado, ajuste da area a 1920x1080 sem distorcao.
Maximizar e interacao real pelo mouse ainda nao foram validados em janela real.

Previews locais, ignorados pelo git e regenerados pelo teste:
- `testes/preview_menu.png`
- `testes/preview_personagens.png`
- `testes/preview_recompensas.png`
- `testes/preview_fase_1.png`, `preview_fase_2.png`, `preview_fase_3.png`
- `testes/preview_dificuldade.png`, `preview_partida.png`, `preview_boss.png`
- `testes/preview_derrota.png`, `preview_vitoria.png`

Esses previews mostram layout e recortes; nao provam que tamanho dos bonecos
ou sensacao de controle agradam ao usuario. O feedback atual e que continuam
pequenos e precisam se aproximar do estudo.

### Cuidados ao retomar

- O repositorio ja tinha varias alteracoes e remocoes em `legado/` antes desta
  sessao; nao restaurar, limpar ou fazer commit por conta propria.
- Fonte oficial unico: `bomberman.cpp`. Preservar matriz, structs, vetores,
  BFS template, explosao recursiva e demais criterios academicos.
- Documentacao README/DEFESA/ROTEIRO ainda tem conteudo antigo de M1. O log e
  este handoff sao as referencias atuais; atualizar documentacao ao fechar M2.
- Pequena limpeza futura: `spriteJogadorDirecional` ainda e carregado embora
  jogadores ja usem `personagens[]`; atlas branco de poses tambem fica redundante
  porque o branco usa os atlas v2. Remover apenas depois de conferir referencias.
- Comentarios e identificadores em portugues, comentarios do codigo sem acentos.
- Nenhuma necessidade de plugin ou engine; continuar C++/Win32/GDI+ simples.

---

> Atualizacao Codex (2026-10-08): o polimento abaixo foi retomado e implementado.
> Leia primeiro "Retomada Codex — Polimento da Fase 8" em `PROGRESSO_M2.md`.
> Build limpo e testes isolados passaram; menu, HUD, animacoes finais, bomba e
> movimento foram atualizados. Falta avaliacao manual do usuario sobre ritmo e
> dificuldade. O texto abaixo preserva o contexto ANTERIOR a essa retomada.

Documento de transferência de contexto. Fui eu (Claude) quem implementou tudo até aqui; o usuário
pediu pra passar o bastão pro Codex continuar a partir deste ponto. Leia isto, depois
`PLANO_M2.md` (plano completo com todas as decisões de design) e `PROGRESSO_M2.md` (log fase por
fase, com testes e aprendizados — a última entrada, "Fase 8 (continuação) — Fluidez, velocidade e
dificuldade", é o trabalho mais recente e ainda **EM ANDAMENTO**).

## Estado do projeto

Trabalho M2 (segunda entrega) de Bomberman pra disciplina de Algoritmos e Programação II da
UNIVALI, baseado em `instruction-bomberman/T1 - Bomberman - 2025-part2.pdf`. Único arquivo-fonte
real: `bomberman.cpp` (Win32 + GDI+, sem engine). Compila com `.\compilar.ps1`
(`g++ -std=c++17 -O2 -Wall -Wextra -pedantic bomberman.cpp -o bomberman.exe -lgdiplus -lwinmm -mwindows`).

**Tudo que vale nota no PDF está implementado e testado (Fases 0-7 do `PROGRESSO_M2.md`):** menu,
3 dificuldades, 3 fases, portal, inimigo especial, boss, os 6 itens obrigatórios, bombas múltiplas,
explosão recursiva atravessando blocos, vidas/game over, pontuação calculada de verdade, ranking em
arquivo, segundo jogador cooperativo, música procedural, e os 6 critérios técnicos (arquivo,
parâmetro default, sobrecarga, struct, template, recursividade — auditoria completa na Fase 6 do
`PROGRESSO_M2.md`). **Não mexa nessas regras sem necessidade** — é trabalho que já foi validado.

O que está rolando agora é **polimento visual (Fase 8)**, puramente cosmético, sem valor de nota
direto no PDF, mas que o usuário se importa bastante (ver pedidos recentes abaixo).

## Trabalho em andamento AGORA MESMO (não termine de revisar, continue)

O código **compila limpo** no estado atual, mas a última leva de mudanças (interpolação visual)
**não foi testada jogando de verdade** — só revisão de código. Antes de confiar nela, jogue e
confira:

1. Rodei `powershell -ExecutionPolicy Bypass -File .\compilar.ps1` com sucesso depois de cada
   edição. Último build funcionando.
2. **Posição visual separada da posição lógica** foi adicionada (`Inimigo.visualL/visualC`,
   `Boss.visualL/visualC`, globais `jogadorVisualL/C`, `jogador2VisualL/C`), com `avancarVisual()`
   deslizando suavemente até a posição lógica (`VELOCIDADE_VISUAL_CELS_SEG=4.3` células/seg),
   chamada em `atualizar()`. As funções de desenho (`desenharQuadroNaCelula`,
   `desenharSpriteNaCelula`) passaram a receber `float linha, coluna`. **Preciso que você confirme
   visualmente que ficou fluido** (jogando ou, se tiver uma forma de capturar o jogo rodando, que
   fosse diferente da minha restrição abaixo).
3. **IMPORTANTE — restrição que eu segui a sessão inteira:** não uso automação de teclado/captura
   de tela ao vivo (SendKeys/keybd_event/screenshot da janela) pra testar este projeto, porque o
   usuário usa o mesmo computador ao mesmo tempo (navegando, etc.) e isso já causou confusão real
   uma vez (um "screenshot do jogo" capturou a janela do navegador dele por engano — ver Fase 1 do
   `PROGRESSO_M2.md`). Se você (Codex) tiver uma forma mais isolada/segura de testar (sandbox
   próprio, VM, etc.) sinta-se livre, mas **se for rodar no mesmo ambiente do usuário, pergunte
   antes ou evite** — não quero que você repita esse problema.

## Pedidos do usuário ainda não atendidos (mensagem mais recente, literal)

Resumindo o que ele disse, na ordem que mencionou:

1. **"a versão atual ainda está bem distante em termos de animação, movimentação fluidez do que
   está em `D:\algoritmos_prog_2\estudos_sprite`, bomba lá tá muito mais bonita e animada,
   movimentação também"** — parcialmente atacado (interpolação visual, bomba/explosão animadas já
   existem desde antes desta leva). Pode precisar de mais polimento depois de testar ao vivo.
2. **"o menu daqui está horrível, tem que ser todo repaginado, pode até fazer uma montagem de
   imagens pro menu com os sprites, etc, a tela de lá é bem maior que a nossa"** — **NÃO
   COMEÇADO**. O usuário mandou 4 capturas de tela de referência em
   `D:\algoritmos_prog_2\estudos_sprite\prints\` (arquivos `Captura de tela 2026-10-08
   211837.png`, `...211906.png`, `...211948.png`, `...212019.png`) — eu não cheguei a abrir essas
   imagens, só listei o diretório. **Primeira coisa a fazer: veja essas 4 capturas.** A janela de
   referência (`estudos_sprite/batalha.cpp`) abre em 1080×744; a nossa é 780×852
   (`LARGURA_JOGO`/`ALTURA_JOGO` em `bomberman.cpp`, calculados de `TAMANHO_CELULA=52`,
   `LINHAS=13`, `COLUNAS=15`, `ALTURA_CABECALHO=104`, `ALTURA_RODAPE=72`). Aumentar a janela ou
   adicionar um painel lateral de informações (como o do estudo, que tem foto do boss, feed de
   eventos, placar) é uma mudança de layout maior — avalie o impacto antes de mexer.
3. **"estude de novo o que tem lá e traga pra cá"** — a pasta `estudos_sprite` foi atualizada pelo
   usuário várias vezes durante a sessão. Ela tem um documento muito bom e atualizado,
   `estudos_sprite/EXPLICACAO_SPRITES_CPP.md`, que explica em português simples todas as técnicas
   (sprite sheets, animação por quadro vs. distância percorrida, movimento suave com
   `moveSpeed=4.2`, cache de terreno, etc.) — **leia esse arquivo primeiro**, é mais rápido que
   vasculhar `estudos_sprite/batalha.cpp` (43KB) direto. Só recorra ao `.cpp` quando precisar de
   coordenadas/fórmulas exatas que o `.md` não detalha.
4. **"nosso jogo no nível fácil está muito difícil"** — parcialmente atacado (reduzi
   `CHANCE_PERSEGUICAO_FACIL` 65→40 e `DISTANCIA_PERSEGUICAO` 6→5, e removi a rajada de 1-3
   células dos inimigos). Não testado jogando. Pode precisar de mais ajuste.
5. **"os inimigos está andando na diagonal, não acho isso legal"** — minha hipótese (não
   confirmada jogando) é a combinação de rajada de múltiplas células por decisão + mudança de
   direção no meio da rajada, já que o movimento é sempre eixo-único no código
   (`moverIndividualAleatorio`/`moverIndividualPerseguicao` só se movem em `{-1,0},{1,0},{0,-1},
   {0,1}`, nunca diagonal de verdade). Removi a rajada (agora sempre 1 célula por decisão). **Se
   o problema persistir depois de testar, investigue mais a fundo** — pode ser outra causa que eu
   não enxerguei.
6. **"no geral nosso jogo está muito rápido e o de lá mais fluido numa velocidade mais
   agradável"** — atacado via `INTERVALO_MOVIMENTO_MS` 105→230ms e a interpolação visual nova.
   Teste e ajuste `VELOCIDADE_VISUAL_CELS_SEG`/`INTERVALO_MOVIMENTO_MS` se ainda não estiver bom —
   são as duas constantes que controlam o ritmo.
7. **"aqui tem círculos nas paredes especiais indicando que há algo lá, deveria ser surpresa"** —
   **FEITO**. Removido o círculo branco de `PAREDE_BONUS` em `desenharTerrenoCelula`, e a cor de
   fallback (sem textura) foi igualada à de `PAREDE_FRAGIL`. Bloco bônus agora é visualmente
   idêntico a um bloco frágil comum.
8. **"não tem animação de quando a gente morre ou quando zeramos o jogo e dá pra pegar isso nos
   sprites"** — **NÃO COMEÇADO**. `assets/personagem_principal_sprite_v2.png` tem poses de
   vitória marcadas como `loop` e uma sequência de derrota/desmaio clara (identificado na Fase 0 do
   `PROGRESSO_M2.md`, nunca efetivamente fatiado/usado). A tela de vitória (`estado == VITORIA`) e
   a tela de Game Over (`estado == FIM_DE_JOGO`) hoje são só texto estático em `desenharRodape()`.

## Depois disso: o Mario ajudante

O usuário mencionou o ajudante Mario (ver seção "Extras opcionais" do `PLANO_M2.md`) como próximo
passo depois dos ajustes de fluidez/menu/animação acima — ele disse explicitamente "antes de ir
pro Mario revise lá de novo", ou seja, **essa lista de pontos acima vem primeiro**, o Mario é
depois e é opcional (não vale nota, é só diversão/extra).

## Como estudar `estudos_sprite` com eficiência

- `estudos_sprite/EXPLICACAO_SPRITES_CPP.md` — leia primeiro, é a explicação em português já
  escrita pelo usuário/outra sessão, cobre tudo: bibliotecas usadas, animação por sprite sheet,
  mapeamento direção→linha da folha, troca de quadro por distância percorrida (não por tempo),
  movimento gradual com posição visual separada da lógica, duplo buffer, cache de cenário, e as
  estruturas de dados usadas.
- `estudos_sprite/batalha.cpp` — código-fonte real, 43KB. Útil pra pegar coordenadas/fórmulas
  exatas de recorte quando o `.md` não é específico o suficiente. **Não é código deste projeto —
  é só uma referência/estudo separado, nunca copie trechos inteiros sem adaptar; o usuário foi
  explícito que isso é só inspiração, não deve mudar as regras do nosso jogo.**
- `estudos_sprite/prints/` — as 4 capturas de tela de referência pro menu/HUD (ainda não vistas
  por mim).
- Esse diretório **fica fora do repositório do jogo** (é `D:\algoritmos_prog_2\estudos_sprite`,
  não dentro de `D:\algoritmos_prog_2\bomberman-univali-prog-2`) — não precisa (e não deve) ser
  commitado ou versionado junto com o jogo.

## Convenções que segui (pra manter consistência)

- **Protocolo de teste:** compilar limpo (`-Wall -Wextra -pedantic`, sem warnings) + revisão de
  código deliberada, célula por célula/linha por linha nas partes sensíveis. Sem automação de tela
  ao vivo (ver acima). Registrar cada fase em `PROGRESSO_M2.md` com "o que foi feito" / "testes
  feitos" / "aprendizados".
- **Comentários em português, sem acento** no código (o resto do arquivo já segue esse padrão —
  `nao` em vez de `não`, etc.) — mantive isso em tudo que escrevi.
- Nomes de função/variável em português, no mesmo estilo do resto do arquivo.
- Para assets grandes (multi-megapixel), sempre pré-recorte uma vez e descarte a imagem gigante —
  ver `prepararTemaFase2Compacto()` como exemplo (evitou um travamento de 3-4s medido no próprio
  estudo do usuário).
- Removo código morto imediatamente quando um sprite/campo antigo fica órfão (não deixo "por via
  das dúvidas") — ver remoção de `spriteJogador`/`spriteInimigo`/`spriteBomba` estáticos ao longo
  da Fase 8.

## Arquivos-chave

- `bomberman.cpp` — único fonte do jogo.
- `PLANO_M2.md` — plano completo, todas as decisões de design (algumas específicas: boss só na
  última fase, vidas compartilhadas no 2P, pontuação compartilhada no 2P — decisão tomada por mim
  por custo de implementação, avisada ao usuário).
- `PROGRESSO_M2.md` — log fase por fase, leia a última entrada antes de continuar.
- `compilar.ps1` — script de build.
- `ranking.csv` — gerado em runtime ao lado do executável, não versionado (está no `.gitignore`).

Boa sorte! Qualquer dúvida sobre uma decisão específica, o "porquê" quase sempre está comentado
no código ao lado da mudança, ou documentado no `PROGRESSO_M2.md`.
