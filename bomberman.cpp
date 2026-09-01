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
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#endif

using namespace std;

const int LINHAS = 13;
const int COLUNAS = 15;
const int TEMPO_BOMBA_MS = 2200;
const int TEMPO_EXPLOSAO_MS = 650;
const int INTERVALO_INIMIGO_MS = 550;
const int INTERVALO_MOVIMENTO_MS = 105;
const int ALCANCE_EXPLOSAO = 2;
const int PASSO_LOGICA_MS = 25;
const int INTERVALO_BOT_MS = 170;
const int TAMANHO_CELULA = 52;
const int ALTURA_CABECALHO = 104;
const int ALTURA_RODAPE = 72;
const int LARGURA_JOGO = COLUNAS * TAMANHO_CELULA;
const int ALTURA_JOGO = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA + ALTURA_RODAPE;

enum TipoCelula { VAZIO, PAREDE_SOLIDA, PAREDE_FRAGIL };
enum EstadoJogo { JOGANDO, VITORIA, DERROTA };
enum ModoJogo { MENU, MANUAL, AUTOMATICO };

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
ModoJogo modo = MENU;
int pontos = 0;
int tempoInimigosMs = 0;
int tempoMovimentoMs = 0;
int tempoBotMs = 0;
bool executando = true;
mt19937 gerador(random_device{}());
HWND janelaPrincipal = nullptr;
ULONG_PTR tokenGdiPlus = 0;
unique_ptr<Gdiplus::Image> spriteJogador;
unique_ptr<Gdiplus::Image> spriteInimigo;
unique_ptr<Gdiplus::Image> spriteBomba;
float escalaDpi = 1.0f;

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
    tempoBotMs = 0;
}

void iniciarPartida(ModoJogo novoModo) {
    modo = novoModo;
    reiniciar();
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

bool perigoDaBomba(Posicao p) {
    if (!bomba.ativa) return false;
    if (iguais(p, bomba.posicao)) return true;
    int dl = p.linha - bomba.posicao.linha;
    int dc = p.coluna - bomba.posicao.coluna;
    if (dl != 0 && dc != 0) return false;
    int distancia = abs(dl) + abs(dc);
    if (distancia > ALCANCE_EXPLOSAO) return false;
    int passoL = (dl > 0) - (dl < 0);
    int passoC = (dc > 0) - (dc < 0);
    for (int i = 1; i <= distancia; i++) {
        int l = bomba.posicao.linha + passoL * i;
        int c = bomba.posicao.coluna + passoC * i;
        if (mapa[l][c] == PAREDE_SOLIDA) return false;
        if (mapa[l][c] == PAREDE_FRAGIL) return i == distancia;
    }
    return true;
}

bool alvoParaBomba(Posicao p) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int d = 0; d < 4; d++) {
        for (int distancia = 1; distancia <= ALCANCE_EXPLOSAO; distancia++) {
            int l = p.linha + direcoes[d][0] * distancia;
            int c = p.coluna + direcoes[d][1] * distancia;
            if (!dentro(l, c) || mapa[l][c] == PAREDE_SOLIDA) break;
            if (mapa[l][c] == PAREDE_FRAGIL) return true;
            if (temInimigo({l, c})) return true;
        }
    }
    return false;
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
        if (alvoParaBomba(atual)) {
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
    if (modo != AUTOMATICO || estado != JOGANDO) return;
    if (bomba.ativa) {
        if (!perigoDaBomba(jogador)) return;
        const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& direcao : direcoes) {
            Posicao destino{jogador.linha + direcao[0], jogador.coluna + direcao[1]};
            if (livre(destino.linha, destino.coluna) && !temInimigo(destino) &&
                !perigoDaBomba(destino)) {
                tempoMovimentoMs = 0;
                moverJogador(direcao[0], direcao[1]);
                return;
            }
        }
        for (const auto& direcao : direcoes) {
            Posicao destino{jogador.linha + direcao[0], jogador.coluna + direcao[1]};
            if (livre(destino.linha, destino.coluna) && !temInimigo(destino)) {
                tempoMovimentoMs = 0;
                moverJogador(direcao[0], direcao[1]);
                return;
            }
        }
        return;
    }
    if (alvoParaBomba(jogador)) {
        colocarBomba();
        return;
    }
    Posicao destino = proximoPassoDoBot();
    tempoMovimentoMs = 0;
    moverJogador(destino.linha - jogador.linha, destino.coluna - jogador.coluna);
}

void atualizar(int tempoMs) {
    if (estado != JOGANDO) return;
    tempoMovimentoMs = max(0, tempoMovimentoMs - tempoMs);
    tempoBotMs += tempoMs;
    if (modo == AUTOMATICO && tempoBotMs >= INTERVALO_BOT_MS) {
        tempoBotMs = 0;
        atualizarBot();
    }
    atualizarBomba(tempoMs);
    tempoInimigosMs += tempoMs;
    if (tempoInimigosMs >= INTERVALO_INIMIGO_MS) {
        tempoInimigosMs = 0;
        moverInimigos();
    }
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
    wstring mensagem = L"Nao foi possivel carregar o asset:\n" + caminho +
                       L"\n\nO jogo usara uma representacao textual.";
    MessageBoxW(janelaPrincipal, mensagem.c_str(), L"Asset nao encontrado", MB_OK | MB_ICONWARNING);
    return nullptr;
}

void carregarAssets() {
    spriteJogador = carregarSprite(L"bomberman.png");
    spriteInimigo = carregarSprite(L"perfil.png");
    spriteBomba = carregarSprite(L"bomba.png");
}

void liberarAssets() {
    spriteJogador.reset();
    spriteInimigo.reset();
    spriteBomba.reset();
}

void desenharTexto(Gdiplus::Graphics& g, const wstring& texto, float x, float y,
                   float tamanho, Gdiplus::Color cor, bool centralizado = false) {
    Gdiplus::Font fonte(L"Segoe UI", px(static_cast<int>(tamanho)), Gdiplus::FontStyleRegular,
                       Gdiplus::UnitPixel);
    Gdiplus::SolidBrush pincel(cor);
    Gdiplus::StringFormat formato;
    if (centralizado) formato.SetAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF area(px(static_cast<int>(x)), px(static_cast<int>(y)),
                        px(LARGURA_JOGO - static_cast<int>(x) * 2), px(50));
    g.DrawString(texto.c_str(), -1, &fonte, area, &formato, &pincel);
}

void desenharSpriteNaCelula(Gdiplus::Graphics& g, Gdiplus::Image* imagem,
                            int linha, int coluna, const wchar_t* fallback) {
    int margem = px(4);
    int xCelula = px(coluna * TAMANHO_CELULA);
    int yCelula = px(ALTURA_CABECALHO + linha * TAMANHO_CELULA);
    if (!imagem) {
        desenharTexto(g, fallback, coluna * TAMANHO_CELULA + 4,
                      ALTURA_CABECALHO + linha * TAMANHO_CELULA + 10, 18,
                      Gdiplus::Color(255, 255, 255, 255));
        return;
    }
    float disponivel = static_cast<float>(px(TAMANHO_CELULA) - 2 * margem);
    float escala = min(disponivel / imagem->GetWidth(), disponivel / imagem->GetHeight());
    int largura = static_cast<int>(imagem->GetWidth() * escala + 0.5f);
    int altura = static_cast<int>(imagem->GetHeight() * escala + 0.5f);
    int x = xCelula + (px(TAMANHO_CELULA) - largura) / 2;
    int y = yCelula + (px(TAMANHO_CELULA) - altura) / 2;
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.DrawImage(imagem, x, y, largura, altura);
}

void desenharCelula(Gdiplus::Graphics& g, int l, int c) {
    int x = px(c * TAMANHO_CELULA);
    int y = px(ALTURA_CABECALHO + l * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    Gdiplus::Color cor = mapa[l][c] == PAREDE_SOLIDA ? Gdiplus::Color(255, 105, 110, 115) :
                         mapa[l][c] == PAREDE_FRAGIL ? Gdiplus::Color(255, 45, 105, 180) :
                                                      Gdiplus::Color(255, 38, 128, 67);
    if (naExplosao({l, c})) cor = Gdiplus::Color(255, 255, 158, 35);
    Gdiplus::SolidBrush fundo(cor);
    g.FillRectangle(&fundo, x, y, lado, lado);
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    if (mapa[l][c] == PAREDE_SOLIDA) {
        Gdiplus::Pen detalhe(Gdiplus::Color(150, 210, 215, 220), max(1, px(1)));
        g.DrawLine(&detalhe, x + px(5), y + lado / 2, x + lado - px(5), y + lado / 2);
    }
}

void desenharMapa(Gdiplus::Graphics& g) {
    for (int l = 0; l < LINHAS; l++) for (int c = 0; c < COLUNAS; c++) desenharCelula(g, l, c);
    if (bomba.ativa && !bomba.explodindo)
        desenharSpriteNaCelula(g, spriteBomba.get(), bomba.posicao.linha, bomba.posicao.coluna, L"BO");
    for (const Inimigo& inimigo : inimigos)
        if (inimigo.vivo) desenharSpriteNaCelula(g, spriteInimigo.get(), inimigo.posicao.linha,
                                                  inimigo.posicao.coluna, L"IN");
    desenharSpriteNaCelula(g, spriteJogador.get(), jogador.linha, jogador.coluna, L"PJ");
}

void desenharHUD(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_CABECALHO));
    desenharTexto(g, L"BOMBERMAN", 0, 8, 29, Gdiplus::Color(255, 65, 210, 235), true);
    wstring dados = L"Pontos: " + to_wstring(pontos) + L"    Inimigos: " + to_wstring(inimigosVivos());
    if (modo == AUTOMATICO) dados += L"    AUTO 1,5x";
    desenharTexto(g, dados, 18, 50, 18, Gdiplus::Color(255, 245, 245, 245));
    wstring aviso = L"Espaco coloca uma bomba";
    if (bomba.ativa && !bomba.explodindo)
        aviso = L"Bomba explode em: " + to_wstring((bomba.tempoMs + 999) / 1000) + L"s";
    else if (bomba.explodindo) aviso = L"BOOM! Afaste-se das chamas!";
    desenharTexto(g, aviso, 18, 76, 15, Gdiplus::Color(255, 255, 210, 85));
}

void desenharRodape(Gdiplus::Graphics& g) {
    int y = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA;
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, px(y), px(LARGURA_JOGO), px(ALTURA_RODAPE));
    desenharTexto(g, L"WASD/SETAS: mover   ESPACO: bomba   R: reiniciar   M: menu   Q/ESC: sair",
                  12, y + 8, 15, Gdiplus::Color(255, 225, 230, 235), true);
    if (estado == VITORIA)
        desenharTexto(g, L"VOCE VENCEU! Pressione R.", 12, y + 37, 18,
                      Gdiplus::Color(255, 90, 225, 120), true);
    else if (estado == DERROTA)
        desenharTexto(g, L"GAME OVER! Pressione R.", 12, y + 37, 18,
                      Gdiplus::Color(255, 245, 85, 85), true);
}

void desenharMenu(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush fundo(Gdiplus::Color(255, 20, 28, 38));
    g.FillRectangle(&fundo, 0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO));
    desenharTexto(g, L"BOMBERMAN", 20, 130, 42, Gdiplus::Color(255, 65, 210, 235), true);
    desenharTexto(g, L"ESCOLHA O MODO DE JOGO", 20, 230, 21,
                  Gdiplus::Color(255, 245, 245, 245), true);
    desenharTexto(g, L"[1] Jogar manualmente", 20, 305, 24,
                  Gdiplus::Color(255, 90, 225, 120), true);
    desenharTexto(g, L"[2] Jogo automatico 1,5x", 20, 365, 24,
                  Gdiplus::Color(255, 255, 210, 85), true);
    desenharTexto(g, L"[Q ou ESC] Sair", 20, 445, 18,
                  Gdiplus::Color(255, 205, 210, 215), true);
}

void desenhar(HWND janela) {
    PAINTSTRUCT ps;
    HDC destino = BeginPaint(janela, &ps);
    RECT cliente;
    GetClientRect(janela, &cliente);
    HDC memoria = CreateCompatibleDC(destino);
    HBITMAP bitmap = CreateCompatibleBitmap(destino, cliente.right, cliente.bottom);
    HGDIOBJ anterior = SelectObject(memoria, bitmap);
    Gdiplus::Graphics g(memoria);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (modo == MENU) desenharMenu(g);
    else {
        desenharHUD(g);
        desenharMapa(g);
        desenharRodape(g);
    }
    BitBlt(destino, 0, 0, cliente.right, cliente.bottom, memoria, 0, 0, SRCCOPY);
    SelectObject(memoria, anterior);
    DeleteObject(bitmap);
    DeleteDC(memoria);
    EndPaint(janela, &ps);
}

void lerTeclado(WPARAM tecla) {
    if (modo == MENU) {
        if (tecla == '1') iniciarPartida(MANUAL);
        else if (tecla == '2') iniciarPartida(AUTOMATICO);
        else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
        return;
    }
    if (tecla == 'M') modo = MENU;
    else if (tecla == 'W' || tecla == VK_UP) moverJogador(-1, 0);
    else if (tecla == 'S' || tecla == VK_DOWN) moverJogador(1, 0);
    else if (tecla == 'A' || tecla == VK_LEFT) moverJogador(0, -1);
    else if (tecla == 'D' || tecla == VK_RIGHT) moverJogador(0, 1);
    else if (tecla == ' ') colocarBomba();
    else if (tecla == 'R') reiniciar();
    else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
}

void atualizarEscalaDpi(HWND janela) {
    escalaDpi = GetDpiForWindow(janela) / 96.0f;
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
        double multiplicador = modo == AUTOMATICO ? 1.5 : 1.0;
        atualizar(static_cast<int>(min(decorrido, 100) * multiplicador));
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        lerTeclado(wParam);
        InvalidateRect(janela, nullptr, FALSE);
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

    const wchar_t CLASSE[] = L"BombermanUnivali";
    WNDCLASSW classe{};
    classe.lpfnWndProc = processarMensagem;
    classe.hInstance = instancia;
    classe.lpszClassName = CLASSE;
    classe.hCursor = LoadCursor(nullptr, IDC_ARROW);
    classe.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&classe);

    RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
    AdjustWindowRect(&area, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND janela = CreateWindowExW(0, CLASSE, L"Bomberman - Algoritmos e Programacao II",
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
