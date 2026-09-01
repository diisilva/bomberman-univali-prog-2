/*
Bomberman para console - Trabalho M1 (sem GLUT)
Desenvolvedores: PREENCHER COM OS NOMES DA EQUIPE

Feito com matriz, structs, vetores e sub-rotinas. As funcoes de console do
Windows servem somente para ler teclas sem Enter, colorir e redesenhar a tela.

WASD/setas: mover | Espaco: bomba | R: reiniciar | Q ou ESC: sair
*/

#include <windows.h>
#include <conio.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std;

const int LINHAS = 13;
const int COLUNAS = 15;
const int TEMPO_BOMBA_MS = 2200;
const int TEMPO_EXPLOSAO_MS = 650;
const int INTERVALO_INIMIGO_MS = 550;
const int INTERVALO_MOVIMENTO_MS = 105;
const int ALCANCE_EXPLOSAO = 2;
const int PASSO_LOGICA_MS = 25;

enum TipoCelula { VAZIO, PAREDE_SOLIDA, PAREDE_FRAGIL };
enum EstadoJogo { JOGANDO, VITORIA, DERROTA };

struct Posicao { int linha, coluna; };
struct Inimigo { Posicao posicao; bool vivo; };
struct Bomba {
    Posicao posicao{0, 0};
    bool ativa = false;
    bool explodindo = false;
    int tempoMs = 0;
};

TipoCelula mapa[LINHAS][COLUNAS];
Posicao jogador{1, 1};
vector<Inimigo> inimigos;
vector<Posicao> areaExplosao;
Bomba bomba;
EstadoJogo estado = JOGANDO;
int pontos = 0;
int tempoInimigosMs = 0;
int tempoMovimentoMs = 0;
bool executando = true;
mt19937 gerador(random_device{}());
HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

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
    limparArea(1, 1);
    limparArea(1, COLUNAS - 2);
    limparArea(LINHAS - 2, 1);
    limparArea(LINHAS - 2, COLUNAS - 2);
}

void reiniciar() {
    criarMapa();
    jogador = {1, 1};
    inimigos = {{{1, COLUNAS - 2}, true},
                 {{LINHAS - 2, 1}, true},
                 {{LINHAS - 2, COLUNAS - 2}, true},
                 {{LINHAS - 2, COLUNAS / 2}, true}};
    mapa[LINHAS - 2][COLUNAS / 2] = VAZIO;
    bomba = Bomba{};
    areaExplosao.clear();
    estado = JOGANDO;
    pontos = 0;
    tempoInimigosMs = 0;
    tempoMovimentoMs = 0;
}

void moverJogador(int dl, int dc) {
    if (estado != JOGANDO || tempoMovimentoMs > 0) return;
    Posicao destino{jogador.linha + dl, jogador.coluna + dc};
    // A bomba nao impede o movimento do jogador.
    if (livre(destino.linha, destino.coluna)) jogador = destino;
    if (temInimigo(jogador) || naExplosao(jogador)) estado = DERROTA;
    tempoMovimentoMs = INTERVALO_MOVIMENTO_MS;
}

void colocarBomba() {
    if (estado != JOGANDO || bomba.ativa) return;
    bomba.posicao = jogador;
    bomba.ativa = true;
    bomba.explodindo = false;
    bomba.tempoMs = TEMPO_BOMBA_MS;
}

void adicionarRaio(int dl, int dc) {
    for (int distancia = 1; distancia <= ALCANCE_EXPLOSAO; distancia++) {
        int l = bomba.posicao.linha + dl * distancia;
        int c = bomba.posicao.coluna + dc * distancia;
        if (!dentro(l, c) || mapa[l][c] == PAREDE_SOLIDA) return;
        areaExplosao.push_back({l, c});
        if (mapa[l][c] == PAREDE_FRAGIL) {
            mapa[l][c] = VAZIO;
            return;
        }
    }
}

void atingirPersonagens(vector<Inimigo>& listaInimigos,
                        EstadoJogo& estadoAtual, int& pontuacao) {
    // Os dados recebidos por referencia sao alterados diretamente.
    if (naExplosao(jogador)) estadoAtual = DERROTA;
    for (Inimigo& inimigo : listaInimigos) {
        if (inimigo.vivo && naExplosao(inimigo.posicao)) {
            inimigo.vivo = false;
            pontuacao += 100;
        }
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

void moverInimigos() {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    uniform_int_distribution<int> sorteioDirecao(0, 3);
    uniform_int_distribution<int> sorteioPassos(1, 3);

    for (int i = 0; i < static_cast<int>(inimigos.size()); i++) {
        if (!inimigos[i].vivo) continue;
        int inicio = sorteioDirecao(gerador);
        int passos = sorteioPassos(gerador);
        for (int tentativa = 0; tentativa < 4; tentativa++) {
            int d = (inicio + tentativa) % 4;
            bool moveu = false;
            for (int passo = 0; passo < passos; passo++) {
                Posicao destino{inimigos[i].posicao.linha + direcoes[d][0],
                                inimigos[i].posicao.coluna + direcoes[d][1]};
                if (!livre(destino.linha, destino.coluna)) break;
                if (bomba.ativa && iguais(destino, bomba.posicao)) break;
                if (temInimigo(destino, i)) break;
                inimigos[i].posicao = destino;
                moveu = true;
                if (iguais(destino, jogador)) estado = DERROTA;
                if (naExplosao(destino)) {
                    inimigos[i].vivo = false;
                    pontos += 100;
                    break;
                }
            }
            if (moveu) break;
        }
    }
}

void verificarVitoria() {
    if (estado != JOGANDO) return;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) return;
    estado = VITORIA;
}

void atualizar(int tempoMs) {
    if (estado != JOGANDO) return;
    tempoMovimentoMs = max(0, tempoMovimentoMs - tempoMs);
    atualizarBomba(tempoMs);
    tempoInimigosMs += tempoMs;
    if (tempoInimigosMs >= INTERVALO_INIMIGO_MS) {
        tempoInimigosMs = 0;
        moverInimigos();
    }
    if (temInimigo(jogador)) estado = DERROTA;
    verificarVitoria();
}

void cor(WORD valor) { SetConsoleTextAttribute(console, valor); }

void desenharBloco(WORD fundo, const string& texto = "  ", WORD frente = 15) {
    cor(static_cast<WORD>((fundo << 4) | frente));
    cout << texto;
}

void desenharCelula(int l, int c) {
    Posicao p{l, c};
    if (naExplosao(p)) desenharBloco(6, "**", 14);
    else if (iguais(jogador, p)) desenharBloco(9, "PJ", 15);
    else if (temInimigo(p)) desenharBloco(4, "IN", 15);
    else if (bomba.ativa && iguais(bomba.posicao, p)) desenharBloco(0, "BO", 14);
    else if (mapa[l][c] == PAREDE_SOLIDA) desenharBloco(8, "##", 7);
    else if (mapa[l][c] == PAREDE_FRAGIL) desenharBloco(1, "+=", 11);
    else desenharBloco(2, "  ", 2);
}

int inimigosVivos() {
    int total = 0;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) total++;
    return total;
}

void posicionarCursor(short x, short y) {
    SetConsoleCursorPosition(console, {x, y});
}

void desenhar() {
    posicionarCursor(0, 0);
    cor(11);
    cout << "+--------------------------------+\n";
    cout << "|       BOMBERMAN CONSOLE        |\n";
    cout << "+--------------------------------+\n";
    cor(15);
    cout << " Pontos: " << pontos << "   Inimigos: " << inimigosVivos() << "          \n";
    if (bomba.ativa && !bomba.explodindo)
        cout << " Bomba explode em: " << (bomba.tempoMs + 999) / 1000 << "s              \n";
    else if (bomba.explodindo) cout << " BOOM! Afaste-se das chamas!       \n";
    else cout << " Espaco coloca uma bomba           \n";

    for (int l = 0; l < LINHAS; l++) {
        cout << " ";
        for (int c = 0; c < COLUNAS; c++) desenharCelula(l, c);
        cor(15);
        cout << " \n";
    }
    cor(7);
    cout << " WASD/SETAS: mover  ESPACO: bomba  \n";
    cout << " R: reiniciar       Q/ESC: sair    \n";
    if (estado == VITORIA) { cor(10); cout << "       VOCE VENCEU! Pressione R.   \n"; }
    else if (estado == DERROTA) { cor(12); cout << "       GAME OVER! Pressione R.     \n"; }
    else cout << "                                    \n";
    cor(7);
    cout.flush();
}

void lerTeclado() {
    if (!_kbhit()) return;
    int tecla = _getch();
    if (tecla == 0 || tecla == 224) {
        int especial = _getch();
        if (especial == 72) moverJogador(-1, 0);
        else if (especial == 80) moverJogador(1, 0);
        else if (especial == 75) moverJogador(0, -1);
        else if (especial == 77) moverJogador(0, 1);
        return;
    }
    if (tecla == 'w' || tecla == 'W') moverJogador(-1, 0);
    else if (tecla == 's' || tecla == 'S') moverJogador(1, 0);
    else if (tecla == 'a' || tecla == 'A') moverJogador(0, -1);
    else if (tecla == 'd' || tecla == 'D') moverJogador(0, 1);
    else if (tecla == ' ') colocarBomba();
    else if (tecla == 'r' || tecla == 'R') reiniciar();
    else if (tecla == 'q' || tecla == 'Q' || tecla == 27) executando = false;
}

void prepararConsole() {
    // Configuracao visual do console.
    CONSOLE_FONT_INFOEX fonte{};
    fonte.cbSize = sizeof(CONSOLE_FONT_INFOEX);
    fonte.dwFontSize.X = 0;
    fonte.dwFontSize.Y = 28;
    fonte.FontFamily = FF_DONTCARE;
    fonte.FontWeight = FW_NORMAL;
    wcscpy_s(fonte.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(console, FALSE, &fonte);

    COORD tamanhoBuffer{38, 24};
    SetConsoleScreenBufferSize(console, tamanhoBuffer);
    SMALL_RECT tamanhoJanela{0, 0, 37, 23};
    SetConsoleWindowInfo(console, TRUE, &tamanhoJanela);

    CONSOLE_CURSOR_INFO cursor;
    GetConsoleCursorInfo(console, &cursor);
    cursor.bVisible = FALSE;
    SetConsoleCursorInfo(console, &cursor);
    SetConsoleTitleA("Bomberman Console - sem GLUT");
    system("cls");
}

int main() {
    prepararConsole();
    reiniciar();
    auto ultimoInstante = chrono::steady_clock::now();

    while (executando) {
        auto agora = chrono::steady_clock::now();
        int decorrido = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(agora - ultimoInstante).count());
        if (decorrido >= PASSO_LOGICA_MS) {
            ultimoInstante = agora;
            lerTeclado();
            atualizar(min(decorrido, 100));
            desenhar();
        }
        this_thread::sleep_for(chrono::milliseconds(2));
    }

    cor(7);
    posicionarCursor(0, 23);
    cout << "Jogo encerrado.                         \n";
    return 0;
}
