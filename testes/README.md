# Verificacao do polimento

Execute na raiz do projeto, com MinGW/G++ no PATH:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_polimento.cpp -o verificacao_polimento.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_polimento.exe
```

O executavel deve ficar na raiz para localizar `assets/`. O teste nao abre janela,
nao envia teclas, nao toca musica e nao escreve no ranking. Usa GDI+ em memoria
para verificar recortes e exportar os previews `testes/preview_*.png`.

Verifica curvas sem cortar caminho, inimigos esperando o fim do passo, poses de
caminhada, assets finais, tres inimigos no Facil e movimento/reinicio automaticos.
Os previews permitem revisar layout; nao substituem avaliar o controle jogando.

Para reconstruir os dois atlas finais a partir do original (Python e Pillow):

```powershell
python preparar_animacoes.py
```

Os PNGs finais ja estao em `assets/`, portanto Python nao e necessario para jogar.

O teste de polimento tambem percorre o fluxo dificuldade > personagem > fase,
confere dez arenas, chefe na fase final, nova tecla com prioridade nas curvas,
portal consumido somente na chegada e animacao de morte antes do reinicio.
Gera a galeria `preview_seletor_fases.png` e todas as `preview_fase_1.png` a
`preview_fase_10.png`. Os nove quadros `preview_morte_0.png` a `preview_morte_8.png`
mostram a morte na arena.

Regressao do Modo Insano (testes originais do estudo):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -static testes/verificar_insano.cpp -o verificacao_insano.exe -lgdiplus -lgdi32 -luser32 -lshell32
.\verificacao_insano.exe
```

Inclui comportamento, movimento, colisoes e dano do boss, ritmo, controle,
combate, Mario e macas. Nenhuma janela e aberta. A validacao de fidelidade e
descrita em `modo_insano/README.md`.

Regressao de colisoes durante o movimento (sem janela nem assets):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_colisoes.cpp -o verificacao_colisoes.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_colisoes.exe
```

Verifica bombas na casa de saida durante o passo nas quatro direcoes, para
P1/P2, e na nova casa apos a chegada; permite sair e bloqueia voltar sobre a
bomba. Cobre tambem duas casas de separacao vertical/horizontal e uma em cada eixo,
cruzamentos, inimigos e boss, casas vizinhas, chamas, imunidade, P2 cooperativo
e ausencia de dano de um P2 inativo no automatico.

Tambem verifica inversao imediata durante o passo, antes/depois de plantar a
bomba, sem teleporte e com mudancas repetidas de sentido. P1/P2 podem cancelar
a saida da propria bomba enquanto o passo esta em curso; depois de sair, nao
podem voltar sobre ela. Bombas de outro jogador bloqueiam a volta e curvas
perpendiculares continuam esperando o centro da casa.

Chefes da campanha e Mario aliado (na raiz, sem abrir janela):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_boss_fase8.cpp -o verificacao_boss_fase8.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_boss_fase8.exe
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_mario_aliado.cpp -o verificacao_mario_aliado.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_mario_aliado.exe
```

Confere chefes nas fases pares, avisos sem dano, ataques distintos, fogo,
movimento 3x2, bloqueio do caminho, HP e portal. Mario: bloco surpresa somente
nas fases pares, resgate, velocidade, cooldown, voo das macas, impactos,
recarga, tres vidas, imunidade e protecao do aliado/bloco pelo boss.
Polimento exporta `preview_boss_fase_2.png`, `preview_boss_fase_4.png`,
`preview_boss_fase_6.png` com Mario e `preview_boss_fogo.png` para a fase 8.

Ataques atualizados: `verificar_boss_fase8.cpp` tambem confere machados,
energia, rajada de veneno, colisao em subpassos, aviso sem dano, impacto nas
paredes, bonus, Mario/P2, poca circular e investida sem contato antecipado.
Os previews `preview_ataque_fase_{2,4,6}_{0..4}.png` mostram preparacao e voo;
`preview_veneno_poca.png` mostra o efeito persistente com sprites verdes.


Novos chefes (Hornet/Bowser):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -static testes/verificar_novos_bosses.cpp -o verificacao_novos_bosses.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_novos_bosses.exe
```

Verifica corpo atual, preparacao, retorno da agulha, fios, casco, fogo, soco e portal somente apos a animacao de derrota. O polimento exporta as poses dos novos chefes e as dez arenas sem abrir janela.

Para reconstruir os atlas dos chefes e tiles novos, use `python preparar_bosses_arenas.py` (Pillow e numpy). Todas as folhas originais e coordenadas catalogadas estao no projeto. Python nao e necessario para jogar.

A Hornet tem 22 etapas verificadas, com sequencias completas de preparo,
ataque e recuperacao. O polimento exporta `preview_hornet_etapa_0.png` ate
`preview_hornet_etapa_21.png`. O teste dos novos chefes confere retorno da
agulha em paredes, preservacao do bloco secreto, passagem durante voo,
contra-ataque apos dano e bloqueio de novos ataques durante a animacao.

## Ranking, nomes e pontuacao

`verificar_ranking_pontuacao.cpp` usa um CSV temporario independente do ranking
real. Verifica criar, ler, atualizar recorde, renomear sem perder pontos,
rejeitar nome duplicado na edicao e excluir/recriar do zero. Cobre nomes UTF-8,
acentos, simbolos, aspas e ponto-e-virgula; consolida duplicados antigos.
Tambem verifica cursor, atalhos bloqueados durante digitacao, movimento e
bomba sem desconto imediato, bonus de eficiencia no portal e Mario sem pontos
por macas coletadas ou acertos isolados. Compilar na raiz:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -static testes/verificar_ranking_pontuacao.cpp -o verificar_ranking_pontuacao.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificar_ranking_pontuacao.exe
```

O polimento gera previews adicionais do menu, cursor, ranking, renomeacao e
regras de pontuacao; os registros de exemplo sao fornecidos em memoria.
Confirmacoes de nome existente e exclusao devem ser avaliadas jogando.

## Controle, jogadores e menu

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_controle.cpp -o verificacao_controle.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\verificacao_controle.exe
```

Sem hardware nem janela. Verifica traducao XInput/DirectInput/SDL2 (zona morta
contra o ruido do 8BitDo), rejeicao do Pro Controller pelo DirectInput, nenhum
movimento ou bomba so por plugar o controle, 1P teclado, 1P joystick (teclado
ignorado, bomba uma vez por toque, retorno ao teclado ao desconectar), 2P com
P1 teclado + P2 joystick e o inverso, 2P so no teclado, demonstracao sem
interferencia e o fluxo JOGAR > quantidade > nomes > controles > personagens >
dificuldade > fase, inclusive navegando pelo joystick. Tambem cobre o limite de
bombas por jogador, os modos AMIGOS/INIMIGOS (quem fere quem e pontos de cada
um), o Mario do lado de quem o libertou, bombas de inimigos (proporcao por
dificuldade, mira, recarga) e o A que reinicia a fase sem armar bomba.

O polimento exporta `preview_quantidade_jogadores.png`,
`preview_controles_1p_joystick.png`, `preview_controles_2p.png`,
`preview_controles_sem_joystick.png`, `preview_personagens_2p.png`,
`preview_quantidade_inimigos.png`, `preview_nome_cursor.png` e
`preview_nomes_2p.png` (com TOP 10), `preview_partida_2p_inimigos.png` e
`preview_fim_2p_inimigos.png`.

Para conferir o controle fisico com a mesma rotina do jogo (SDL2.dll na raiz):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -static testes/diagnosticar_sdl.cpp -o diagnosticar_sdl.exe -lgdiplus -lwinmm -ldinput8 -ldxguid -lgdi32
.\diagnosticar_sdl.exe 20
```

Mostra se a SDL2.dll carregou, o nome do controle e, por 20 s, cada mudanca de
direcao e botao (BOMBA, VOLTAR, CHUTE, TELEPORTE, DETONAR, START, SELECT). Em
repouso deve ficar em `mov=(0,0)` sem botoes. Em 2026-10-13 o 8BitDo SN30 Pro
por USB apareceu como `Nintendo Switch Pro Controller` via SDL2, em repouso
sem leituras fantasmas.

`diagnosticar_controle.cpp` continua disponivel para inspecionar o caminho
DirectInput bruto (`--live`), onde esse mesmo controle gera ruido.
