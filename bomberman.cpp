/*
Algoritmos e programação II, Trabalho M1
Desenvolvedores: Diego Silva |  Gabriel Bianchessi

Feito com matriz, structs, vetores e sub-rotinas. A interface usa Win32 e
GDI+ para abrir uma janela propria e desenhar os sprites PNG.

WASD/setas: mover | Espaco: bomba | R: reiniciar | Q ou ESC: sair
*/

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

const int LINHAS = 13;
const int COLUNAS = 15;
const int TEMPO_BOMBA_MS = 2200;
const int TEMPO_EXPLOSAO_MS = 650;
const int INTERVALO_MOVIMENTO_MS = 105;
const int ALCANCE_EXPLOSAO_INICIAL = 1;
const int ALCANCE_EXPLOSAO_MAXIMO = 3;
const int PASSO_LOGICA_MS = 25;
const int INTERVALO_INIMIGO_MIN_MS = 350;
const int INTERVALO_INIMIGO_MAX_MS = 700;
const int DISTANCIA_PERSEGUICAO = 6;
const int CHANCE_PERSEGUICAO = 65;
const int ALCANCE_CHUTE = 4;
const int TOTAL_INIMIGOS_INICIAL = 6;
#ifdef VERSAO_LEGADO
const int TAMANHO_CELULA = 56;
#else
const int TAMANHO_CELULA = 52;
#endif
const int ALTURA_CABECALHO = 104;
const int ALTURA_RODAPE = 72;
const int LARGURA_JOGO = COLUNAS * TAMANHO_CELULA;
const int ALTURA_JOGO = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA + ALTURA_RODAPE;

enum TipoCelula { VAZIO, PAREDE_SOLIDA, PAREDE_FRAGIL, PAREDE_BONUS };
enum EstadoJogo { JOGANDO, VITORIA, DERROTA };
enum ModoJogo { MENU, MANUAL, AUTOMATICO };
enum TipoItem { CHUTE, TELEPORTE };

struct Posicao { int linha, coluna; };
struct Inimigo { Posicao posicao; bool vivo; int tempoProximoMovimentoMs; };
struct Bomba {
    Posicao posicao{0, 0};
    bool ativa = false;
    bool explodindo = false;
    int tempoMs = 0;
};
struct Item { Posicao posicao; TipoItem tipo; };
struct BlocoBonus { Posicao posicao; TipoItem tipo; };

TipoCelula mapa[LINHAS][COLUNAS];
Posicao jogador{1, 1};
Posicao direcaoJogador{0, 1};
vector<Inimigo> inimigos;
vector<Posicao> areaExplosao;
vector<BlocoBonus> blocosBonus;
vector<Item> itensNoChao;
Bomba bomba;
EstadoJogo estado = JOGANDO;
ModoJogo modo = MENU;
int pontos = 0;
int alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
int tempoMovimentoMs = 0;
bool executando = true;
bool possuiCartaChute = false;
bool possuiCartaTeleporte = false;
bool teclaPressionada[256]{};
mt19937 gerador(random_device{}());
HWND janelaPrincipal = nullptr;
ULONG_PTR tokenGdiPlus = 0;
unique_ptr<Gdiplus::Image> spriteJogador;
unique_ptr<Gdiplus::Image> spriteInimigo;
unique_ptr<Gdiplus::Image> spriteBomba;
float escalaDpi = 1.0f;

// Backbuffer reaproveitado entre frames: recriar HDC/HBITMAP a cada WM_PAINT
// (antes: uma vez por frame a ~40fps) custava CPU real por nada, ja que o
// tamanho da janela quase nunca muda.
HDC bufferDC = nullptr;
HBITMAP bufferBitmap = nullptr;
int bufferLargura = 0;
int bufferAltura = 0;

// Fontes GDI+ tambem sao caras de criar; cachear por tamanho em pixels evita
// recriar uma Font a cada chamada de desenharTexto (varias por frame).
unordered_map<int, unique_ptr<Gdiplus::Font>> cacheFontes;

// Bitmap com o terreno do tabuleiro (paredes/chao) ja desenhado; so e
// reconstruido quando o mapa muda de verdade (reiniciar ou parede destruida).
unique_ptr<Gdiplus::Bitmap> cacheMapa;
bool cacheMapaSujo = true;

// Versoes pre-redimensionadas dos sprites, no tamanho final de tela (ver
// prepararSpriteEscalado). Evita reamostrar a imagem original a cada frame.
unique_ptr<Gdiplus::Bitmap> cacheSpriteJogador;
unique_ptr<Gdiplus::Bitmap> cacheSpriteInimigo;
unique_ptr<Gdiplus::Bitmap> cacheSpriteBomba;

void invalidarCacheMapa() { cacheMapaSujo = true; }

bool iguais(Posicao a, Posicao b) {
    return a.linha == b.linha && a.coluna == b.coluna;
}

bool dentro(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

bool livre(int linha, int coluna) {
    return dentro(linha, coluna) && mapa[linha][coluna] == VAZIO;
}

bool temInimigo(Posicao p, int ignorar = -1) {
    for (int i = 0; i < static_cast<int>(inimigos.size()); i++) {
        if (i != ignorar && inimigos[i].vivo && iguais(inimigos[i].posicao, p)) return true;
    }
    return false;
}

bool naExplosao(Posicao p) {
    for (Posicao parte : areaExplosao) if (iguais(parte, p)) return true;
    return false;
}

void limparArea(int linha, int coluna) {
    mapa[linha][coluna] = VAZIO;
    if (linha > 0) mapa[linha - 1][coluna] = VAZIO;
    if (linha < LINHAS - 1) mapa[linha + 1][coluna] = VAZIO;
    if (coluna > 0) mapa[linha][coluna - 1] = VAZIO;
    if (coluna < COLUNAS - 1) mapa[linha][coluna + 1] = VAZIO;
}

void criarMapa() {
    uniform_int_distribution<int> chance(0, 99);
    for (int l = 0; l < LINHAS; l++) {
        for (int c = 0; c < COLUNAS; c++) {
            bool borda = l == 0 || l == LINHAS - 1 || c == 0 || c == COLUNAS - 1;
            bool pilar = l % 2 == 0 && c % 2 == 0;
            if (borda || pilar) mapa[l][c] = PAREDE_SOLIDA;
            else if (chance(gerador) < 36) mapa[l][c] = PAREDE_FRAGIL;
            else mapa[l][c] = VAZIO;
        }
    }
}

vector<Posicao> posicoesLivresParaBonus() {
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (mapa[l][c] == PAREDE_FRAGIL) livres.push_back({l, c});
    return livres;
}

void posicionarBlocosBonus() {
    blocosBonus.clear();
    vector<Posicao> candidatos = posicoesLivresParaBonus();
    shuffle(candidatos.begin(), candidatos.end(), gerador);
    vector<TipoItem> tipos = {CHUTE, TELEPORTE};
    for (int i = 0; i < 2 && i < static_cast<int>(candidatos.size()); i++) {
        mapa[candidatos[i].linha][candidatos[i].coluna] = PAREDE_BONUS;
        blocosBonus.push_back({candidatos[i], tipos[i]});
    }
}

void sortearPosicoesIniciais(vector<Posicao>& posicoes) {
    // Cantos do mapa mais os pontos medios das bordas: da para o jogador e
    // os inimigos comecarem em posicoes diferentes a cada partida.
    posicoes = {{1, 1}, {1, COLUNAS - 2}, {LINHAS - 2, 1}, {LINHAS - 2, COLUNAS - 2},
                {1, COLUNAS / 2}, {LINHAS - 2, COLUNAS / 2},
                {LINHAS / 2, 1}, {LINHAS / 2, COLUNAS - 2}};
    shuffle(posicoes.begin(), posicoes.end(), gerador);
    for (const Posicao& p : posicoes) limparArea(p.linha, p.coluna);
}

void reiniciar() {
    criarMapa();
    vector<Posicao> spawns;
    sortearPosicoesIniciais(spawns);
    posicionarBlocosBonus();
    invalidarCacheMapa();
    jogador = spawns[0];
    direcaoJogador = {0, 1};
    inimigos.clear();
    uniform_int_distribution<int> sorteioIntervalo(INTERVALO_INIMIGO_MIN_MS, INTERVALO_INIMIGO_MAX_MS);
    for (int i = 1; i <= TOTAL_INIMIGOS_INICIAL; i++)
        inimigos.push_back({spawns[i], true, sorteioIntervalo(gerador)});
    itensNoChao.clear();
    possuiCartaChute = false;
    possuiCartaTeleporte = false;
    alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
    bomba = Bomba{};
    areaExplosao.clear();
    estado = JOGANDO;
    pontos = 0;
    tempoMovimentoMs = 0;
}

void iniciarPartida(ModoJogo novoModo) {
    modo = novoModo;
    reiniciar();
}

void coletarItens() {
    for (size_t i = 0; i < itensNoChao.size(); i++) {
        if (iguais(itensNoChao[i].posicao, jogador)) {
            if (itensNoChao[i].tipo == CHUTE) possuiCartaChute = true;
            else possuiCartaTeleporte = true;
            itensNoChao.erase(itensNoChao.begin() + i);
            return;
        }
    }
}

void moverJogador(int dl, int dc) {
    direcaoJogador = {dl, dc};
    if (estado != JOGANDO || tempoMovimentoMs > 0) return;
    Posicao destino{jogador.linha + dl, jogador.coluna + dc};
    // A bomba nao impede o movimento do jogador.
    if (livre(destino.linha, destino.coluna)) jogador = destino;
    if (temInimigo(jogador) || naExplosao(jogador)) estado = DERROTA;
    coletarItens();
    tempoMovimentoMs = INTERVALO_MOVIMENTO_MS;
}

void colocarBomba() {
    if (estado != JOGANDO || bomba.ativa) return;
    bomba.posicao = jogador;
    bomba.ativa = true;
    bomba.explodindo = false;
    bomba.tempoMs = TEMPO_BOMBA_MS;
}

void chutarBomba() {
    if (estado != JOGANDO || !possuiCartaChute || !bomba.ativa || bomba.explodindo) return;
    Posicao esperado{jogador.linha + direcaoJogador.linha, jogador.coluna + direcaoJogador.coluna};
    if (!iguais(bomba.posicao, esperado)) return;
    Posicao destinoFinal = bomba.posicao;
    for (int passo = 1; passo <= ALCANCE_CHUTE; passo++) {
        Posicao tentativa{bomba.posicao.linha + direcaoJogador.linha * passo,
                          bomba.posicao.coluna + direcaoJogador.coluna * passo};
        if (!livre(tentativa.linha, tentativa.coluna) || temInimigo(tentativa)) break;
        destinoFinal = tentativa;
    }
    bomba.posicao = destinoFinal;
    possuiCartaChute = false;
}

void teletransportarJogador() {
    if (estado != JOGANDO || !possuiCartaTeleporte) return;
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (livre(l, c) && !temInimigo({l, c}) && !(bomba.ativa && iguais({l, c}, bomba.posicao)))
                livres.push_back({l, c});
    if (livres.empty()) return;
    uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
    jogador = livres[sorteio(gerador)];
    if (temInimigo(jogador) || naExplosao(jogador)) estado = DERROTA;
    coletarItens();
    possuiCartaTeleporte = false;
}

void destruirBlocoBonus(int linha, int coluna) {
    for (size_t i = 0; i < blocosBonus.size(); i++) {
        if (blocosBonus[i].posicao.linha == linha && blocosBonus[i].posicao.coluna == coluna) {
            itensNoChao.push_back({blocosBonus[i].posicao, blocosBonus[i].tipo});
            blocosBonus.erase(blocosBonus.begin() + i);
            return;
        }
    }
}

void adicionarRaio(int dl, int dc) {
    for (int distancia = 1; distancia <= alcanceExplosao; distancia++) {
        int l = bomba.posicao.linha + dl * distancia;
        int c = bomba.posicao.coluna + dc * distancia;
        if (!dentro(l, c) || mapa[l][c] == PAREDE_SOLIDA) return;
        areaExplosao.push_back({l, c});
        if (mapa[l][c] == PAREDE_FRAGIL) {
            mapa[l][c] = VAZIO;
            invalidarCacheMapa();
            return;
        }
        if (mapa[l][c] == PAREDE_BONUS) {
            mapa[l][c] = VAZIO;
            destruirBlocoBonus(l, c);
            invalidarCacheMapa();
            return;
        }
    }
}

void matarInimigo(Inimigo& inimigo, int& pontuacao) {
    inimigo.vivo = false;
    pontuacao += 100;
    // A cada inimigo derrotado a bomba fica mais forte, ate o limite maximo.
    if (alcanceExplosao < ALCANCE_EXPLOSAO_MAXIMO) alcanceExplosao++;
}

void atingirPersonagens(vector<Inimigo>& listaInimigos,
                        EstadoJogo& estadoAtual, int& pontuacao) {
    // Os dados recebidos por referencia sao alterados diretamente.
    if (naExplosao(jogador)) estadoAtual = DERROTA;
    for (Inimigo& inimigo : listaInimigos) {
        if (inimigo.vivo && naExplosao(inimigo.posicao)) matarInimigo(inimigo, pontuacao);
    }
}

void iniciarExplosao() {
    bomba.explodindo = true;
    bomba.tempoMs = TEMPO_EXPLOSAO_MS;
    areaExplosao.clear();
    areaExplosao.push_back(bomba.posicao);
    adicionarRaio(-1, 0);
    adicionarRaio(1, 0);
    adicionarRaio(0, -1);
    adicionarRaio(0, 1);
    atingirPersonagens(inimigos, estado, pontos);
}

void atualizarBomba(int tempoMs) {
    if (!bomba.ativa) return;
    bomba.tempoMs -= tempoMs;
    if (!bomba.explodindo && bomba.tempoMs <= 0) iniciarExplosao();
    else if (bomba.explodindo) {
        atingirPersonagens(inimigos, estado, pontos);
        if (bomba.tempoMs <= 0) {
            bomba.ativa = false;
            bomba.explodindo = false;
            areaExplosao.clear();
        }
    }
}

Posicao proximoPassoParaAlvo(Posicao origem, Posicao alvo) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};
    Posicao anterior[LINHAS][COLUNAS];
    queue<Posicao> fila;
    fila.push(origem);
    visitado[origem.linha][origem.coluna] = true;
    bool encontrou = false;

    while (!fila.empty() && !encontrou) {
        Posicao atual = fila.front();
        fila.pop();
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(proxima.linha, proxima.coluna) || visitado[proxima.linha][proxima.coluna]) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            anterior[proxima.linha][proxima.coluna] = atual;
            if (iguais(proxima, alvo)) { encontrou = true; break; }
            fila.push(proxima);
        }
    }

    if (!encontrou) return origem;
    Posicao passo = alvo;
    while (!iguais(anterior[passo.linha][passo.coluna], origem))
        passo = anterior[passo.linha][passo.coluna];
    return passo;
}

void moverIndividualAleatorio(Inimigo& inimigo, int indice) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    uniform_int_distribution<int> sorteioDirecao(0, 3);
    uniform_int_distribution<int> sorteioPassos(1, 3);
    int inicio = sorteioDirecao(gerador);
    int passos = sorteioPassos(gerador);
    for (int tentativa = 0; tentativa < 4; tentativa++) {
        int d = (inicio + tentativa) % 4;
        bool moveu = false;
        for (int passo = 0; passo < passos; passo++) {
            Posicao destino{inimigo.posicao.linha + direcoes[d][0], inimigo.posicao.coluna + direcoes[d][1]};
            if (!livre(destino.linha, destino.coluna)) break;
            if (bomba.ativa && iguais(destino, bomba.posicao)) break;
            if (temInimigo(destino, indice)) break;
            inimigo.posicao = destino;
            moveu = true;
            if (iguais(destino, jogador)) estado = DERROTA;
            if (naExplosao(destino)) { matarInimigo(inimigo, pontos); break; }
        }
        if (moveu) break;
    }
}

void moverIndividualPerseguicao(Inimigo& inimigo, int indice, int passos) {
    for (int passo = 0; passo < passos; passo++) {
        Posicao alvo = proximoPassoParaAlvo(inimigo.posicao, jogador);
        bool bloqueado = iguais(alvo, inimigo.posicao) || temInimigo(alvo, indice) ||
                          (bomba.ativa && iguais(alvo, bomba.posicao));
        if (bloqueado) return;
        inimigo.posicao = alvo;
        if (iguais(alvo, jogador)) { estado = DERROTA; return; }
        if (naExplosao(alvo)) { matarInimigo(inimigo, pontos); return; }
    }
}

void moverInimigos(int tempoMs) {
    // Cada inimigo tem seu proprio relogio (sorteado em INTERVALO_INIMIGO_MIN/MAX_MS),
    // entao eles nao andam mais todos em sincronia. Perto do jogador, ha uma
    // chance de perseguir usando o caminho mais curto (mapeado com BFS, 1 a 3
    // passos por vez, na mesma velocidade do passeio aleatorio); caso
    // contrario, mantem o passeio aleatorio original.
    uniform_int_distribution<int> sorteioIntervalo(INTERVALO_INIMIGO_MIN_MS, INTERVALO_INIMIGO_MAX_MS);
    uniform_int_distribution<int> sorteioChance(0, 99);
    uniform_int_distribution<int> sorteioPassos(1, 3);
    for (int i = 0; i < static_cast<int>(inimigos.size()); i++) {
        Inimigo& inimigo = inimigos[i];
        if (!inimigo.vivo) continue;
        inimigo.tempoProximoMovimentoMs -= tempoMs;
        if (inimigo.tempoProximoMovimentoMs > 0) continue;
        inimigo.tempoProximoMovimentoMs = sorteioIntervalo(gerador);

        int distancia = abs(inimigo.posicao.linha - jogador.linha) + abs(inimigo.posicao.coluna - jogador.coluna);
        bool perseguir = distancia <= DISTANCIA_PERSEGUICAO && sorteioChance(gerador) < CHANCE_PERSEGUICAO;
        if (perseguir) moverIndividualPerseguicao(inimigo, i, sorteioPassos(gerador));
        else moverIndividualAleatorio(inimigo, i);
    }
}

void verificarVitoria() {
    if (estado != JOGANDO) return;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) return;
    estado = VITORIA;
}

// Logica pura de "essa celula seria atingida por uma bomba plantada em
// origemBomba", sem depender da bomba de verdade estar ativa. Usada tanto
// para o perigo da bomba real (perigoDaBomba) quanto para o bot simular,
// antes de plantar, se uma bomba hipotetica ali teria escape (existeFugaSegura).
bool celulaNoAlcance(Posicao origemBomba, Posicao p) {
    if (iguais(p, origemBomba)) return true;
    int dl = p.linha - origemBomba.linha;
    int dc = p.coluna - origemBomba.coluna;
    if (dl != 0 && dc != 0) return false;
    int distancia = abs(dl) + abs(dc);
    if (distancia > alcanceExplosao) return false;
    int passoL = (dl > 0) - (dl < 0);
    int passoC = (dc > 0) - (dc < 0);
    for (int i = 1; i <= distancia; i++) {
        int l = origemBomba.linha + passoL * i;
        int c = origemBomba.coluna + passoC * i;
        if (mapa[l][c] == PAREDE_SOLIDA) return false;
        if (mapa[l][c] == PAREDE_FRAGIL || mapa[l][c] == PAREDE_BONUS) return i == distancia;
    }
    return true;
}

bool perigoDaBomba(Posicao p) {
    if (!bomba.ativa) return false;
    return celulaNoAlcance(bomba.posicao, p);
}

// BFS a partir de `origem` simulando uma bomba plantada ali: existe alguma
// celula fora do alcance da explosao, alcancavel andando (sem atravessar
// paredes/inimigos) dentro do tempo que a bomba demora pra explodir? Sem essa
// checagem, o bot plantava bomba em cantos fechados (comuns perto do spawn,
// cercados por pilares) e ficava sem para onde fugir, se matando sozinho.
bool existeFugaSegura(Posicao origem) {
    int passosDisponiveis = TEMPO_BOMBA_MS / INTERVALO_MOVIMENTO_MS;
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};
    queue<pair<Posicao, int>> fila;
    fila.push({origem, 0});
    visitado[origem.linha][origem.coluna] = true;
    while (!fila.empty()) {
        auto [atual, passos] = fila.front();
        fila.pop();
        if (!iguais(atual, origem) && !celulaNoAlcance(origem, atual)) return true;
        if (passos >= passosDisponiveis) continue;
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(proxima.linha, proxima.coluna) || visitado[proxima.linha][proxima.coluna] ||
                temInimigo(proxima)) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            fila.push({proxima, passos + 1});
        }
    }
    return false;
}

bool alvoParaBomba(Posicao p) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int d = 0; d < 4; d++) {
        for (int distancia = 1; distancia <= alcanceExplosao; distancia++) {
            int l = p.linha + direcoes[d][0] * distancia;
            int c = p.coluna + direcoes[d][1] * distancia;
            if (!dentro(l, c) || mapa[l][c] == PAREDE_SOLIDA) break;
            if (mapa[l][c] == PAREDE_FRAGIL || mapa[l][c] == PAREDE_BONUS) return true;
            if (temInimigo({l, c})) return true;
        }
    }
    return false;
}

// So considera um alvo valido se o bot tambem conseguir fugir de la depois de
// plantar a bomba; senao o bot ficaria mirando cantos fechados sem escape.
bool alvoParaBombaSeguro(Posicao p) {
    return alvoParaBomba(p) && existeFugaSegura(p);
}

Posicao proximoPassoDoBot() {
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
        if (alvoParaBombaSeguro(atual)) {
            objetivo = atual;
            encontrou = true;
            break;
        }
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(proxima.linha, proxima.coluna) ||
                visitado[proxima.linha][proxima.coluna] || temInimigo(proxima)) continue;
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

// BFS ate a celula segura mais proxima (fora do alcance da bomba ativa), no
// mesmo molde de proximoPassoDoBot. Substitui a fuga antiga, que so olhava 1
// passo a frente e podia oscilar de volta pra cima da propria bomba.
Posicao proximoPassoParaFuga() {
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
        if (!perigoDaBomba(atual)) {
            objetivo = atual;
            encontrou = true;
            break;
        }
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!livre(proxima.linha, proxima.coluna) ||
                visitado[proxima.linha][proxima.coluna] || temInimigo(proxima)) continue;
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

void atualizarBot() {
    // Decide a cada tick de logica (nao mais a cada INTERVALO_BOT_MS), mas o
    // movimento em si continua pausado pelo mesmo cooldown do jogador manual
    // (tempoMovimentoMs, dentro de moverJogador). Isso elimina a dessincronia
    // entre "quando o bot decide" e "quando o jogo deixa mover", que causava
    // o personagem parecendo travar/bater em paredes no modo automatico.
    if (modo != AUTOMATICO || estado != JOGANDO || tempoMovimentoMs > 0) return;
    if (bomba.ativa) {
        if (!perigoDaBomba(jogador)) return;
        Posicao destino = proximoPassoParaFuga();
        if (!iguais(destino, jogador)) moverJogador(destino.linha - jogador.linha, destino.coluna - jogador.coluna);
        return;
    }
    if (alvoParaBombaSeguro(jogador)) {
        colocarBomba();
        return;
    }
    Posicao destino = proximoPassoDoBot();
    if (!iguais(destino, jogador)) moverJogador(destino.linha - jogador.linha, destino.coluna - jogador.coluna);
}

void processarMovimentoContinuo() {
    // Le o estado real das teclas a cada tick de logica, em vez de depender
    // do WM_KEYDOWN, que segue o delay/taxa de repeticao configurados no
    // Windows e causa a sensacao de input lag/travamento ao segurar uma tecla.
    if (modo != MANUAL) return;
    bool cima = teclaPressionada['W'] || teclaPressionada[VK_UP];
    bool baixo = teclaPressionada['S'] || teclaPressionada[VK_DOWN];
    bool esquerda = teclaPressionada['A'] || teclaPressionada[VK_LEFT];
    bool direita = teclaPressionada['D'] || teclaPressionada[VK_RIGHT];
    if (cima) moverJogador(-1, 0);
    else if (baixo) moverJogador(1, 0);
    else if (esquerda) moverJogador(0, -1);
    else if (direita) moverJogador(0, 1);
}

void atualizar(int tempoMs) {
    if (estado != JOGANDO) return;
    tempoMovimentoMs = max(0, tempoMovimentoMs - tempoMs);
    processarMovimentoContinuo();
    if (modo == AUTOMATICO) atualizarBot();
    atualizarBomba(tempoMs);
    moverInimigos(tempoMs);
    if (temInimigo(jogador)) estado = DERROTA;
    verificarVitoria();
    if (modo == AUTOMATICO && estado == DERROTA) reiniciar();
}

int inimigosVivos() {
    int total = 0;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) total++;
    return total;
}

int px(int valor) { return static_cast<int>(valor * escalaDpi + 0.5f); }

wstring pastaDoExecutavel() {
    wchar_t caminho[MAX_PATH];
    DWORD tamanho = GetModuleFileNameW(nullptr, caminho, MAX_PATH);
    wstring pasta(caminho, tamanho);
    size_t separador = pasta.find_last_of(L"\\/");
    return separador == wstring::npos ? L"." : pasta.substr(0, separador);
}

unique_ptr<Gdiplus::Image> carregarSprite(const wstring& nome) {
    wstring caminho = pastaDoExecutavel() + L"\\assets\\" + nome;
    auto imagem = make_unique<Gdiplus::Image>(caminho.c_str());
    if (imagem->GetLastStatus() == Gdiplus::Ok) return imagem;
    // A build legada fica em uma subpasta e compartilha os assets da raiz.
    wstring caminhoCompartilhado = pastaDoExecutavel() + L"\\..\\assets\\" + nome;
    imagem = make_unique<Gdiplus::Image>(caminhoCompartilhado.c_str());
    if (imagem->GetLastStatus() == Gdiplus::Ok) return imagem;
    wstring mensagem = L"Nao foi possivel carregar o asset:\n" + caminho + L"\nNem em:\n" +
                       caminhoCompartilhado +
                       L"\n\nO jogo usara uma representacao textual.";
    MessageBoxW(janelaPrincipal, mensagem.c_str(), L"Asset nao encontrado", MB_OK | MB_ICONWARNING);
    return nullptr;
}

void carregarAssets() {
    spriteJogador = carregarSprite(L"bomberman.png");
    spriteInimigo = carregarSprite(L"perfil.png");
    spriteBomba = carregarSprite(L"bomba.png");
}

void liberarBuffer() {
    if (bufferDC) { DeleteDC(bufferDC); bufferDC = nullptr; }
    if (bufferBitmap) { DeleteObject(bufferBitmap); bufferBitmap = nullptr; }
    bufferLargura = 0;
    bufferAltura = 0;
}

void liberarAssets() {
    spriteJogador.reset();
    spriteInimigo.reset();
    spriteBomba.reset();
    cacheFontes.clear();
    cacheMapa.reset();
    cacheMapaSujo = true;
    cacheSpriteJogador.reset();
    cacheSpriteInimigo.reset();
    cacheSpriteBomba.reset();
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

Gdiplus::Font* obterFonte(int tamanhoPx) {
    auto it = cacheFontes.find(tamanhoPx);
    if (it != cacheFontes.end()) return it->second.get();
#ifdef VERSAO_LEGADO
    const wchar_t* nomeFonte = L"Consolas";
#else
    const wchar_t* nomeFonte = L"Segoe UI";
#endif
    auto fonte = make_unique<Gdiplus::Font>(nomeFonte, tamanhoPx, Gdiplus::FontStyleRegular,
                                            Gdiplus::UnitPixel);
    Gdiplus::Font* ponteiro = fonte.get();
    cacheFontes[tamanhoPx] = move(fonte);
    return ponteiro;
}

void desenharTexto(Gdiplus::Graphics& g, const wstring& texto, float x, float y,
                   float tamanho, Gdiplus::Color cor, bool centralizado = false) {
    Gdiplus::Font* fonte = obterFonte(px(static_cast<int>(tamanho)));
    Gdiplus::SolidBrush pincel(cor);
    Gdiplus::StringFormat formato;
    if (centralizado) formato.SetAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF area(px(static_cast<int>(x)), px(static_cast<int>(y)),
                        px(LARGURA_JOGO - static_cast<int>(x) * 2), px(50));
    g.DrawString(texto.c_str(), -1, fonte, area, &formato, &pincel);
}

// O sprite dos inimigos (perfil.png) veio na versao V2 com 1254x1254px.
// Reamostrar essa imagem gigante toda vez que um inimigo e desenhado (ate 6x
// por frame, mais jogador e bomba) e o que estava consumindo ~90ms/frame de
// CPU. A solucao e reamostrar uma unica vez para o tamanho final de tela e
// reaproveitar esse resultado pequeno em todos os frames seguintes.
Gdiplus::Bitmap* prepararSpriteEscalado(unique_ptr<Gdiplus::Bitmap>& cache, Gdiplus::Image* origem) {
    int margem = px(4);
    float disponivel = static_cast<float>(px(TAMANHO_CELULA) - 2 * margem);
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

void desenharSpriteNaCelula(Gdiplus::Graphics& g, Gdiplus::Image* imagem, unique_ptr<Gdiplus::Bitmap>& cacheEscalado,
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

// So o terreno (paredes/chao), sem a sobreposicao de explosao: isso e o que
// vai para o bitmap cacheado, ja que muda raramente (so quando uma bomba
// destroi uma parede ou o jogo reinicia). Coordenadas relativas ao proprio
// cache (sem o deslocamento do cabecalho).
void desenharTerrenoCelula(Gdiplus::Graphics& g, int l, int c) {
    int x = px(c * TAMANHO_CELULA);
    int y = px(l * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    Gdiplus::Color cor;
#ifdef VERSAO_LEGADO
    if (mapa[l][c] == PAREDE_SOLIDA) cor = Gdiplus::Color(255, 128, 128, 128);
    else if (mapa[l][c] == PAREDE_FRAGIL) cor = Gdiplus::Color(255, 0, 70, 180);
    else if (mapa[l][c] == PAREDE_BONUS) cor = Gdiplus::Color(255, 205, 165, 0);
    else cor = Gdiplus::Color(255, 0, 128, 0);
#else
    if (mapa[l][c] == PAREDE_SOLIDA) cor = Gdiplus::Color(255, 105, 110, 115);
    else if (mapa[l][c] == PAREDE_FRAGIL) cor = Gdiplus::Color(255, 45, 105, 180);
    else if (mapa[l][c] == PAREDE_BONUS) cor = Gdiplus::Color(255, 225, 175, 40);
    else cor = Gdiplus::Color(255, 38, 128, 67);
#endif
    Gdiplus::SolidBrush fundo(cor);
    g.FillRectangle(&fundo, x, y, lado, lado);
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    if (mapa[l][c] == PAREDE_SOLIDA) {
        Gdiplus::Pen detalhe(Gdiplus::Color(150, 210, 215, 220), max(1, px(1)));
        g.DrawLine(&detalhe, x + px(5), y + lado / 2, x + lado - px(5), y + lado / 2);
    }
    if (mapa[l][c] == PAREDE_BONUS) {
        Gdiplus::Pen detalhe(Gdiplus::Color(220, 255, 255, 255), max(1, px(2)));
        g.DrawEllipse(&detalhe, x + px(10), y + px(10), lado - px(20), lado - px(20));
    }
}

// A explosao muda a cada frame (avanca, expande, termina), entao nao pode
// entrar no cache do terreno; e desenhada por cima, mas so nas poucas
// celulas realmente afetadas (no maximo ~25), nao no grid inteiro.
void desenharExplosaoOverlay(Gdiplus::Graphics& g) {
    if (areaExplosao.empty()) return;
    Gdiplus::SolidBrush pincel(Gdiplus::Color(255, 255, 158, 35));
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    int lado = px(TAMANHO_CELULA);
    for (const Posicao& p : areaExplosao) {
        int x = px(p.coluna * TAMANHO_CELULA);
        int y = px(ALTURA_CABECALHO + p.linha * TAMANHO_CELULA);
        g.FillRectangle(&pincel, x, y, lado, lado);
        g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    }
}

void desenharItem(Gdiplus::Graphics& g, const Item& item) {
    int x = px(item.posicao.coluna * TAMANHO_CELULA);
    int y = px(ALTURA_CABECALHO + item.posicao.linha * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    Gdiplus::Color cor = item.tipo == CHUTE ? Gdiplus::Color(255, 235, 120, 40)
                                            : Gdiplus::Color(255, 80, 200, 235);
    Gdiplus::SolidBrush pincel(cor);
    int margem = px(8);
    g.FillEllipse(&pincel, x + margem, y + margem, lado - 2 * margem, lado - 2 * margem);
    desenharTexto(g, item.tipo == CHUTE ? L"E" : L"T", item.posicao.coluna * TAMANHO_CELULA + 16,
                  ALTURA_CABECALHO + item.posicao.linha * TAMANHO_CELULA + 12, 18,
                  Gdiplus::Color(255, 20, 20, 20));
}

void desenharMapa(Gdiplus::Graphics& g) {
    // O terreno (195 celulas) so muda quando uma bomba destroi parede ou o
    // jogo reinicia (cacheMapaSujo). Nos demais frames (a grande maioria)
    // reaproveita o bitmap ja pronto: 1 DrawImage em vez de ~400 chamadas
    // GDI+ por frame, que era o gargalo real de CPU/lentidao.
    int larguraGrid = px(LARGURA_JOGO);
    int alturaGrid = px(LINHAS * TAMANHO_CELULA);
    if (cacheMapaSujo || !cacheMapa || static_cast<int>(cacheMapa->GetWidth()) != larguraGrid ||
        static_cast<int>(cacheMapa->GetHeight()) != alturaGrid) {
        cacheMapa = make_unique<Gdiplus::Bitmap>(larguraGrid, alturaGrid, PixelFormat32bppPARGB);
        Gdiplus::Graphics gCache(cacheMapa.get());
        gCache.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        for (int l = 0; l < LINHAS; l++)
            for (int c = 0; c < COLUNAS; c++) desenharTerrenoCelula(gCache, l, c);
        cacheMapaSujo = false;
    }
    g.DrawImage(cacheMapa.get(), 0, px(ALTURA_CABECALHO));
    desenharExplosaoOverlay(g);
    for (const Item& item : itensNoChao) desenharItem(g, item);
    if (bomba.ativa && !bomba.explodindo)
        desenharSpriteNaCelula(g, spriteBomba.get(), cacheSpriteBomba, bomba.posicao.linha, bomba.posicao.coluna, L"BO");
    for (const Inimigo& inimigo : inimigos)
        if (inimigo.vivo) desenharSpriteNaCelula(g, spriteInimigo.get(), cacheSpriteInimigo, inimigo.posicao.linha,
                                                  inimigo.posicao.coluna, L"IN");
    desenharSpriteNaCelula(g, spriteJogador.get(), cacheSpriteJogador, jogador.linha, jogador.coluna, L"PJ");
}

void desenharHUD(Gdiplus::Graphics& g) {
#ifdef VERSAO_LEGADO
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 0, 0, 0));
#else
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
#endif
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_CABECALHO));
#ifdef VERSAO_LEGADO
    desenharTexto(g, L"+--------- BOMBERMAN LEGADO ---------+", 0, 8, 24,
                  Gdiplus::Color(255, 0, 220, 235), true);
#else
    desenharTexto(g, L"BOMBERMAN", 0, 8, 29, Gdiplus::Color(255, 65, 210, 235), true);
#endif
    wstring dados = L"Pontos: " + to_wstring(pontos) + L"  Inimigos: " + to_wstring(inimigosVivos()) +
                    L"  Raio bomba: " + to_wstring(alcanceExplosao);
    if (modo == AUTOMATICO) dados += L"  AUTO 1,25x";
    if (possuiCartaChute) dados += L"  [E] Chute";
    if (possuiCartaTeleporte) dados += L"  [T] Teleporte";
    desenharTexto(g, dados, 18, 50, 15, Gdiplus::Color(255, 245, 245, 245));
    wstring aviso = L"Espaco coloca uma bomba";
    if (bomba.ativa && !bomba.explodindo)
        aviso = L"Bomba explode em: " + to_wstring((bomba.tempoMs + 999) / 1000) + L"s";
    else if (bomba.explodindo) aviso = L"BOOM! Afaste-se das chamas!";
    desenharTexto(g, aviso, 18, 76, 15, Gdiplus::Color(255, 255, 210, 85));
}

void desenharRodape(Gdiplus::Graphics& g) {
    int y = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA;
#ifdef VERSAO_LEGADO
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 0, 0, 0));
#else
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
#endif
    g.FillRectangle(&fundo, 0, px(y), px(LARGURA_JOGO), px(ALTURA_RODAPE));
    desenharTexto(g, L"WASD/SETAS: mover  ESPACO: bomba  E: chutar bomba  T: teletransportar  R: reiniciar  M: menu  Q/ESC: sair",
                  12, y + 8, 13, Gdiplus::Color(255, 225, 230, 235), true);
    if (estado == VITORIA)
        desenharTexto(g, L"VOCE VENCEU! Pressione R.", 12, y + 37, 18,
                      Gdiplus::Color(255, 90, 225, 120), true);
    else if (estado == DERROTA)
        desenharTexto(g, L"GAME OVER! Pressione R.", 12, y + 37, 18,
                      Gdiplus::Color(255, 245, 85, 85), true);
}

void desenharMenu(Gdiplus::Graphics& g) {
#ifdef VERSAO_LEGADO
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 0, 0, 0));
#else
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
#endif
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO));
#ifdef VERSAO_LEGADO
    desenharTexto(g, L"+----------------------------------+", 20, 85, 26,
                  Gdiplus::Color(255, 0, 220, 235), true);
    desenharTexto(g, L"|       BOMBERMAN LEGADO          |", 20, 130, 30,
                  Gdiplus::Color(255, 0, 220, 235), true);
    desenharTexto(g, L"+----------------------------------+", 20, 180, 26,
                  Gdiplus::Color(255, 0, 220, 235), true);
#else
    desenharTexto(g, L"BOMBERMAN", 20, 130, 42, Gdiplus::Color(255, 65, 210, 235), true);
#endif
    desenharTexto(g, L"ESCOLHA O MODO DE JOGO", 20, 230, 21,
                  Gdiplus::Color(255, 245, 245, 245), true);
    desenharTexto(g, L"[1] Jogar manualmente", 20, 305, 24,
                  Gdiplus::Color(255, 90, 225, 120), true);
    desenharTexto(g, L"[2] Jogo automatico 1,25x", 20, 365, 24,
                  Gdiplus::Color(255, 255, 210, 85), true);
    desenharTexto(g, L"[Q ou ESC] Sair", 20, 445, 18,
                  Gdiplus::Color(255, 205, 210, 215), true);
}

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

void lerTeclado(WPARAM tecla, LPARAM lParam) {
    // Bit 30 do lParam indica auto-repeticao do SO; ignoramos para acoes
    // pontuais (senao segurar R/espaco dispara a acao varias vezes por
    // segundo na taxa de repeticao do teclado, fora do controle do jogo).
    bool repetida = (lParam & (1 << 30)) != 0;
    if (repetida) return;
    if (modo == MENU) {
        // Aceita tanto os numeros da fileira superior quanto o teclado
        // numerico (numpad), que usa codigos de tecla virtuais diferentes.
        if (tecla == '1' || tecla == VK_NUMPAD1) iniciarPartida(MANUAL);
        else if (tecla == '2' || tecla == VK_NUMPAD2) iniciarPartida(AUTOMATICO);
        else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
        return;
    }
    if (tecla == 'M') modo = MENU;
    else if (tecla == ' ') colocarBomba();
    else if (tecla == 'E') chutarBomba();
    else if (tecla == 'T') teletransportarJogador();
    else if (tecla == 'R') reiniciar();
    else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
}

void atualizarEscalaDpi(HWND janela) {
    escalaDpi = GetDpiForWindow(janela) / 96.0f;
    cacheFontes.clear();
}

LRESULT CALLBACK processarMensagem(HWND janela, UINT mensagem, WPARAM wParam, LPARAM lParam) {
    static auto ultimoInstante = chrono::steady_clock::now();
    switch (mensagem) {
    case WM_CREATE:
        janelaPrincipal = janela;
        atualizarEscalaDpi(janela);
        carregarAssets();
        SetTimer(janela, 1, PASSO_LOGICA_MS, nullptr);
        return 0;
    case WM_TIMER: {
        auto agora = chrono::steady_clock::now();
        int decorrido = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(agora - ultimoInstante).count());
        ultimoInstante = agora;
        double multiplicador = modo == AUTOMATICO ? 1.25 : 1.0;
        atualizar(static_cast<int>(min(decorrido, 100) * multiplicador));
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam < 256) teclaPressionada[wParam] = true;
        lerTeclado(wParam, lParam);
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    case WM_KEYUP:
        if (wParam < 256) teclaPressionada[wParam] = false;
        return 0;
    case WM_PAINT:
        desenhar(janela);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED: {
        RECT* sugerido = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(janela, nullptr, sugerido->left, sugerido->top,
                     sugerido->right - sugerido->left, sugerido->bottom - sugerido->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        atualizarEscalaDpi(janela);
        return 0;
    }
    case WM_KILLFOCUS:
        fill(begin(teclaPressionada), end(teclaPressionada), false);
        return 0;
    case WM_DESTROY:
        executando = false;
        KillTimer(janela, 1);
        liberarAssets();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wParam, lParam);
}

void tornarAplicacaoDpiAware() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int exibir) {
    tornarAplicacaoDpiAware();
    escalaDpi = GetDpiForSystem() / 96.0f;
    Gdiplus::GdiplusStartupInput entradaGdiPlus;
    if (Gdiplus::GdiplusStartup(&tokenGdiPlus, &entradaGdiPlus, nullptr) != Gdiplus::Ok) return 1;

#ifdef VERSAO_LEGADO
    const wchar_t CLASSE[] = L"BombermanUnivaliLegado";
    const wchar_t TITULO[] = L"Bomberman Legado - Algoritmos e Programacao II";
#else
    const wchar_t CLASSE[] = L"BombermanUnivali";
    const wchar_t TITULO[] = L"Bomberman - Algoritmos e Programacao II";
#endif
    WNDCLASSW classe{};
    classe.lpfnWndProc = processarMensagem;
    classe.hInstance = instancia;
    classe.lpszClassName = CLASSE;
    classe.hCursor = LoadCursor(nullptr, IDC_ARROW);
    classe.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&classe);

    RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
    AdjustWindowRect(&area, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND janela = CreateWindowExW(0, CLASSE, TITULO,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
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
