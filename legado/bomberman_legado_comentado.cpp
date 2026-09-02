/*
===============================================================================
BOMBERMAN LEGADO — FONTE COMENTADA PARA A DEFESA
===============================================================================

Este arquivo e uma entrada de estudo compilavel. Ele inclui a mesma fonte usada
pela versao moderna, mas define VERSAO_LEGADO antes da inclusao. Dessa forma, a
logica existe em um unico lugar e a compilacao seleciona apenas o tema visual.

COMO COMPILAR ESTE ARQUIVO MANUALMENTE

g++ -std=c++17 -Wall -Wextra -pedantic bomberman_legado_comentado.cpp ^
    -o bomberman_legado_comentado.exe -lgdiplus -mwindows

-------------------------------------------------------------------------------
1. POR QUE A VERSAO LEGADA NAO USA O CONSOLE VERDADEIRO?
-------------------------------------------------------------------------------

O zoom do Windows Terminal pertence ao proprio terminal e nao pode ser
controlado de maneira confiavel pelo programa. Alem disso, o console nao possui
renderizacao nativa de PNG com transparencia. A versao legada recria a aparencia
do console em uma janela Win32 para oferecer tamanho previsivel e sprites reais.

-------------------------------------------------------------------------------
2. O QUE A MACRO VERSAO_LEGADO MUDA?
-------------------------------------------------------------------------------

Os blocos #ifdef VERSAO_LEGADO existentes em ../bomberman.cpp selecionam:

- celulas de 56 x 56 unidades-base;
- area util de 840 x 904 pixels em escala de 100%;
- fonte Consolas;
- fundo preto;
- cores fortes semelhantes aos blocos do console;
- molduras textuais no menu e no cabecalho;
- titulo e classe de janela exclusivos da versao legada.

A macro NAO altera mapa, movimento, bomba, explosao, inimigos, colisao, vitoria,
derrota ou bot automatico.

-------------------------------------------------------------------------------
3. ESTRUTURAS ACADEMICAS COMPARTILHADAS
-------------------------------------------------------------------------------

- mapa[LINHAS][COLUNAS]: matriz fixa que representa o cenario;
- Posicao: struct com linha e coluna;
- Inimigo: struct com posicao e indicador de vida;
- Bomba: struct com posicao, fases e contador;
- vector<Inimigo>: conjunto dinamico de inimigos;
- vector<Posicao>: celulas temporariamente atingidas pela explosao;
- queue<Posicao>: fila usada pela busca em largura do bot.

-------------------------------------------------------------------------------
4. PRINCIPAIS SUB-ROTINAS DA LOGICA
-------------------------------------------------------------------------------

- criarMapa(): cria bordas, pilares e paredes frageis;
- moverJogador(): valida destino, paredes e colisoes;
- colocarBomba(): garante que somente uma bomba esteja ativa;
- adicionarRaio(): propaga a explosao nas quatro direcoes;
- atingirPersonagens(): altera inimigos, estado e pontos por referencia;
- moverInimigos(): sorteia direcao e de um a tres passos;
- verificarVitoria(): detecta quando todos os inimigos morreram;
- proximoPassoDoBot(): usa BFS para encontrar um objetivo;
- atualizarBot(): decide entre andar, colocar bomba ou fugir;
- atualizar(): coordena toda a logica conforme o tempo decorrido.

-------------------------------------------------------------------------------
5. CAMADA VISUAL
-------------------------------------------------------------------------------

Win32 API:
  WinMain cria a janela. processarMensagem recebe WM_KEYDOWN, WM_TIMER,
  WM_PAINT, WM_DPICHANGED e WM_DESTROY.

GDI:
  CreateCompatibleDC e CreateCompatibleBitmap criam o quadro fora da tela.
  BitBlt copia o quadro pronto de uma vez e reduz flickering.

GDI+:
  Image carrega os PNGs com alpha. DrawImage desenha os sprites. A funcao
  desenharSpriteNaCelula preserva proporcao, margem e centralizacao.

Essas APIs cuidam somente de janela, entrada e desenho. Elas nao fornecem as
regras do jogo e nao constituem um motor grafico.

-------------------------------------------------------------------------------
6. ASSETS
-------------------------------------------------------------------------------

O executavel legado fica em legado/. Por isso carregarSprite() procura primeiro
legado/assets e depois ../assets. As imagens sao carregadas uma vez na abertura,
reutilizadas em todos os quadros e liberadas no fechamento.

-------------------------------------------------------------------------------
7. INCLUSAO DA FONTE OFICIAL
-------------------------------------------------------------------------------
*/

// Ativa os blocos de aparencia classica antes de incluir o nucleo compartilhado.
#define VERSAO_LEGADO

// Inclui estruturas, regras, renderizacao e ponto de entrada oficiais.
#include "../bomberman.cpp"
