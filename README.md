# Bomberman — Algoritmos e Programação II

Projeto em C++17 para Windows, com Win32, GDI e GDI+. Desenvolvedores: Diego Silva e Gabriel Bianchessi. Sem engine.

O menu reúne a campanha acadêmica e o **Modo Insano**, que executa o jogo completo de batalha de sprites.

## Compilar e executar

Os executaveis `bomberman.exe` e `modo_insano.exe` estao incluidos no repositorio.
Para jogar no Windows 10/11, baixe a pasta completa e abra `bomberman.exe`;
mantenha `assets/`, `assets_otimizados/` e `SDL2.dll` junto dos executaveis. As bibliotecas
do GCC sao incorporadas ao build, sem exigir MinGW instalado para jogar.

Requisitos: Windows 10/11, PowerShell e GCC/MinGW C++17. O script procura `g++` no PATH e em `C:\msys64\ucrt64\bin\g++.exe`.

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
.\bomberman.exe
```

O build gera `bomberman.exe` e `modo_insano.exe`. Feche instâncias anteriores antes de recompilar. Python e Pillow são usados somente pelos scripts opcionais que regeneram os sprites; não são necessários para jogar.

Assets são carregados uma vez, a partir da pasta do executável. Mantenha `assets/`, `assets_otimizados/` e os dois executáveis juntos. A janela permite redimensionar e maximizar, mantendo a proporção e centralizando o desenho.

## Menu

| Opção | Ação |
| --- | --- |
| 1 | Campanha manual: 1 ou 2 jogadores, teclado/joystick, personagens, dificuldade e fase |
| 2 | Demonstração: a IA joga sozinha |
| 3–6 | Como jogar, dificuldades, pontuação e ranking |
| 7 | Escolher personagens |
| 8 | Seletor de fases |
| 9 | Modo Insano |
| Q / Esc | Sair |

Fluxo de **1 JOGAR**: quantidade de jogadores (1 JOGADOR, 2 AMIGOS ou 2 INIMIGOS) → nome de cada jogador para o ranking (com o TOP 10 ao lado) → quem usa teclado e joystick → personagem de cada jogador → dificuldade → fase. Nas skins: 1–4 ou setas selecionam, Enter confirma P1 e passa a P2, Tab alterna. Esc volta ao menu; M volta uma tela. Na galeria: 1–9, 0 (fase 10) ou setas selecionam e Enter inicia. Cartões também aceitam mouse.

Branco, preto, azul e vermelho têm as mesmas habilidades. A escolha muda aparência, caminhada e poses finais.

## Campanha

### Joystick e dois jogadores

Conectar um controle **nunca** cria um jogador. Em **1 JOGAR** escolha 1 ou 2
jogadores, digite o nome de cada um e, na tela "Quem usa cada controle?", leve cada dispositivo para o
lado de P1, de P2 ou para "sem uso" (setas/A/D movem o teclado; o direcional
do joystick move o joystick; 1/2 sao atalhos; clique tambem funciona):

| Jogadores | Opcoes |
| --- | --- |
| 1 | P1 no teclado **ou** P1 no joystick |
| 2 com joystick | P1 teclado + P2 joystick **ou** P1 joystick + P2 teclado |
| 2 sem joystick | P1 WASD + Espaco, P2 setas + Enter |

Quem fica sozinho no teclado usa WASD ou setas e Espaco ou Enter. No joystick:
direcional ou analogico movem, A ou B plantam bomba; para P1 no joystick,
X chuta, Y teleporta e L/R detonam (rodape, painel de itens e tela final passam
a mostrar esses botoes). Select volta ao menu. Na tela de fim, A/Start tentam de
novo e B/Select vao ao menu, apos o painel aparecer. Nos menus, direcional = setas, A/Start = Enter, B/Select = voltar.
O joystick so age com a janela do jogo em foco. Se ele desconectar durante a
partida, o teclado volta a controlar aquele jogador.

A leitura usa a **SDL2** (`SDL2.dll` oficial 2.30.9, licenca zlib, ver
`README-SDL2.txt`) carregada da pasta do executavel. Ela le controles em modo
Switch/Pro Controller (como o 8BitDo SN30 Pro por USB), Xbox, PlayStation e
outros com mapeamento conhecido. Sem a DLL o jogo ainda abre e usa XInput ou
DirectInput generico; um Pro Controller visto so pelo DirectInput e ignorado,
porque nesse caminho ele gera movimentos e bombas fantasmas.

O menu [5] abre uma aba com todas as regras de pontuacao e um exemplo do bonus.

As oito arenas anteriores foram preservadas. Duas novas adaptam os mapas de Quiz Caravan Cult Q e Langrisser II para o tabuleiro de Bomberman. A campanha segue da fase selecionada até a décima.

| Fase | Arena |
| --- | --- |
| 1 | Casino Azul |
| 2 | Masmorra |
| 3 | Fábrica Neon |
| 4 | Ruínas de Pedra |
| 5 | Cânion Ceedrun |
| 6 | Selva Viva |
| 7 | Caverna Congo |
| 8 | Templo Arcano |
| 9 | Mundo Bomber |
| 10 | Vale Langrisser |

Limpe os inimigos e entre no portal. Nas fases 2, 4, 6, 8 e 10, derrote tambem o chefe para abrir o portal. Facil tem tres inimigos normais, perseguicao limitada e ritmo mais tranquilo; Medio e Dificil tem cinco e sete.

| Tecla | Ação na campanha |
| --- | --- |
| WASD ou setas | Mover; com 2 jogadores no teclado, WASD = P1 e setas = P2 |
| Espaço / Enter | Bomba (com 2 jogadores no teclado: P1 / P2) |
| E | Chutar bomba, após obter item |
| T | Teletransportar, após obter item |
| F | Detonar bombas-relógio |
| R | Reiniciar; após morte, aguarda a animação terminar |
| M | Menu |
| Q / Esc | Sair |

Com dois jogadores há dois modos:

| Modo | Regras |
| --- | --- |
| AMIGOS | A bomba de um não fere o outro; a própria bomba continua ferindo. Vidas, itens e pontos do time; os dois nomes entram no ranking com a pontuação do time. |
| INIMIGOS | Todos contra todos: qualquer bomba fere qualquer jogador. Cada um soma os próprios pontos (+100 por derrubar o rival) e entra no ranking com eles. O Mario ajuda quem o libertar primeiro e também ataca o rival. |

Cada jogador tem seu próprio limite de bombas (a bomba de um não ocupa a vaga do outro). No Fácil, um inimigo solta bombas; no Médio, metade; no Difícil, 75%. A bomba de inimigo tem arco roxo, alcance 2 e fere só os jogadores e o Mario.

Nos dois modos, cada jogador tem 3 vidas por fase, mostradas em corações no topo da tela. Quem é atingido perde uma vida e renasce num lugar seguro, piscando e protegido por 2 segundos, enquanto o outro continua. Quem perde as 3 vidas fica fora até a próxima fase; ao passar pelo portal, os dois voltam com 3 vidas cada. Se os dois perderem as 3 vidas na mesma fase, é fim de jogo. Os itens são compartilhados. Bônus de fase são reiniciados ao trocar/repetir fase. Recompensas usam ícones distintos para fogo, bomba extra, vida, relógio, proteção, atravessar blocos, chute e teleporte. Blocos especiais não mostram pistas do conteúdo.

A morte toca nove poses no local do personagem atingido antes de exibir continuar/game over. O movimento termina cada passo antes da curva; a última direção pressionada tem prioridade. Contato e dano seguem a posição atual dos corpos, sem atingir antecipadamente por uma casa de destino reservada.

O ranking é salvo em `ranking.csv`, ao terminar uma partida manual. Cada nome mantém um único recorde; nomes existentes pedem confirmação antes de jogar. Os nomes são pedidos depois da escolha de 1 ou 2 jogadores (dois nomes diferentes quando jogam dois). Na tela de ranking, setas selecionam, E renomeia e Delete exclui com aviso de perda. Nomes aceitam números, acentos e símbolos (até 24 unidades UTF-16), gravados em UTF-8 com escape CSV. Na tela de nomes, Enter confirma e passa ao próximo campo, setas cima/baixo trocam de campo e Esc volta. O arquivo anterior à primeira alteração fica em `ranking.csv.bak`.

Pontos: +100 por inimigo, +20 por caixa, +30 por item, +1000 por chefe. Andar e plantar bombas não mudam o placar imediatamente. Ao entrar no portal, o bônus de eficiência é `max(0, 500 - movimentos da fase - 5 × bombas da fase)`, contando todas as tentativas da mesma fase. Mario segue as mesmas recompensas por conquistas; maçãs coletadas e acertos isolados não pontuam.

Chefes da campanha:

| Fase | Chefe | Poder | HP |
|---|---|---|---|
| 2 | Golem da Masmorra | Machados e facoes giratorios (Metal Slug) direcionados | 48 |
| 4 | Egg-HVC / Demolidor | Disparos de energia e investida ortogonal com preparacao | 64 |
| 6 | Hornet da Selva | Agulha com retorno, investida e salto com fios animados | 80 |
| 8 | Guardiao Arcano | Avancos e bolas de fogo do Modo Insano | 100 |
| 10 | Bowser | Rajada de fogo, investida no casco, salto e soco com onda de impacto | 112 |

Hornet substitui a Hidra da Selva Viva (fase 6); o Golem da Masmorra permanece na fase 2. As folhas completas foram catalogadas: 243 quadros da Hornet e 389 do Bowser, com corrida, preparação, ataque, salto, recuperação e dano. As ações usam sequências próprias; os atlas também conservam poses e efeitos disponíveis para expansão. Os novos chefes animam a derrota por 1,05 s antes do portal.

A Hornet alterna agulha com fio e retorno, investida terrestre, salto com giro,
investida aérea e guarda. Receber dano na guarda provoca reação, recuperação e
contra-ataque. Cada ação termina antes da próxima. É possível passar por baixo
dela durante o voo; ataques do piso não a atingem quando está elevada.

Em cada fase de chefe, Mario esta escondido em um bloco quebravel comum, sem
marca. Exploda o bloco para liberta-lo. Ele ajuda automaticamente: movimento
ortogonal a 3,57 casas/s, tres vidas, dois segundos de protecao inicial e 1,6 s
apos sofrer dano. Lanca macas em arco a cada 1,05 s, elimina inimigos, detona
bombas, quebra caixas e causa 3 de dano ao chefe. As macas seguem os tempos,
trajetoria, sprites e regras de impacto do estudo; os alvos sao os inimigos da
campanha. Mario coleta macas extras para recarregar. Proteja-o: perder suas tres
vidas causa derrota, como no Insano. Fases impares nao possuem Mario escondido.

## Modo Insano

Executa uma cópia integral do jogo de `estudos_sprite/batalha.cpp`: quatro Bombermans, Mario, Ceedrun, sentinelas, maçãs, projéteis, oito arenas e boss original de 100 HP. Mecânicas e controles permanecem iguais ao estudo; somente o caminho dos assets foi tornado relativo.

Enter alterna White/automático; M alterna Mario/automático; WASD move; Espaço usa bomba/maçã; T alterna arenas; P pausa; R reinicia; L ativa câmera lenta; Esc fecha. Ao fechar o Insano, o menu principal reaparece. Ele não escreve no ranking CSV da campanha.

Origem, hash do fonte e validação: [modo_insano/README.md](modo_insano/README.md).

## Organização e validação

- `bomberman.cpp`: fonte oficial da campanha; matriz 13×15, structs, vetores e sub-rotinas.
- `modo_insano/batalha_referencia.cpp`: cópia preservada do estudo.
- `compilar.ps1`: gera os dois executáveis; fonte portátil do Insano é gerado no build.
- `testes/`: regressões de colisão, fluidez, seleção, assets e combate original do Insano.
- `PROGRESSO_M2.md` e `HANDOFF_CODEX.md`: estado e decisões recentes.
- `DEFESA.md` e `ROTEIRO_DEFESA.md`: materiais de defesa que ainda precisam da revisão completa para M2.

Win32 cuida de janela, teclado, tempo e DPI. GDI apresenta o buffer duplo; GDI+ desenha sprites com alpha e pixel art. Terreno e fontes usam cache. BFS template, explosão recursiva, sobrecargas, parâmetro default, structs e arquivo permanecem na campanha.

Instruções dos testes e geração de previews: [testes/README.md](testes/README.md). Testes isolados não abrem janela, não enviam teclas e não alteram o ranking. O controle real, maximizar e a transição entre janelas devem ser avaliados jogando.

## Créditos de assets

Assets fornecidos para estudo acadêmico de Bomberman/Hudson/Konami. Folhas trazem créditos de Cyber Wolf, Roviozza, raccoon sam (2012), JCGBR e Boo; Mario por ItsMeRen. Originais foram preservados. Ícones e recortes derivados mantêm esses créditos.
