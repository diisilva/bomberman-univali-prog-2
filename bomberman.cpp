/* ===========================================================================
   BOMBERMAN, Trabalho M1 de Algoritmos e Programacao II (22817)
   Universidade do Vale do Itajai, Escola Politecnica
   Curso de Ciencia da Computacao

   [PDF - TECNICA 1] "Identificacao dos desenvolvedores e comentarios
   pertinentes no codigo."

   Desenvolvedores: Diego Silva | Gabriel Bianchessi

   COMO O ARQUIVO ESTA ORGANIZADO
   Cada bloco abaixo tem um cabecalho dizendo (a) o que ele faz, (b) qual
   biblioteca ele usa e (c) qual item da grade de avaliacao do PDF ele atende.
   As marcacoes seguem este padrao:

       [PDF - FUNCIONALIDADE n]  -> um dos 15 itens de "FUNCIONALIDADES"
       [PDF - TECNICA n]         -> um dos 4 itens de "TECNICAS"

   A divisao geral do arquivo e:
       1) Bibliotecas usadas
       2) Constantes de regra e de tela
       3) Tipos (enums e structs) e estado do jogo
       4) Consultas basicas sobre o tabuleiro
       5) Geracao do cenario
       6) Jogador (movimento, bombas, cartas)
       7) Bombas (contagem, explosao, dano)
       8) Inimigos (caminhada aleatoria e perseguicao)
       9) Vitoria e derrota
      10) Modo automatico (bot)
      11) Atualizacao geral da logica
      12) Camada grafica (Win32 + GDI + GDI+)
      13) Janela, teclado e ponto de entrada

   CONTROLES
       WASD/setas .... mover        Espaco ... colocar bomba
       T ............. teletransportar (precisa da carta)
       R ............. reiniciar     M ....... voltar ao menu
       Q ou ESC ...... sair

   CARTAS ESPECIAIS (extra nosso, nao pedido no enunciado)
   Dois blocos dourados escondem cartas. Ao quebra-los com uma bomba, a carta
   cai no chao e basta pisar em cima:
       DUAS BOMBAS .. passa a permitir duas bombas no cenario ao mesmo tempo,
                      pelo resto da partida;
       TELEPORTE .... teletransporta o jogador uma vez, com a tecla T.
   =========================================================================== */

/* ---------------------------------------------------------------------------
   1) BIBLIOTECAS USADAS

   Bibliotecas do Windows (fazem parte do sistema, nao sao motor grafico):
     <windows.h>  Win32 API: cria a janela, recebe o teclado, dispara o timer
                  e entrega as mensagens do sistema. Tambem traz o GDI, que
                  fornece o bitmap de memoria usado no buffer duplo.
     <gdiplus.h>  GDI+: carrega arquivos PNG com transparencia (canal alpha) e
                  desenha imagem e texto com qualidade. O GDI puro nao le PNG.

   Bibliotecas padrao do C++ (conteudo da disciplina):
     <algorithm>  min, max, shuffle e fill.
     <chrono>     relogio de alta precisao para medir o tempo entre quadros.
     <memory>     unique_ptr, que libera a memoria das imagens sozinho.
     <queue>      fila usada na busca em largura (BFS) do bot e dos inimigos.
     <random>     mt19937 e uniform_int_distribution para todo sorteio do jogo.
     <string>     wstring, o texto em formato wide usado pelo Windows.
     <unordered_map> tabela usada para reaproveitar fontes ja criadas.
     <vector>     vetores de inimigos, de bombas, de celulas e de itens.
   --------------------------------------------------------------------------- */
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#endif

using namespace std;

/* ---------------------------------------------------------------------------
   2) CONSTANTES DE REGRA

   Todo numero que define o comportamento do jogo fica aqui em cima, com nome.
   Assim, mudar o ritmo do jogo e trocar uma constante, sem cacar numero solto
   no meio do codigo. Isso atende o pedido do PDF de "produzam todo o codigo
   pensando na possibilidade de que novas funcionalidades poderao ser
   solicitadas no futuro".
   --------------------------------------------------------------------------- */
const int LINHAS = 15;                      // altura do tabuleiro em celulas
const int COLUNAS = 19;                     // largura do tabuleiro em celulas
const int TOTAL_INIMIGOS_INICIAL = 7;       // quantos inimigos nascem na partida
const int TEMPO_BOMBA_MS = 2200;            // tempo entre plantar e explodir
const int TEMPO_EXPLOSAO_MS = 650;          // quanto tempo as chamas ficam na tela
const int INTERVALO_MOVIMENTO_MS = 105;     // espera entre dois passos do jogador
const int ALCANCE_EXPLOSAO_INICIAL = 1;     // raio da explosao no comeco
const int ALCANCE_EXPLOSAO_MAXIMO = 3;      // raio maximo que a bomba alcanca
const int PASSO_LOGICA_MS = 25;             // periodo do timer que atualiza a logica
const int PONTOS_POR_INIMIGO = 100;         // pontuacao ganha por inimigo derrotado

// [PDF - BOMBA] "Apenas uma bomba podera estar presente no cenario do jogo."
// O limite comeca em 1, como o enunciado exige. A carta DUAS_BOMBAS (extra
// nosso) sobe esse limite para 2 pelo resto da partida.
const int MAX_BOMBAS_INICIAL = 1;
const int MAX_BOMBAS_COM_CARTA = 2;

// Ritmo dos inimigos. A caminhada de 1 a 3 quadrados exigida pelo PDF acontece
// UM QUADRADO POR VEZ, separada por INTERVALO_PASSO_INIMIGO_MS. Se os tres
// quadrados fossem aplicados no mesmo instante, o inimigo sumiria de um ponto e
// apareceria no outro (efeito de teletransporte) em vez de caminhar.
const int INTERVALO_PASSO_INIMIGO_MS = 150; // tempo entre dois quadrados da mesma caminhada
const int PAUSA_INIMIGO_MIN_MS = 500;       // pausa minima entre uma caminhada e a proxima
const int PAUSA_INIMIGO_MAX_MS = 900;       // pausa maxima entre uma caminhada e a proxima
const int PASSOS_INIMIGO_MIN = 1;           // "1, 2 ou 3 quadrados" do enunciado
const int PASSOS_INIMIGO_MAX = 3;
const int DISTANCIA_PERSEGUICAO = 5;        // a partir de que distancia o inimigo pode perseguir
const int CHANCE_PERSEGUICAO = 45;          // chance (%) de perseguir em vez de andar ao acaso
const int CHANCE_PAREDE_FRAGIL = 36;        // chance (%) de uma celula livre virar parede fragil
const int TOTAL_BLOCOS_BONUS = 2;           // quantos blocos escondem cartas especiais

/* ---------------------------------------------------------------------------
   CONSTANTES DE TELA

   Medidas em "unidades base" (como se a tela estivesse em 100% de escala). A
   funcao px() converte essas unidades para pixels reais conforme a escala
   escolhida em ajustarEscalaParaCaberNaTela(), que leva em conta o DPI do
   monitor E o espaco realmente disponivel na tela.
   --------------------------------------------------------------------------- */
const int TAMANHO_CELULA = 52;              // lado de cada quadrado do tabuleiro
const int ALTURA_CABECALHO = 104;           // faixa superior com titulo e placar
const int ALTURA_RODAPE = 90;               // faixa inferior com controles e regras das cartas
const int LARGURA_JOGO = COLUNAS * TAMANHO_CELULA;
const int ALTURA_JOGO = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA + ALTURA_RODAPE;
const float ESCALA_MINIMA = 0.45f;          // piso de legibilidade em telas muito pequenas

/* ---------------------------------------------------------------------------
   3) TIPOS DO JOGO

   enum: da nome a valores que so podem ser um de uma lista fechada, no lugar de
   numeros soltos (0, 1, 2...). struct: agrupa dados que andam juntos.

   [PDF - TECNICA 4] "Subrotinas: boa segmentacao e emprego das tecnicas": structs
   e enums mantem os dados organizados por entidade.
   --------------------------------------------------------------------------- */

// O que pode existir em uma celula do tabuleiro.
// [PDF - PAREDES] "O cenario do jogo devera ser composto por paredes solidas e
// por paredes frageis."
enum TipoCelula {
    VAZIO,          // chao livre: da para andar
    PAREDE_SOLIDA,  // cinza: bloqueia e NAO pode ser destruida
    PAREDE_FRAGIL,  // azul: bloqueia mas a bomba destroi
    PAREDE_BONUS    // parede fragil especial: ao quebrar, solta uma carta
};

enum EstadoJogo { JOGANDO, VITORIA, DERROTA };  // situacao atual da partida
enum ModoJogo { MENU, MANUAL, AUTOMATICO };     // tela de menu, jogador humano ou bot
enum TipoItem { DUAS_BOMBAS, TELEPORTE };       // cartas especiais que os blocos bonus soltam

// Uma coordenada do tabuleiro. Usar uma struct evita ficar carregando duas
// variaveis soltas (linha e coluna) em todo lugar.
struct Posicao { int linha, coluna; };

// Um inimigo. Alem da posicao, cada um carrega o proprio relogio e a propria
// caminhada, para que eles nao andem todos juntos no mesmo instante.
struct Inimigo {
    Posicao posicao;            // onde ele esta no tabuleiro
    bool vivo;                  // false depois de ser atingido pela explosao
    int tempoProximoPassoMs;    // quanto falta (ms) para o proximo quadrado
    int passosRestantes;        // quadrados que ainda faltam na caminhada atual
    Posicao direcao;            // direcao sorteada para a caminhada atual
    bool perseguindo;           // true = segue o jogador; false = anda ao acaso
};

// Uma bomba plantada. Estar dentro do vetor `bombas` ja significa que ela
// existe no cenario, entao nao ha campo "ativa": quem some do vetor, acabou.
struct Bomba {
    Posicao posicao{0, 0};      // onde ela foi plantada
    bool explodindo = false;    // false = pavio queimando; true = chamas na tela
    int tempoMs = 0;            // contagem regressiva da fase atual
    vector<Posicao> celulas;    // celulas em chamas desta bomba (calculadas na explosao)
};

struct Item { Posicao posicao; TipoItem tipo; };        // carta caida no chao
struct BlocoBonus { Posicao posicao; TipoItem tipo; };  // carta ainda escondida na parede

/* ---------------------------------------------------------------------------
   ESTADO DO JOGO (variaveis globais)

   Sao globais porque a Win32 chama nossas funcoes atraves de um callback
   (processarMensagem) que nao permite carregar o estado do jogo como
   parametro. As sub-rotinas de regra, porem, recebem por parametro/referencia
   aquilo em que trabalham, conforme a TECNICA 3 mais abaixo.
   --------------------------------------------------------------------------- */

// [PDF - TECNICA 4] O cenario e uma MATRIZ FIXA, conteudo da disciplina.
TipoCelula mapa[LINHAS][COLUNAS];

Posicao jogador{1, 1};              // posicao atual do protagonista
vector<Inimigo> inimigos;           // VETOR de inimigos
vector<Bomba> bombas;               // VETOR de bombas no cenario (0, 1 ou 2)
vector<Posicao> areaExplosao;       // VETOR com as celulas atingidas pelas explosoes
vector<BlocoBonus> blocosBonus;     // cartas ainda escondidas dentro de paredes
vector<Item> itensNoChao;           // cartas ja liberadas, esperando o jogador
EstadoJogo estado = JOGANDO;
ModoJogo modo = MENU;
int pontos = 0;
int alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
int maxBombas = MAX_BOMBAS_INICIAL; // quantas bombas cabem no cenario ao mesmo tempo
bool possuiCartaTeleporte = false;  // o jogador tem a carta de teleporte guardada?
int tempoMovimentoMs = 0;           // trava que impede o jogador de andar rapido demais
bool teclaPressionada[256]{};       // estado de cada tecla, lido a cada tick
mt19937 gerador(random_device{}()); // gerador de numeros aleatorios (biblioteca <random>)

/* ---------------------------------------------------------------------------
   ESTADO DA CAMADA GRAFICA

   Nada aqui altera regra de jogo: sao apenas recursos do Windows guardados
   para nao serem recriados a cada quadro. Recriar bitmap, fonte e imagem
   redimensionada 40 vezes por segundo consumia quase um nucleo inteiro de CPU
   e fazia o teclado atrasar, porque a mesma thread que desenha e a que le o
   teclado.
   --------------------------------------------------------------------------- */
HWND janelaPrincipal = nullptr;     // identificador da janela (Win32)
ULONG_PTR tokenGdiPlus = 0;         // sessao aberta do GDI+
float escalaDpi = 1.0f;             // 1.0 = 100%, 1.5 = 150% etc.

unique_ptr<Gdiplus::Image> spriteJogador;     // PNG original do jogador (GDI+)
unique_ptr<Gdiplus::Image> spriteInimigo;     // PNG original do inimigo
unique_ptr<Gdiplus::Image> spriteBomba;       // PNG original da bomba
unique_ptr<Gdiplus::Image> spriteDuasBombas;  // PNG da carta "duas bombas"
unique_ptr<Gdiplus::Image> spriteTeleporte;   // PNG da carta de teleporte

HDC bufferDC = nullptr;             // contexto de memoria do buffer duplo (GDI)
HBITMAP bufferBitmap = nullptr;     // bitmap onde o quadro e montado (GDI)
int bufferLargura = 0;
int bufferAltura = 0;

unordered_map<int, unique_ptr<Gdiplus::Font>> cacheFontes;  // fontes por tamanho
unique_ptr<Gdiplus::Bitmap> cacheMapa;      // terreno ja desenhado
bool cacheMapaSujo = true;                  // true = precisa redesenhar o terreno
unique_ptr<Gdiplus::Bitmap> cacheSpriteJogador;  // PNG ja reduzido ao tamanho da celula
unique_ptr<Gdiplus::Bitmap> cacheSpriteInimigo;
unique_ptr<Gdiplus::Bitmap> cacheSpriteBomba;
unique_ptr<Gdiplus::Bitmap> cacheSpriteDuasBombas;
unique_ptr<Gdiplus::Bitmap> cacheSpriteTeleporte;

// Marca que o terreno precisa ser redesenhado (chamada quando uma parede cai).
void invalidarCacheMapa() { cacheMapaSujo = true; }

/* ---------------------------------------------------------------------------
   4) CONSULTAS BASICAS SOBRE O TABULEIRO

   Sub-rotinas curtas que respondem "sim ou nao" e sao reusadas por todo o
   resto do arquivo.

   [PDF - TECNICA 3] "Subrotinas: parametros e referencia corretos."
   A escolha de cada passagem segue um criterio:
     - int e Posicao vao POR VALOR: sao pequenos e a funcao nao precisa altera-los;
     - a matriz e os vetores vao POR REFERENCIA com const: assim nao e feita
       uma copia do tabuleiro/vetor inteiro a cada chamada, e o const garante
       que a funcao so le, nunca escreve.
   --------------------------------------------------------------------------- */

// Duas posicoes sao a mesma celula?
bool iguais(Posicao a, Posicao b) {
    return a.linha == b.linha && a.coluna == b.coluna;
}

// A coordenada existe dentro da matriz? Protege contra acesso fora do vetor.
bool dentro(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

// [PDF - FUNCIONALIDADE 3] "O jogador e bloqueado por qualquer tipo de parede."
// [PDF - PAREDES] "Todas as paredes deverao impedir a passagem do jogador e dos inimigos."
// Esta e a unica regra de colisao com parede do jogo: so a celula VAZIO e
// caminhavel. PAREDE_SOLIDA, PAREDE_FRAGIL e PAREDE_BONUS bloqueiam igual.
//
// [PDF - FUNCIONALIDADE 2] "O jogador consegue se mover em uma area com bomba."
// A funcao olha SOMENTE a matriz do cenario. A bomba nao esta na matriz (esta
// no vetor `bombas`), entao ela nao aparece aqui e por isso nao impede a
// passagem do jogador.
bool livre(const TipoCelula cenario[][COLUNAS], int linha, int coluna) {
    return dentro(linha, coluna) && cenario[linha][coluna] == VAZIO;
}

// Existe algum inimigo vivo nesta celula? O parametro "ignorar" serve para um
// inimigo nao se enxergar a si mesmo ao testar para onde pode andar.
bool temInimigo(const vector<Inimigo>& lista, Posicao p, int ignorar = -1) {
    for (int i = 0; i < static_cast<int>(lista.size()); i++) {
        if (i != ignorar && lista[i].vivo && iguais(lista[i].posicao, p)) return true;
    }
    return false;
}

// Existe uma bomba ainda nao detonada nesta celula? Serve para os inimigos
// desviarem dela, para o teleporte nao cair em cima e para nao empilhar duas
// bombas na mesma casa.
bool temBombaEm(const vector<Bomba>& lista, Posicao p) {
    for (const Bomba& b : lista) if (!b.explodindo && iguais(b.posicao, p)) return true;
    return false;
}

// Esta celula esta pegando fogo neste instante?
bool naExplosao(const vector<Posicao>& area, Posicao p) {
    for (Posicao parte : area) if (iguais(parte, p)) return true;
    return false;
}

// Quantos inimigos ainda estao vivos. Usado no placar e na vitoria.
int inimigosVivos(const vector<Inimigo>& lista) {
    int total = 0;
    for (const Inimigo& inimigo : lista) if (inimigo.vivo) total++;
    return total;
}

/* ---------------------------------------------------------------------------
   5) GERACAO DO CENARIO

   [PDF - PAREDES] "O cenario do jogo devera ser composto por paredes solidas e
   por paredes frageis."
   --------------------------------------------------------------------------- */

// Abre espaco em volta de um ponto de nascimento, para ninguem nascer preso.
// O laco vai de 1 ate LINHAS-2 / COLUNAS-2 de proposito: a moldura externa
// (linha 0, linha LINHAS-1, coluna 0, coluna COLUNAS-1) NUNCA pode ser aberta,
// senao o tabuleiro fica com buracos na parede de fora e os personagens saem
// andando pela borda.
void limparArea(TipoCelula cenario[][COLUNAS], int linha, int coluna) {
    cenario[linha][coluna] = VAZIO;
    if (linha > 1) cenario[linha - 1][coluna] = VAZIO;
    if (linha < LINHAS - 2) cenario[linha + 1][coluna] = VAZIO;
    if (coluna > 1) cenario[linha][coluna - 1] = VAZIO;
    if (coluna < COLUNAS - 2) cenario[linha][coluna + 1] = VAZIO;
}

// Monta o tabuleiro do zero. Tres regras, nesta ordem:
//   1. borda        -> parede solida (a moldura que fecha o mapa);
//   2. pilar        -> parede solida nas celulas de linha E coluna pares,
//                      formando o xadrez classico do Bomberman;
//   3. resto        -> CHANCE_PAREDE_FRAGIL% de virar parede fragil, senao chao.
// A matriz e recebida por parametro (ela e alterada aqui dentro).
void criarMapa(TipoCelula cenario[][COLUNAS]) {
    uniform_int_distribution<int> chance(0, 99);
    for (int l = 0; l < LINHAS; l++) {
        for (int c = 0; c < COLUNAS; c++) {
            bool borda = l == 0 || l == LINHAS - 1 || c == 0 || c == COLUNAS - 1;
            bool pilar = l % 2 == 0 && c % 2 == 0;
            if (borda || pilar) cenario[l][c] = PAREDE_SOLIDA;
            else if (chance(gerador) < CHANCE_PAREDE_FRAGIL) cenario[l][c] = PAREDE_FRAGIL;
            else cenario[l][c] = VAZIO;
        }
    }
}

// Lista todas as paredes frageis existentes; delas sairao os blocos bonus.
vector<Posicao> paredesFrageisDoMapa(const TipoCelula cenario[][COLUNAS]) {
    vector<Posicao> encontradas;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (cenario[l][c] == PAREDE_FRAGIL) encontradas.push_back({l, c});
    return encontradas;
}

// Escolhe ao acaso algumas paredes frageis e as transforma em PAREDE_BONUS,
// guardando no vetor blocosBonus qual carta cada uma esconde.
void posicionarBlocosBonus(TipoCelula cenario[][COLUNAS], vector<BlocoBonus>& blocos) {
    blocos.clear();
    vector<Posicao> candidatos = paredesFrageisDoMapa(cenario);
    shuffle(candidatos.begin(), candidatos.end(), gerador);  // <algorithm> + <random>
    vector<TipoItem> tipos = {DUAS_BOMBAS, TELEPORTE};
    for (int i = 0; i < TOTAL_BLOCOS_BONUS && i < static_cast<int>(candidatos.size()); i++) {
        cenario[candidatos[i].linha][candidatos[i].coluna] = PAREDE_BONUS;
        blocos.push_back({candidatos[i], tipos[i]});
    }
}

// Sorteia de onde cada personagem comeca: os cantos do mapa e os pontos medios
// das bordas, embaralhados. O vetor vai POR REFERENCIA porque a funcao
// preenche ele para quem chamou.
void sortearPosicoesIniciais(vector<Posicao>& posicoes, TipoCelula cenario[][COLUNAS]) {
    posicoes = {{1, 1}, {1, COLUNAS - 2}, {LINHAS - 2, 1}, {LINHAS - 2, COLUNAS - 2},
                {1, COLUNAS / 2}, {LINHAS - 2, COLUNAS / 2},
                {LINHAS / 2, 1}, {LINHAS / 2, COLUNAS - 2}};
    shuffle(posicoes.begin(), posicoes.end(), gerador);
    for (const Posicao& p : posicoes) limparArea(cenario, p.linha, p.coluna);
}

// Prepara um inimigo recem-nascido: vivo, parado, esperando a primeira pausa
// terminar para comecar a andar.
Inimigo criarInimigo(Posicao onde) {
    uniform_int_distribution<int> sorteioPausa(PAUSA_INIMIGO_MIN_MS, PAUSA_INIMIGO_MAX_MS);
    Inimigo novo;
    novo.posicao = onde;
    novo.vivo = true;
    novo.tempoProximoPassoMs = sorteioPausa(gerador);
    novo.passosRestantes = 0;
    novo.direcao = {0, 0};
    novo.perseguindo = false;
    return novo;
}

// Devolve o jogo ao estado inicial: mapa novo, inimigos novos, placar zerado.
// Chamada pela tecla R, pelo inicio de uma partida e pelo modo automatico.
void reiniciar() {
    criarMapa(mapa);
    vector<Posicao> spawns;
    sortearPosicoesIniciais(spawns, mapa);
    posicionarBlocosBonus(mapa, blocosBonus);
    invalidarCacheMapa();

    jogador = spawns[0];            // o primeiro sorteado e o jogador
    inimigos.clear();
    // [PDF - FUNCIONALIDADE 10] A quantidade de inimigos sai de uma unica
    // constante: TOTAL_INIMIGOS_INICIAL.
    for (int i = 1; i <= TOTAL_INIMIGOS_INICIAL; i++) inimigos.push_back(criarInimigo(spawns[i]));

    itensNoChao.clear();
    alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
    maxBombas = MAX_BOMBAS_INICIAL;     // a carta de duas bombas vale por partida
    possuiCartaTeleporte = false;
    bombas.clear();                     // nenhuma bomba no cenario
    areaExplosao.clear();
    estado = JOGANDO;
    pontos = 0;
    tempoMovimentoMs = 0;
}

// Sai do menu e comeca a partida no modo escolhido.
void iniciarPartida(ModoJogo novoModo) {
    modo = novoModo;
    reiniciar();
}

/* ---------------------------------------------------------------------------
   6) JOGADOR
   --------------------------------------------------------------------------- */

// Se o jogador pisou em cima de uma carta caida no chao, ele a recolhe.
// `int& limiteBombas` vai por referencia porque a carta DUAS_BOMBAS altera o
// limite de bombas do jogo inteiro; `bool& temTeleporte`, pelo mesmo motivo.
void coletarItens(Posicao onde, vector<Item>& chao, int& limiteBombas, bool& temTeleporte) {
    for (size_t i = 0; i < chao.size(); i++) {
        if (iguais(chao[i].posicao, onde)) {
            if (chao[i].tipo == DUAS_BOMBAS) limiteBombas = MAX_BOMBAS_COM_CARTA;
            else temTeleporte = true;
            chao.erase(chao.begin() + i);   // remove do vetor: ja foi pego
            return;
        }
    }
}

// [PDF - FUNCIONALIDADE 1] "O jogador se move corretamente para todas as direcoes."
// [PDF - FUNCIONALIDADE 4] "O jogador se move sem bugs."
// dl e dc sao o deslocamento: (-1,0) cima, (1,0) baixo, (0,-1) esquerda, (0,1) direita.
// A posicao vai POR REFERENCIA (Posicao&) porque a funcao precisa altera-la.
void moverJogador(Posicao& personagem, int dl, int dc) {
    if (estado != JOGANDO || tempoMovimentoMs > 0) return;  // partida acabou ou em cooldown

    Posicao destino{personagem.linha + dl, personagem.coluna + dc};

    // [PDF - FUNCIONALIDADE 3] So anda se o destino for chao. Qualquer parede barra.
    // [PDF - FUNCIONALIDADE 2] livre() nao consulta as bombas, entao andar sobre
    // a celula de uma bomba e permitido.
    if (livre(mapa, destino.linha, destino.coluna)) personagem = destino;

    // [PDF - FUNCIONALIDADE 8] "O jogador morre quando colide com um inimigo."
    // [PDF - FUNCIONALIDADE 9] Andar para dentro do fogo tambem mata.
    if (temInimigo(inimigos, personagem) || naExplosao(areaExplosao, personagem)) estado = DERROTA;

    coletarItens(personagem, itensNoChao, maxBombas, possuiCartaTeleporte);

    // [PDF - FUNCIONALIDADE 4] O cooldown e o que impede o personagem de
    // atravessar o mapa inteiro em um quadro e de "tremer" na tela.
    tempoMovimentoMs = INTERVALO_MOVIMENTO_MS;
}

// [PDF - FUNCIONALIDADE 5] "O jogador consegue colocar bombas onde esta."
// [PDF - FUNCIONALIDADE 6] "O jogador nao consegue colocar uma segunda bomba
//                           enquanto outra esta no mapa."
// [PDF - BOMBA] "Apenas uma bomba podera estar presente no cenario do jogo."
//
// A regra do enunciado e o teste `lista.size() >= limite`, com `limite` valendo
// MAX_BOMBAS_INICIAL (= 1) durante toda a partida. O limite so sobe para 2 se o
// jogador encontrar a carta DUAS_BOMBAS, que e um extra nosso, fora do
// enunciado. Enquanto ele nao pegar a carta, o comportamento e exatamente o
// exigido: uma bomba por vez.
void colocarBomba(vector<Bomba>& lista, Posicao origem, int limite) {
    if (estado != JOGANDO) return;
    if (static_cast<int>(lista.size()) >= limite) return;   // <- FUNCIONALIDADE 6
    if (temBombaEm(lista, origem)) return;                  // nao empilha duas na mesma casa

    Bomba nova;
    nova.posicao = origem;              // a bomba nasce exatamente onde o jogador esta
    nova.explodindo = false;
    nova.tempoMs = TEMPO_BOMBA_MS;      // comeca a contagem regressiva
    lista.push_back(nova);
}

// EXTRA (nao pedido no PDF): com a carta de teleporte, pula para uma celula
// livre sorteada do tabuleiro.
void teletransportarJogador() {
    if (estado != JOGANDO || !possuiCartaTeleporte) return;
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (livre(mapa, l, c) && !temInimigo(inimigos, {l, c}) && !temBombaEm(bombas, {l, c}))
                livres.push_back({l, c});
    if (livres.empty()) return;
    uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
    jogador = livres[sorteio(gerador)];
    if (temInimigo(inimigos, jogador) || naExplosao(areaExplosao, jogador)) estado = DERROTA;
    coletarItens(jogador, itensNoChao, maxBombas, possuiCartaTeleporte);
    possuiCartaTeleporte = false;
}

/* ---------------------------------------------------------------------------
   7) BOMBAS E EXPLOSAO
   --------------------------------------------------------------------------- */

// Quando uma PAREDE_BONUS e destruida, a carta que estava escondida vira um
// item no chao. Os dois vetores vao por referencia porque ambos sao alterados.
void destruirBlocoBonus(vector<BlocoBonus>& blocos, vector<Item>& chao, int linha, int coluna) {
    for (size_t i = 0; i < blocos.size(); i++) {
        if (blocos[i].posicao.linha == linha && blocos[i].posicao.coluna == coluna) {
            chao.push_back({blocos[i].posicao, blocos[i].tipo});
            blocos.erase(blocos.begin() + i);
            return;
        }
    }
}

// [PDF - FUNCIONALIDADE 11] "A bomba nao destroi paredes solidas."
// [PDF - FUNCIONALIDADE 12] "A bomba destroi paredes frageis."
// [PDF - BOMBA] "A explosao nao podera quebrar paredes solidas."
// Avanca uma das quatro direcoes da cruz de fogo, celula por celula:
//   - PAREDE_SOLIDA  -> return imediato: a chama nem entra na celula;
//   - PAREDE_FRAGIL  -> a celula pega fogo, a parede vira chao e o raio PARA
//                       ali (uma parede nao deixa a chama passar para tras dela);
//   - chao           -> a celula pega fogo e o raio continua.
// O vetor "area" vai por referencia porque a funcao acrescenta celulas nele.
void adicionarRaio(TipoCelula cenario[][COLUNAS], Posicao origem, int dl, int dc,
                   int alcance, vector<Posicao>& area) {
    for (int distancia = 1; distancia <= alcance; distancia++) {
        int l = origem.linha + dl * distancia;
        int c = origem.coluna + dc * distancia;

        if (!dentro(l, c) || cenario[l][c] == PAREDE_SOLIDA) return;  // FUNCIONALIDADE 11

        area.push_back({l, c});                                       // FUNCIONALIDADE 13

        if (cenario[l][c] == PAREDE_FRAGIL) {                         // FUNCIONALIDADE 12
            cenario[l][c] = VAZIO;
            invalidarCacheMapa();
            return;
        }
        if (cenario[l][c] == PAREDE_BONUS) {
            cenario[l][c] = VAZIO;
            destruirBlocoBonus(blocosBonus, itensNoChao, l, c);
            invalidarCacheMapa();
            return;
        }
    }
}

// Marca um inimigo como morto e paga a recompensa.
// [PDF - TECNICA 3] Tres passagens diferentes na mesma assinatura:
//   Inimigo& -> referencia, porque o inimigo e alterado (vivo = false);
//   int&     -> referencia, porque a pontuacao e o alcance sao somados aqui e
//               o resultado precisa valer para quem chamou.
void matarInimigo(Inimigo& inimigo, int& pontuacao, int& alcance) {
    inimigo.vivo = false;
    pontuacao += PONTOS_POR_INIMIGO;
    // EXTRA: a cada inimigo derrotado a bomba fica mais forte, ate o limite.
    if (alcance < ALCANCE_EXPLOSAO_MAXIMO) alcance++;
}

// [PDF - FUNCIONALIDADE 9]  "O jogador morre quando uma bomba explode perto dele."
// [PDF - FUNCIONALIDADE 15] "A condicao de derrota e atingida corretamente."
// [PDF - BOMBA] "Quando a bomba explodir devera destruir todos os jogadores e
//                inimigos que estiverem em sua proximidade."
// Esta e a sub-rotina que o PDF cobra em TECNICA 3: ela recebe a area em chamas
// so para leitura (const) e o vetor de inimigos, o estado da partida, a
// pontuacao e o alcance por referencia, alterando os quatro diretamente.
void atingirPersonagens(const vector<Posicao>& area, vector<Inimigo>& listaInimigos,
                        EstadoJogo& estadoAtual, int& pontuacao, int& alcance) {
    if (naExplosao(area, jogador)) estadoAtual = DERROTA;           // FUNCIONALIDADE 9
    for (Inimigo& inimigo : listaInimigos) {                        // & = altera o original
        if (inimigo.vivo && naExplosao(area, inimigo.posicao))
            matarInimigo(inimigo, pontuacao, alcance);              // INIMIGO morre na explosao
    }
}

// [PDF - FUNCIONALIDADE 13] "A explosao da bomba devera ser visivel."
// Monta a cruz de fogo DESTA bomba e guarda as celulas dentro dela. As paredes
// frageis sao destruidas aqui, uma unica vez, no instante da explosao.
void iniciarExplosao(Bomba& alvo) {
    alvo.explodindo = true;
    alvo.tempoMs = TEMPO_EXPLOSAO_MS;   // agora conta o tempo das chamas na tela
    alvo.celulas.clear();
    alvo.celulas.push_back(alvo.posicao);                                     // centro
    adicionarRaio(mapa, alvo.posicao, -1, 0, alcanceExplosao, alvo.celulas);  // cima
    adicionarRaio(mapa, alvo.posicao, 1, 0, alcanceExplosao, alvo.celulas);   // baixo
    adicionarRaio(mapa, alvo.posicao, 0, -1, alcanceExplosao, alvo.celulas);  // esquerda
    adicionarRaio(mapa, alvo.posicao, 0, 1, alcanceExplosao, alvo.celulas);   // direita
}

// Junta em um unico vetor as celulas em chamas de TODAS as bombas que estao
// explodindo. Com duas bombas no cenario, as duas cruzes de fogo machucam ao
// mesmo tempo, e o resto do jogo continua consultando uma lista so.
void recalcularAreaExplosao(const vector<Bomba>& lista, vector<Posicao>& area) {
    area.clear();
    for (const Bomba& b : lista)
        if (b.explodindo)
            for (const Posicao& p : b.celulas) area.push_back(p);
}

// [PDF - BOMBA] "A bomba ... devera explodir depois de um tempo."
// [PDF - FUNCIONALIDADE 7] "O jogador consegue colocar uma segunda bomba apos
//                           a explosao da anterior."
// Relogio de todas as bombas, chamado a cada tick. Cada bomba vive duas fases:
//   fase 1 (explodindo = false) -> conta TEMPO_BOMBA_MS, o pavio;
//   fase 2 (explodindo = true)  -> conta TEMPO_EXPLOSAO_MS com o fogo na tela.
// Ao fim da fase 2 a bomba SAI DO VETOR, e e so por isso que o jogador pode
// plantar a proxima (ver colocarBomba, FUNCIONALIDADE 6).
void atualizarBombas(int tempoMs, vector<Bomba>& lista) {
    for (Bomba& b : lista) {
        b.tempoMs -= tempoMs;
        if (!b.explodindo && b.tempoMs <= 0) iniciarExplosao(b);
    }

    // Remove as explosoes que ja terminaram. O laco nao usa for-each porque
    // apagar um elemento no meio do vetor muda os indices seguintes.
    for (size_t i = 0; i < lista.size(); ) {
        if (lista[i].explodindo && lista[i].tempoMs <= 0) lista.erase(lista.begin() + i);
        else i++;
    }

    recalcularAreaExplosao(lista, areaExplosao);

    // O dano e reaplicado a cada tick enquanto houver fogo: assim, quem ANDAR
    // para dentro das chamas tambem morre, nao so quem estava la no instante
    // da explosao.
    if (!areaExplosao.empty())
        atingirPersonagens(areaExplosao, inimigos, estado, pontos, alcanceExplosao);
}

/* ---------------------------------------------------------------------------
   8) INIMIGOS

   [PDF - FUNCIONALIDADE 10] "O inimigo se move conforme solicitado."
   [PDF - INIMIGO] "O inimigo, de tempos em tempos, ira se mover 1, 2 ou 3
                    quadrados em uma direcao aleatoria livre ou ate encontrar
                    um obstaculo."

   Como isso foi implementado: cada inimigo guarda uma CAMINHADA (uma direcao
   sorteada + quantos quadrados faltam). Ele da um quadrado por vez, esperando
   INTERVALO_PASSO_INIMIGO_MS entre eles, e ao terminar faz uma pausa sorteada
   entre PAUSA_INIMIGO_MIN_MS e PAUSA_INIMIGO_MAX_MS ("de tempos em tempos").
   Dar os tres quadrados de uma vez so tambem cumpriria o enunciado, mas na
   tela o inimigo pareceria teletransportar, porque ele seria desenhado apenas
   na posicao final.
   --------------------------------------------------------------------------- */

// Busca em largura (BFS) com FILA (<queue>): devolve qual o PRIMEIRO passo do
// caminho mais curto de "origem" ate "alvo", andando so por celulas livres.
// Se nao houver caminho, devolve a propria origem.
Posicao proximoPassoParaAlvo(Posicao origem, Posicao alvo) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};       // matriz de apoio: ja passei por aqui?
    Posicao anterior[LINHAS][COLUNAS];      // de qual celula eu cheguei nesta
    queue<Posicao> fila;                    // a fila que da nome a "busca em largura"
    fila.push(origem);
    visitado[origem.linha][origem.coluna] = true;
    bool encontrou = false;

    while (!fila.empty() && !encontrou) {
        Posicao atual = fila.front();
        fila.pop();
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(mapa, proxima.linha, proxima.coluna) ||
                visitado[proxima.linha][proxima.coluna]) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            anterior[proxima.linha][proxima.coluna] = atual;
            if (iguais(proxima, alvo)) { encontrou = true; break; }
            fila.push(proxima);
        }
    }

    if (!encontrou) return origem;
    // Volta pelo caminho de tras para frente ate achar o vizinho da origem.
    Posicao passo = alvo;
    while (!iguais(anterior[passo.linha][passo.coluna], origem))
        passo = anterior[passo.linha][passo.coluna];
    return passo;
}

// Sorteia a proxima caminhada de um inimigo: quantos quadrados e para onde.
// Perto do jogador, ha CHANCE_PERSEGUICAO% de chance de ele perseguir usando o
// BFS acima; fora disso, escolhe uma das quatro direcoes livres ao acaso,
// exatamente como o enunciado pede.
void planejarCaminhada(Inimigo& inimigo, int indice) {
    uniform_int_distribution<int> sorteioPassos(PASSOS_INIMIGO_MIN, PASSOS_INIMIGO_MAX);
    uniform_int_distribution<int> sorteioDirecao(0, 3);
    uniform_int_distribution<int> sorteioChance(0, 99);

    // Distancia de Manhattan: soma das diferencas de linha e de coluna.
    int distancia = abs(inimigo.posicao.linha - jogador.linha) +
                    abs(inimigo.posicao.coluna - jogador.coluna);
    inimigo.perseguindo = distancia <= DISTANCIA_PERSEGUICAO &&
                          sorteioChance(gerador) < CHANCE_PERSEGUICAO;
    inimigo.passosRestantes = sorteioPassos(gerador);   // 1, 2 ou 3 quadrados

    if (inimigo.perseguindo) {
        inimigo.direcao = {0, 0};   // perseguindo, a direcao e recalculada a cada passo
        return;
    }

    // Caminhada aleatoria: testa as quatro direcoes a partir de uma sorteada,
    // e fica com a primeira que estiver livre.
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    int inicio = sorteioDirecao(gerador);
    for (int tentativa = 0; tentativa < 4; tentativa++) {
        int d = (inicio + tentativa) % 4;
        Posicao destino{inimigo.posicao.linha + direcoes[d][0],
                        inimigo.posicao.coluna + direcoes[d][1]};
        if (livre(mapa, destino.linha, destino.coluna) && !temInimigo(inimigos, destino, indice)) {
            inimigo.direcao = {direcoes[d][0], direcoes[d][1]};
            return;
        }
    }
    inimigo.passosRestantes = 0;    // cercado: fica parado ate a proxima pausa
}

// Executa UM quadrado da caminhada atual. Devolve false se o inimigo bateu em
// um obstaculo (ai a caminhada e interrompida, como o enunciado manda: "ou ate
// encontrar um obstaculo").
bool darUmPassoInimigo(Inimigo& inimigo, int indice) {
    Posicao destino;
    if (inimigo.perseguindo) {
        destino = proximoPassoParaAlvo(inimigo.posicao, jogador);
        if (iguais(destino, inimigo.posicao)) return false;  // sem caminho ate o jogador
    } else {
        destino = {inimigo.posicao.linha + inimigo.direcao.linha,
                   inimigo.posicao.coluna + inimigo.direcao.coluna};
    }

    // [PDF - INIMIGO] "...em uma direcao aleatoria livre..."
    // TRAVA ANTI-DIAGONAL: um passo so vale se for de EXATAMENTE um quadrado,
    // em linha OU em coluna. A soma dos deslocamentos em modulo tem de dar 1:
    //   cima/baixo/esquerda/direita -> |dl| + |dc| = 1  (aceito)
    //   diagonal                    -> |dl| + |dc| = 2  (recusado)
    //   pulo de 2 ou 3 celulas      -> |dl| + |dc| >= 2 (recusado)
    // As direcoes ja saem da tabela de 4 direcoes cardeais e o BFS da
    // perseguicao so expande vizinhos cardeais, entao isto e uma garantia
    // extra: nenhum caminho do codigo consegue mover um inimigo na diagonal
    // nem faze-lo "pular" celulas dentro do mesmo quadro.
    int dl = destino.linha - inimigo.posicao.linha;
    int dc = destino.coluna - inimigo.posicao.coluna;
    if (abs(dl) + abs(dc) != 1) return false;

    // Obstaculos que interrompem a caminhada:
    if (!livre(mapa, destino.linha, destino.coluna)) return false;      // parede
    if (temBombaEm(bombas, destino)) return false;                      // uma bomba no chao
    if (temInimigo(inimigos, destino, indice)) return false;            // outro inimigo

    inimigo.posicao = destino;

    // [PDF - FUNCIONALIDADE 8] Se o inimigo andou por cima do jogador, o
    // jogador morre. A colisao e testada dos DOIS lados (aqui e em moverJogador),
    // porque qualquer um dos dois pode ser quem se move.
    if (iguais(destino, jogador)) { estado = DERROTA; return false; }

    // [PDF - INIMIGO] O inimigo morre se entrar em uma celula em chamas.
    if (naExplosao(areaExplosao, destino)) {
        matarInimigo(inimigo, pontos, alcanceExplosao);
        return false;
    }
    return true;
}

// Relogio de todos os inimigos, chamado uma vez por tick de logica.
// tempoMs e quanto tempo real passou desde o tick anterior.
void moverInimigos(int tempoMs, vector<Inimigo>& lista) {
    uniform_int_distribution<int> sorteioPausa(PAUSA_INIMIGO_MIN_MS, PAUSA_INIMIGO_MAX_MS);

    for (int i = 0; i < static_cast<int>(lista.size()); i++) {
        Inimigo& inimigo = lista[i];        // & : trabalha no inimigo real, nao em uma copia
        if (!inimigo.vivo) continue;

        inimigo.tempoProximoPassoMs -= tempoMs;
        if (inimigo.tempoProximoPassoMs > 0) continue;   // ainda nao chegou a hora dele

        if (inimigo.passosRestantes == 0) planejarCaminhada(inimigo, i);

        bool andou = inimigo.passosRestantes > 0 && darUmPassoInimigo(inimigo, i);
        if (andou) inimigo.passosRestantes--;
        else inimigo.passosRestantes = 0;   // bateu em obstaculo: encerra a caminhada

        // Ainda tem quadrado pela frente -> espera pouco (parece caminhada).
        // Caminhada terminou -> espera a pausa longa ("de tempos em tempos").
        inimigo.tempoProximoPassoMs = inimigo.passosRestantes > 0
                                    ? INTERVALO_PASSO_INIMIGO_MS
                                    : sorteioPausa(gerador);
    }
}

/* ---------------------------------------------------------------------------
   9) VITORIA E DERROTA
   --------------------------------------------------------------------------- */

// [PDF - FUNCIONALIDADE 14] "A condicao de vitoria e atingida corretamente."
// [PDF - CONDICAO DE VITORIA] "O jogador ira vencer se estiver vivo quando
//                              todos os inimigos tiverem morrido."
// O "estiver vivo" e garantido pelo primeiro if: se o estado ja for DERROTA, a
// funcao sai antes e nunca transforma uma derrota em vitoria.
void verificarVitoria(const vector<Inimigo>& lista, EstadoJogo& estadoAtual) {
    if (estadoAtual != JOGANDO) return;
    if (inimigosVivos(lista) == 0) estadoAtual = VITORIA;
}

/* ---------------------------------------------------------------------------
   10) MODO AUTOMATICO (BOT), EXTRA nao pedido no PDF

   O bot usa as MESMAS sub-rotinas do jogador humano (moverJogador e
   colocarBomba). Ele so decide para onde ir; nenhuma regra e duplicada.
   --------------------------------------------------------------------------- */

// Esta celula seria atingida por uma bomba plantada em "origemBomba"? Calculo
// puro, que serve tanto para fugir das bombas reais quanto para o bot simular
// uma bomba que ele ainda nem plantou.
bool celulaNoAlcance(const TipoCelula cenario[][COLUNAS], Posicao origemBomba, Posicao p, int alcance) {
    if (iguais(p, origemBomba)) return true;
    int dl = p.linha - origemBomba.linha;
    int dc = p.coluna - origemBomba.coluna;
    if (dl != 0 && dc != 0) return false;   // fogo so anda em cruz, nunca na diagonal
    int distancia = abs(dl) + abs(dc);
    if (distancia > alcance) return false;
    int passoL = (dl > 0) - (dl < 0);       // -1, 0 ou 1: o sinal da direcao
    int passoC = (dc > 0) - (dc < 0);
    for (int i = 1; i <= distancia; i++) {
        int l = origemBomba.linha + passoL * i;
        int c = origemBomba.coluna + passoC * i;
        if (cenario[l][c] == PAREDE_SOLIDA) return false;   // a parede protege
        if (cenario[l][c] == PAREDE_FRAGIL || cenario[l][c] == PAREDE_BONUS) return i == distancia;
    }
    return true;
}

// A celula esta na mira de ALGUMA bomba que ainda vai explodir?
bool perigoDaBomba(Posicao p) {
    for (const Bomba& b : bombas)
        if (!b.explodindo && celulaNoAlcance(mapa, b.posicao, p, alcanceExplosao)) return true;
    return false;
}

// BFS que responde: se eu plantar uma bomba aqui, consigo chegar a alguma
// celula fora do fogo antes de ela explodir? Sem essa checagem, o bot plantava
// bomba em cantos fechados e se matava sozinho.
bool existeFugaSegura(Posicao origem) {
    int passosDisponiveis = TEMPO_BOMBA_MS / INTERVALO_MOVIMENTO_MS;
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};
    queue<pair<Posicao, int>> fila;         // a fila guarda a celula e quantos passos custou
    fila.push({origem, 0});
    visitado[origem.linha][origem.coluna] = true;
    while (!fila.empty()) {
        auto [atual, passos] = fila.front();
        fila.pop();
        if (!iguais(atual, origem) && !celulaNoAlcance(mapa, origem, atual, alcanceExplosao)) return true;
        if (passos >= passosDisponiveis) continue;   // nao daria tempo de chegar
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(mapa, proxima.linha, proxima.coluna) ||
                visitado[proxima.linha][proxima.coluna] ||
                temInimigo(inimigos, proxima)) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            fila.push({proxima, passos + 1});
        }
    }
    return false;
}

// Vale a pena plantar uma bomba nesta celula? Vale se houver parede fragil ou
// inimigo dentro do alcance da explosao.
bool alvoParaBomba(const TipoCelula cenario[][COLUNAS], Posicao p, int alcance,
                   const vector<Inimigo>& lista) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int d = 0; d < 4; d++) {
        for (int distancia = 1; distancia <= alcance; distancia++) {
            int l = p.linha + direcoes[d][0] * distancia;
            int c = p.coluna + direcoes[d][1] * distancia;
            if (!dentro(l, c) || cenario[l][c] == PAREDE_SOLIDA) break;
            if (cenario[l][c] == PAREDE_FRAGIL || cenario[l][c] == PAREDE_BONUS) return true;
            if (temInimigo(lista, {l, c})) return true;
        }
    }
    return false;
}

// So considera um alvo valido se o bot tambem conseguir fugir de la depois de
// plantar a bomba; senao o bot ficaria mirando cantos fechados sem escape.
bool alvoParaBombaSeguro(Posicao p) {
    return alvoParaBomba(mapa, p, alcanceExplosao, inimigos) && existeFugaSegura(p);
}

// BFS generico usado duas vezes pelo bot. O parametro "procurarAlvoDeBomba"
// escolhe o que a busca considera destino:
//   true  -> a celula mais proxima onde vale a pena plantar uma bomba;
//   false -> a celula segura mais proxima, para fugir das bombas ja plantadas.
Posicao proximoPassoDoBot(bool procurarAlvoDeBomba) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};
    Posicao anterior[LINHAS][COLUNAS];
    queue<Posicao> fila;
    fila.push(jogador);
    visitado[jogador.linha][jogador.coluna] = true;
    Posicao objetivo = jogador;
    bool encontrou = false;

    while (!fila.empty() && !encontrou) {
        Posicao atual = fila.front();
        fila.pop();
        bool serve = procurarAlvoDeBomba ? alvoParaBombaSeguro(atual) : !perigoDaBomba(atual);
        if (serve) { objetivo = atual; encontrou = true; break; }
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(mapa, proxima.linha, proxima.coluna) ||
                visitado[proxima.linha][proxima.coluna] ||
                temInimigo(inimigos, proxima)) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            anterior[proxima.linha][proxima.coluna] = atual;
            fila.push(proxima);
        }
    }

    if (!encontrou || iguais(objetivo, jogador)) return jogador;
    while (!iguais(anterior[objetivo.linha][objetivo.coluna], jogador))
        objetivo = anterior[objetivo.linha][objetivo.coluna];
    return objetivo;
}

// Decisao do bot a cada tick, em ordem de prioridade:
//   1. tem bomba no mapa e eu estou na mira?  -> fugir;
//   2. daqui minha bomba acerta algo e eu escapo? -> plantar;
//   3. caso contrario -> andar na direcao do alvo mais proximo.
//
// O bot planta UMA bomba por vez mesmo depois de pegar a carta DUAS_BOMBAS. A
// checagem de fuga (existeFugaSegura) simula uma unica bomba; com duas no
// cenario ela poderia apontar como segura uma celula que a outra bomba cobre.
// Jogar conservador mantem o modo automatico confiavel na demonstracao.
void atualizarBot() {
    if (modo != AUTOMATICO || estado != JOGANDO || tempoMovimentoMs > 0) return;

    if (!bombas.empty()) {
        if (!perigoDaBomba(jogador)) return;         // ja esta seguro: fica parado
        Posicao destino = proximoPassoDoBot(false);  // false = procurar celula segura
        if (!iguais(destino, jogador))
            moverJogador(jogador, destino.linha - jogador.linha, destino.coluna - jogador.coluna);
        return;
    }
    if (alvoParaBombaSeguro(jogador)) {
        colocarBomba(bombas, jogador, maxBombas);
        return;
    }
    Posicao destino = proximoPassoDoBot(true);       // true = procurar alvo de bomba
    if (!iguais(destino, jogador))
        moverJogador(jogador, destino.linha - jogador.linha, destino.coluna - jogador.coluna);
}

/* ---------------------------------------------------------------------------
   11) ATUALIZACAO GERAL DA LOGICA

   Esta e a unica funcao chamada pelo timer do Windows. Toda a regra do jogo
   passa por aqui, sempre na mesma ordem.
   --------------------------------------------------------------------------- */

// [PDF - FUNCIONALIDADE 1]  Movimento em todas as direcoes.
// [PDF - FUNCIONALIDADE 4]  "O jogador se move sem bugs."
// Le o estado REAL das teclas a cada tick, em vez de depender da repeticao
// automatica do WM_KEYDOWN, que segue o atraso configurado no Windows e dava a
// sensacao de travamento ao segurar uma tecla.
void processarMovimentoContinuo() {
    if (modo != MANUAL) return;
    bool cima     = teclaPressionada['W'] || teclaPressionada[VK_UP];
    bool baixo    = teclaPressionada['S'] || teclaPressionada[VK_DOWN];
    bool esquerda = teclaPressionada['A'] || teclaPressionada[VK_LEFT];
    bool direita  = teclaPressionada['D'] || teclaPressionada[VK_RIGHT];
    // else-if encadeado: uma direcao por vez, nunca diagonal.
    if (cima) moverJogador(jogador, -1, 0);
    else if (baixo) moverJogador(jogador, 1, 0);
    else if (esquerda) moverJogador(jogador, 0, -1);
    else if (direita) moverJogador(jogador, 0, 1);
}

void atualizar(int tempoMs) {
    if (estado != JOGANDO) return;                          // partida encerrada: nada se move
    tempoMovimentoMs = max(0, tempoMovimentoMs - tempoMs);  // desconta o cooldown do jogador
    processarMovimentoContinuo();                           // 1. o jogador anda
    if (modo == AUTOMATICO) atualizarBot();                 //    (ou o bot decide por ele)
    atualizarBombas(tempoMs, bombas);                       // 2. as bombas contam e explodem
    moverInimigos(tempoMs, inimigos);                       // 3. os inimigos andam

    // [PDF - FUNCIONALIDADE 8] Rede de seguranca: se um inimigo terminou o
    // turno em cima do jogador, e derrota mesmo que nenhum dos dois tenha
    // detectado a colisao no proprio movimento.
    if (temInimigo(inimigos, jogador)) estado = DERROTA;

    verificarVitoria(inimigos, estado);                     // 4. acabou?
    if (modo == AUTOMATICO && estado == DERROTA) reiniciar();  // o bot recomeca sozinho
}

/* ---------------------------------------------------------------------------
   12) CAMADA GRAFICA

   Daqui para baixo nao existe mais regra de jogo: este trecho apenas LE o
   estado e desenha. Essa separacao e o que o PDF chama de boa segmentacao
   [PDF - TECNICA 4]. Nao ha motor grafico (Unity, SDL, SFML): so Win32, GDI e
   GDI+, que ja vem com o Windows.
   --------------------------------------------------------------------------- */

// Estilo da janela: barra de titulo, menu do sistema e botao de minimizar.
// Nao ha WS_THICKFRAME nem WS_MAXIMIZEBOX porque o tabuleiro tem tamanho
// fixo, entao esticar a janela so deixaria faixas vazias. A constante fica em
// uma variavel para ser usada nos dois lugares que precisam dela: ao calcular
// o tamanho que cabe na tela e ao criar a janela de fato.
const DWORD ESTILO_JANELA = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

// Converte unidade base em pixel real conforme a escala atual.
int px(int valor) { return static_cast<int>(valor * escalaDpi + 0.5f); }

// Reduz `escalaDpi` se o jogo, no DPI do monitor, nao couber na area de
// trabalho (a tela menos a barra de tarefas).
//
// Por que isso existe: todo desenho passa por px(), que multiplica pela escala
// do Windows. Num notebook configurado em 125% ou 150%, uma janela de 988x974
// unidades viraria mais de 1200x1400 pixels reais e ficaria maior que a tela,
// e o que sairia cortado seria justamente o RODAPE, onde aparecem as mensagens
// de vitoria e de derrota. Limitando a escala aqui, o jogo se reduz sozinho o
// quanto for preciso e continua inteiro em qualquer tela, sem que nenhuma
// outra parte do codigo precise saber disso.
// A CONTA, separada do Windows para poder ser testada sozinha: dado o espaco
// que sobra na tela e a escala que o monitor pediu, qual escala usar?
// Fica com a menor entre "o que cabe na largura" e "o que cabe na altura", e
// nunca aumenta a escala pedida pelo monitor, so reduz quando preciso.
float escalaQueCabe(int larguraDisponivel, int alturaDisponivel, float escalaDesejada) {
    float cabeEmLargura = static_cast<float>(larguraDisponivel) / LARGURA_JOGO;
    float cabeEmAltura = static_cast<float>(alturaDisponivel) / ALTURA_JOGO;
    float maximoQueCabe = min(cabeEmLargura, cabeEmAltura);
    if (maximoQueCabe >= escalaDesejada) return escalaDesejada;  // ja cabe: nao mexe
    return max(maximoQueCabe, ESCALA_MINIMA);                    // reduz, respeitando o piso
}

void ajustarEscalaParaCaberNaTela() {
    RECT areaTrabalho{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &areaTrabalho, 0)) return;

    // Quanto a moldura da janela (borda + barra de titulo) ocupa por fora.
    RECT moldura{0, 0, 0, 0};
    AdjustWindowRect(&moldura, ESTILO_JANELA, FALSE);

    escalaDpi = escalaQueCabe(
        areaTrabalho.right - areaTrabalho.left - (moldura.right - moldura.left),
        areaTrabalho.bottom - areaTrabalho.top - (moldura.bottom - moldura.top),
        escalaDpi);
}

// Descobre a pasta do executavel, para achar assets/ mesmo se o jogo for
// aberto de outro diretorio. GetModuleFileNameW e Win32.
wstring pastaDoExecutavel() {
    wchar_t caminho[MAX_PATH];
    DWORD tamanho = GetModuleFileNameW(nullptr, caminho, MAX_PATH);
    wstring pasta(caminho, tamanho);
    size_t separador = pasta.find_last_of(L"\\/");
    return separador == wstring::npos ? L"." : pasta.substr(0, separador);
}

// Carrega um PNG com GDI+. Se o arquivo faltar, avisa e devolve nullptr, mas
// o jogo continua funcionando e desenha duas letras no lugar do sprite.
unique_ptr<Gdiplus::Image> carregarSprite(const wstring& nome) {
    wstring caminho = pastaDoExecutavel() + L"\\assets\\" + nome;
    auto imagem = make_unique<Gdiplus::Image>(caminho.c_str());
    if (imagem->GetLastStatus() == Gdiplus::Ok) return imagem;
    wstring mensagem = L"Nao foi possivel carregar o asset:\n" + caminho +
                       L"\n\nO jogo usara uma representacao textual.";
    MessageBoxW(janelaPrincipal, mensagem.c_str(), L"Asset nao encontrado", MB_OK | MB_ICONWARNING);
    return nullptr;
}

// Os PNGs sao lidos UMA vez, na criacao da janela.
void carregarAssets() {
    spriteJogador = carregarSprite(L"bomberman.png");
    spriteInimigo = carregarSprite(L"perfil.png");
    spriteBomba = carregarSprite(L"bomba.png");
    spriteDuasBombas = carregarSprite(L"duas_bombas.png");
    spriteTeleporte = carregarSprite(L"teleport.png");
}

// Devolve ao Windows o contexto e o bitmap do buffer duplo (GDI).
void liberarBuffer() {
    if (bufferDC) { DeleteDC(bufferDC); bufferDC = nullptr; }
    if (bufferBitmap) { DeleteObject(bufferBitmap); bufferBitmap = nullptr; }
    bufferLargura = 0;
    bufferAltura = 0;
}

// Libera tudo o que foi alocado na camada grafica, no fechamento da janela.
void liberarAssets() {
    spriteJogador.reset();
    spriteInimigo.reset();
    spriteBomba.reset();
    spriteDuasBombas.reset();
    spriteTeleporte.reset();
    cacheFontes.clear();
    cacheMapa.reset();
    cacheMapaSujo = true;
    cacheSpriteJogador.reset();
    cacheSpriteInimigo.reset();
    cacheSpriteBomba.reset();
    cacheSpriteDuasBombas.reset();
    cacheSpriteTeleporte.reset();
    liberarBuffer();
}

// Garante um HDC/HBITMAP de memoria do tamanho pedido, reaproveitando entre
// frames. So recria quando o tamanho da janela muda de fato.
void garantirBuffer(HDC referencia, int largura, int altura) {
    if (bufferDC && bufferLargura == largura && bufferAltura == altura) return;
    liberarBuffer();
    bufferDC = CreateCompatibleDC(referencia);
    bufferBitmap = CreateCompatibleBitmap(referencia, largura, altura);
    SelectObject(bufferDC, bufferBitmap);
    bufferLargura = largura;
    bufferAltura = altura;
}

// Fontes GDI+ tambem sao caras de criar; cachear por tamanho em pixels evita
// recriar uma Font a cada chamada de desenharTexto (varias por frame).
Gdiplus::Font* obterFonte(int tamanhoPx) {
    auto it = cacheFontes.find(tamanhoPx);
    if (it != cacheFontes.end()) return it->second.get();
    auto fonte = make_unique<Gdiplus::Font>(L"Segoe UI", tamanhoPx, Gdiplus::FontStyleRegular,
                                            Gdiplus::UnitPixel);
    Gdiplus::Font* ponteiro = fonte.get();
    cacheFontes[tamanhoPx] = move(fonte);
    return ponteiro;
}

// Escreve texto na tela com GDI+ (DrawString).
void desenharTexto(Gdiplus::Graphics& g, const wstring& texto, float x, float y,
                   float tamanho, Gdiplus::Color cor, bool centralizado = false) {
    Gdiplus::Font* fonte = obterFonte(max(1, px(static_cast<int>(tamanho))));
    Gdiplus::SolidBrush pincel(cor);
    Gdiplus::StringFormat formato;
    if (centralizado) formato.SetAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF area(px(static_cast<int>(x)), px(static_cast<int>(y)),
                        px(LARGURA_JOGO - static_cast<int>(x) * 2), px(50));
    g.DrawString(texto.c_str(), -1, fonte, area, &formato, &pincel);
}

// Reduz o PNG original ao tamanho exato da celula UMA unica vez e guarda o
// resultado. O sprite dos inimigos tem 1254x1254 pixels; reamostrar essa
// imagem toda vez que um inimigo e desenhado (ate 8x por frame) consumia
// ~90ms/frame de CPU e travava o teclado. Cada sprite e reamostrado uma vez e
// reaproveitado em todos os quadros seguintes.
Gdiplus::Bitmap* prepararSpriteEscalado(unique_ptr<Gdiplus::Bitmap>& cache, Gdiplus::Image* origem) {
    int margem = px(4);
    float disponivel = static_cast<float>(max(1, px(TAMANHO_CELULA) - 2 * margem));
    float escala = min(disponivel / origem->GetWidth(), disponivel / origem->GetHeight());
    int largura = max(1, static_cast<int>(origem->GetWidth() * escala + 0.5f));
    int altura = max(1, static_cast<int>(origem->GetHeight() * escala + 0.5f));
    if (!cache || static_cast<int>(cache->GetWidth()) != largura ||
        static_cast<int>(cache->GetHeight()) != altura) {
        cache = make_unique<Gdiplus::Bitmap>(largura, altura, PixelFormat32bppPARGB);
        Gdiplus::Graphics gEscala(cache.get());
        gEscala.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gEscala.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        gEscala.DrawImage(origem, 0, 0, largura, altura);
    }
    return cache.get();
}

// Desenha um sprite centralizado na celula (linha, coluna). Se o PNG nao
// carregou, escreve as duas letras de fallback no lugar.
void desenharSpriteNaCelula(Gdiplus::Graphics& g, Gdiplus::Image* imagem,
                            unique_ptr<Gdiplus::Bitmap>& cacheEscalado,
                            int linha, int coluna, const wchar_t* fallback) {
    int xCelula = px(coluna * TAMANHO_CELULA);
    int yCelula = px(ALTURA_CABECALHO + linha * TAMANHO_CELULA);
    if (!imagem) {
        desenharTexto(g, fallback, coluna * TAMANHO_CELULA + 4,
                      ALTURA_CABECALHO + linha * TAMANHO_CELULA + 10, 18,
                      Gdiplus::Color(255, 255, 255, 255));
        return;
    }
    Gdiplus::Bitmap* pronto = prepararSpriteEscalado(cacheEscalado, imagem);
    int largura = static_cast<int>(pronto->GetWidth());
    int altura = static_cast<int>(pronto->GetHeight());
    int x = xCelula + (px(TAMANHO_CELULA) - largura) / 2;
    int y = yCelula + (px(TAMANHO_CELULA) - altura) / 2;
    g.DrawImage(pronto, x, y, largura, altura);
}

// [PDF - BOMBA] "Considere: o quadrado cinza parede solida; o quadrado azul
// parede fragil; o laranja explosao; e o preto a bomba."
// Cores do terreno, na convencao pedida pelo enunciado. FillRectangle,
// DrawRectangle, SolidBrush e Pen sao GDI+.
void desenharTerrenoCelula(Gdiplus::Graphics& g, const TipoCelula cenario[][COLUNAS], int l, int c) {
    int x = px(c * TAMANHO_CELULA);
    int y = px(l * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    Gdiplus::Color cor;
    if (cenario[l][c] == PAREDE_SOLIDA) cor = Gdiplus::Color(255, 105, 110, 115);       // cinza
    else if (cenario[l][c] == PAREDE_FRAGIL) cor = Gdiplus::Color(255, 45, 105, 180);   // azul
    else if (cenario[l][c] == PAREDE_BONUS) cor = Gdiplus::Color(255, 225, 175, 40);    // dourado
    else cor = Gdiplus::Color(255, 38, 128, 67);                                        // chao verde

    Gdiplus::SolidBrush fundo(cor);
    g.FillRectangle(&fundo, x, y, lado, lado);
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    if (cenario[l][c] == PAREDE_SOLIDA) {   // risco claro no meio da parede solida
        Gdiplus::Pen detalhe(Gdiplus::Color(150, 210, 215, 220), max(1, px(1)));
        g.DrawLine(&detalhe, x + px(5), y + lado / 2, x + lado - px(5), y + lado / 2);
    }
    if (cenario[l][c] == PAREDE_BONUS) {    // circulo que denuncia o bloco especial
        Gdiplus::Pen detalhe(Gdiplus::Color(220, 255, 255, 255), max(1, px(2)));
        g.DrawEllipse(&detalhe, x + px(10), y + px(10), lado - px(20), lado - px(20));
    }
}

// [PDF - FUNCIONALIDADE 13] "A explosao da bomba devera ser visivel."
// [PDF - BOMBA] "o laranja explosao"
// Pinta de laranja cada celula do vetor areaExplosao (que ja reune as chamas de
// todas as bombas que estao explodindo). Isso e desenhado POR CIMA do terreno,
// e nao dentro do cache, porque a explosao muda a cada quadro.
void desenharExplosaoOverlay(Gdiplus::Graphics& g, const vector<Posicao>& area) {
    if (area.empty()) return;
    Gdiplus::SolidBrush pincel(Gdiplus::Color(255, 255, 158, 35));   // <- a cor da explosao
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    int lado = px(TAMANHO_CELULA);
    for (const Posicao& p : area) {
        int x = px(p.coluna * TAMANHO_CELULA);
        int y = px(ALTURA_CABECALHO + p.linha * TAMANHO_CELULA);
        g.FillRectangle(&pincel, x, y, lado, lado);
        g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    }
}

// Desenha uma carta caida no chao: um halo claro para destacar do chao verde e,
// por cima, o PNG da carta (duas_bombas.png ou teleport.png).
void desenharItem(Gdiplus::Graphics& g, const Item& item) {
    int x = px(item.posicao.coluna * TAMANHO_CELULA);
    int y = px(ALTURA_CABECALHO + item.posicao.linha * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);

    Gdiplus::SolidBrush halo(Gdiplus::Color(90, 255, 255, 255));
    int margem = max(1, px(3));
    g.FillEllipse(&halo, x + margem, y + margem, lado - 2 * margem, lado - 2 * margem);

    if (item.tipo == DUAS_BOMBAS)
        desenharSpriteNaCelula(g, spriteDuasBombas.get(), cacheSpriteDuasBombas,
                               item.posicao.linha, item.posicao.coluna, L"2B");
    else
        desenharSpriteNaCelula(g, spriteTeleporte.get(), cacheSpriteTeleporte,
                               item.posicao.linha, item.posicao.coluna, L"TP");
}

// Desenha o tabuleiro inteiro, na ordem de tras para frente:
// terreno -> explosao -> itens -> bombas -> inimigos -> jogador.
// O terreno so e redesenhado quando o mapa muda de verdade; nos demais quadros
// e um unico DrawImage do bitmap ja pronto.
void desenharMapa(Gdiplus::Graphics& g) {
    int larguraGrid = px(LARGURA_JOGO);
    int alturaGrid = px(LINHAS * TAMANHO_CELULA);
    if (cacheMapaSujo || !cacheMapa || static_cast<int>(cacheMapa->GetWidth()) != larguraGrid ||
        static_cast<int>(cacheMapa->GetHeight()) != alturaGrid) {
        cacheMapa = make_unique<Gdiplus::Bitmap>(larguraGrid, alturaGrid, PixelFormat32bppPARGB);
        Gdiplus::Graphics gCache(cacheMapa.get());
        gCache.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        for (int l = 0; l < LINHAS; l++)
            for (int c = 0; c < COLUNAS; c++) desenharTerrenoCelula(gCache, mapa, l, c);
        cacheMapaSujo = false;
    }
    g.DrawImage(cacheMapa.get(), 0, px(ALTURA_CABECALHO));

    desenharExplosaoOverlay(g, areaExplosao);
    for (const Item& item : itensNoChao) desenharItem(g, item);

    // Cada bomba so aparece enquanto nao explodiu; depois quem aparece e o fogo.
    for (const Bomba& b : bombas)
        if (!b.explodindo)
            desenharSpriteNaCelula(g, spriteBomba.get(), cacheSpriteBomba,
                                   b.posicao.linha, b.posicao.coluna, L"BO");

    for (const Inimigo& inimigo : inimigos)
        if (inimigo.vivo)
            desenharSpriteNaCelula(g, spriteInimigo.get(), cacheSpriteInimigo,
                                   inimigo.posicao.linha, inimigo.posicao.coluna, L"IN");
    // O jogador e desenhado por ultimo para ficar por cima de tudo.
    desenharSpriteNaCelula(g, spriteJogador.get(), cacheSpriteJogador,
                           jogador.linha, jogador.coluna, L"PJ");
}

// Faixa superior: titulo, placar, inimigos restantes e aviso das bombas.
void desenharHUD(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_CABECALHO));
    desenharTexto(g, L"BOMBERMAN", 0, 8, 29, Gdiplus::Color(255, 65, 210, 235), true);

    wstring dados = L"Pontos: " + to_wstring(pontos) +
                    L"  Inimigos: " + to_wstring(inimigosVivos(inimigos)) +
                    L"  Raio bomba: " + to_wstring(alcanceExplosao) +
                    L"  Bombas: " + to_wstring(bombas.size()) + L"/" + to_wstring(maxBombas);
    if (modo == AUTOMATICO) dados += L"  AUTO 1,25x";
    if (maxBombas > MAX_BOMBAS_INICIAL) dados += L"  [DUAS BOMBAS]";
    if (possuiCartaTeleporte) dados += L"  [T] Teleporte";
    desenharTexto(g, dados, 18, 50, 15, Gdiplus::Color(255, 245, 245, 245));

    // Mostra a contagem da bomba que vai explodir primeiro.
    wstring aviso = L"Espaco coloca uma bomba";
    int menorTempo = -1;
    bool alguemExplodindo = false;
    for (const Bomba& b : bombas) {
        if (b.explodindo) alguemExplodindo = true;
        else if (menorTempo < 0 || b.tempoMs < menorTempo) menorTempo = b.tempoMs;
    }
    if (menorTempo >= 0) aviso = L"Bomba explode em: " + to_wstring((menorTempo + 999) / 1000) + L"s";
    else if (alguemExplodindo) aviso = L"BOOM! Afaste-se das chamas!";
    desenharTexto(g, aviso, 18, 76, 15, Gdiplus::Color(255, 255, 210, 85));
}

// Faixa inferior: lista de teclas, regra das cartas e a mensagem de vitoria
// ou derrota.
// [PDF - FUNCIONALIDADE 14 / 15] E aqui que o resultado da partida aparece
// para o jogador.
//
// A segunda linha explica o bloco dourado (PAREDE_BONUS) para quem esta vendo
// o jogo pela primeira vez: sem essa explicacao, a regra das cartas so fica
// clara depois de o jogador quebrar um bloco por acaso e ler o HUD.
void desenharRodape(Gdiplus::Graphics& g) {
    int y = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA;
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, px(y), px(LARGURA_JOGO), px(ALTURA_RODAPE));
    desenharTexto(g, L"WASD/SETAS: mover   ESPACO: bomba   T: teletransportar   R: reiniciar   M: menu   Q/ESC: sair",
                  12, y + 6, 12, Gdiplus::Color(255, 225, 230, 235), true);
    desenharTexto(g, L"BLOCO DOURADO: quebre para soltar uma carta, bomba dupla (2 bombas) ou teleporte (tecla T)",
                  12, y + 27, 12, Gdiplus::Color(255, 175, 210, 235), true);
    if (estado == VITORIA)
        desenharTexto(g, L"VOCE VENCEU! Pressione R.", 12, y + 52, 17,
                      Gdiplus::Color(255, 90, 225, 120), true);
    else if (estado == DERROTA)
        desenharTexto(g, L"GAME OVER! Pressione R.", 12, y + 52, 17,
                      Gdiplus::Color(255, 245, 85, 85), true);
}

// Tela inicial com a escolha do modo de jogo.
void desenharMenu(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO));
    desenharTexto(g, L"BOMBERMAN", 20, 150, 42, Gdiplus::Color(255, 65, 210, 235), true);
    desenharTexto(g, L"ESCOLHA O MODO DE JOGO", 20, 255, 21,
                  Gdiplus::Color(255, 245, 245, 245), true);
    desenharTexto(g, L"[1] Jogar manualmente", 20, 330, 24,
                  Gdiplus::Color(255, 90, 225, 120), true);
    desenharTexto(g, L"[2] Jogo automatico 1,25x", 20, 390, 24,
                  Gdiplus::Color(255, 255, 210, 85), true);
    desenharTexto(g, L"[Q ou ESC] Sair", 20, 470, 18,
                  Gdiplus::Color(255, 205, 210, 215), true);
}

// Monta o quadro inteiro fora da tela e copia de uma vez (BUFFER DUPLO).
// Sem isso, o usuario veria a janela sendo apagada e repintada por partes.
void desenhar(HWND janela) {
    PAINTSTRUCT ps;
    HDC destino = BeginPaint(janela, &ps);
    RECT cliente;
    GetClientRect(janela, &cliente);
    garantirBuffer(destino, cliente.right, cliente.bottom);
    Gdiplus::Graphics g(bufferDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    if (modo == MENU) desenharMenu(g);
    else {
        desenharHUD(g);
        desenharMapa(g);
        desenharRodape(g);
    }
    BitBlt(destino, 0, 0, cliente.right, cliente.bottom, bufferDC, 0, 0, SRCCOPY);
    EndPaint(janela, &ps);
}

/* ---------------------------------------------------------------------------
   13) JANELA, TECLADO E PONTO DE ENTRADA (Win32)
   --------------------------------------------------------------------------- */

// Acoes de toque unico (bomba, teleporte, reiniciar, menu, sair). O movimento
// NAO vem por aqui: ele e lido em processarMovimentoContinuo, a cada tick.
void lerTeclado(WPARAM tecla, LPARAM lParam) {
    // Bit 30 do lParam indica que o Windows esta repetindo a tecla segurada.
    // Ignoramos, senao segurar espaco tentaria plantar bomba varias vezes por segundo.
    bool repetida = (lParam & (1 << 30)) != 0;
    if (repetida) return;

    if (modo == MENU) {
        if (tecla == '1' || tecla == VK_NUMPAD1) iniciarPartida(MANUAL);
        else if (tecla == '2' || tecla == VK_NUMPAD2) iniciarPartida(AUTOMATICO);
        else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
        return;
    }

    if (tecla == 'M') modo = MENU;
    else if (tecla == ' ') colocarBomba(bombas, jogador, maxBombas);   // [PDF - FUNCIONALIDADE 5]
    else if (tecla == 'T') teletransportarJogador();
    else if (tecla == 'R') reiniciar();
    else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
}

// Ajusta a janela ao tamanho que a escala atual pede, mantendo a area util
// exatamente com LARGURA_JOGO x ALTURA_JOGO unidades.
void aplicarTamanhoDaJanela(HWND janela) {
    RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
    AdjustWindowRect(&area, ESTILO_JANELA, FALSE);
    SetWindowPos(janela, nullptr, 0, 0, area.right - area.left, area.bottom - area.top,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

// Recalcula a escala quando a janela muda de monitor ou de DPI, sempre
// respeitando o limite de espaco da tela.
void atualizarEscalaDpi(HWND janela) {
    escalaDpi = GetDpiForWindow(janela) / 96.0f;    // 96 DPI = 100%
    ajustarEscalaParaCaberNaTela();                 // e nunca maior do que cabe
    cacheFontes.clear();                            // as fontes ficaram do tamanho errado
    invalidarCacheMapa();                           // o terreno tambem muda de tamanho
}

// Callback da Win32: o Windows chama esta funcao para cada evento da janela.
// Toda a comunicacao entre o sistema e o jogo passa por aqui, nenhum evento
// chega por outro caminho.
LRESULT CALLBACK processarMensagem(HWND janela, UINT mensagem, WPARAM wParam, LPARAM lParam) {
    static auto ultimoInstante = chrono::steady_clock::now();   // <chrono>
    switch (mensagem) {
    case WM_CREATE:                 // a janela acabou de nascer
        janelaPrincipal = janela;
        atualizarEscalaDpi(janela);
        aplicarTamanhoDaJanela(janela);     // ajusta caso o DPI da janela difira do sistema
        carregarAssets();
        SetTimer(janela, 1, PASSO_LOGICA_MS, nullptr);  // dispara WM_TIMER a cada 25 ms
        return 0;

    case WM_TIMER: {                // batida do relogio: hora de atualizar a logica
        auto agora = chrono::steady_clock::now();
        // Mede o tempo REAL decorrido em vez de supor 25 ms: se o computador
        // engasgar, o jogo continua no mesmo ritmo.
        int decorrido = static_cast<int>(
            chrono::duration_cast<chrono::milliseconds>(agora - ultimoInstante).count());
        ultimoInstante = agora;
        double multiplicador = modo == AUTOMATICO ? 1.25 : 1.0;  // o bot joga acelerado
        atualizar(static_cast<int>(min(decorrido, 100) * multiplicador));
        InvalidateRect(janela, nullptr, FALSE);     // pede um WM_PAINT
        return 0;
    }

    case WM_KEYDOWN:                // tecla pressionada
        if (wParam < 256) teclaPressionada[wParam] = true;
        lerTeclado(wParam, lParam);
        InvalidateRect(janela, nullptr, FALSE);
        return 0;

    case WM_KEYUP:                  // tecla solta
        if (wParam < 256) teclaPressionada[wParam] = false;
        return 0;

    case WM_PAINT:                  // o Windows quer o conteudo da janela
        desenhar(janela);
        return 0;

    case WM_ERASEBKGND:             // nos ja pintamos o fundo inteiro: evita piscar
        return 1;

    case WM_DPICHANGED: {           // a janela foi para um monitor com outra escala
        RECT* sugerido = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(janela, nullptr, sugerido->left, sugerido->top, 0, 0,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
        atualizarEscalaDpi(janela);         // recalcula a escala, ja limitada a tela
        aplicarTamanhoDaJanela(janela);     // e redimensiona de acordo
        return 0;
    }

    case WM_KILLFOCUS:              // a janela perdeu o foco: esquece as teclas
        fill(begin(teclaPressionada), end(teclaPressionada), false);
        return 0;

    case WM_DESTROY:                // fechando: devolve tudo ao sistema
        KillTimer(janela, 1);
        liberarAssets();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wParam, lParam);   // o padrao cuida do resto
}

// Avisa ao Windows que o programa sabe lidar com telas de alta densidade.
void tornarAplicacaoDpiAware() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

// Ponto de entrada de um programa com janela no Windows (equivale ao main()).
int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int exibir) {
    tornarAplicacaoDpiAware();
    escalaDpi = GetDpiForSystem() / 96.0f;
    ajustarEscalaParaCaberNaTela();     // nunca maior do que cabe na area de trabalho

    // Abre a sessao do GDI+ antes de qualquer imagem ser carregada.
    Gdiplus::GdiplusStartupInput entradaGdiPlus;
    if (Gdiplus::GdiplusStartup(&tokenGdiPlus, &entradaGdiPlus, nullptr) != Gdiplus::Ok) return 1;

    // Registra o "molde" da janela, dizendo qual funcao trata as mensagens.
    const wchar_t CLASSE[] = L"BombermanUnivali";
    const wchar_t TITULO[] = L"Bomberman - Algoritmos e Programacao II";
    WNDCLASSW classe{};
    classe.lpfnWndProc = processarMensagem;     // <- nosso callback
    classe.hInstance = instancia;
    classe.lpszClassName = CLASSE;
    classe.hCursor = LoadCursor(nullptr, IDC_ARROW);
    classe.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&classe);

    // Calcula o tamanho externo da janela para que a AREA UTIL fique exatamente
    // com LARGURA_JOGO x ALTURA_JOGO (a borda e a barra de titulo somam por fora).
    RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
    AdjustWindowRect(&area, ESTILO_JANELA, FALSE);
    HWND janela = CreateWindowExW(0, CLASSE, TITULO, ESTILO_JANELA,
        CW_USEDEFAULT, CW_USEDEFAULT, area.right - area.left, area.bottom - area.top,
        nullptr, nullptr, instancia, nullptr);
    if (!janela) {
        Gdiplus::GdiplusShutdown(tokenGdiPlus);
        return 1;
    }
    ShowWindow(janela, exibir);
    UpdateWindow(janela);

    MSG mensagem;
    while (GetMessageW(&mensagem, nullptr, 0, 0) > 0) {
        TranslateMessage(&mensagem);
        DispatchMessageW(&mensagem);
    }
    Gdiplus::GdiplusShutdown(tokenGdiPlus);
    return 0;
}
