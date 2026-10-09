/*
Algoritmos e programação II, Trabalho M1
Desenvolvedores: Diego Silva |  Gabriel Bianchessi

Feito com matriz, structs, vetores e sub-rotinas. A interface usa Win32 e
GDI+ para abrir uma janela propria e desenhar os sprites PNG.

WASD/setas: mover | Espaco: bomba | R: reiniciar | Q ou ESC: sair
*/

#include <windows.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#endif

using namespace std;

const int LINHAS = 13;
const int COLUNAS = 15;
const int TEMPO_BOMBA_MS = 2200;
const int TEMPO_EXPLOSAO_MS = 650;
// Velocidade geral: estudos_sprite/batalha.cpp usa moveSpeed=4.2 celulas/seg
// (~238ms por celula) com posicao visual interpolada suavemente - nosso jogo
// estava em 105ms (quase 2x mais rapido) E teleportando direto de celula pra
// celula, o que o usuario sentiu como "rapido demais" e "nao fluido". Ajustado
// pra um ritmo parecido; a suavizacao visual fica em VELOCIDADE_VISUAL_CELS_SEG.
const int INTERVALO_MOVIMENTO_MS = 280;
const float VELOCIDADE_VISUAL_CELS_SEG = 3.6f;
const int ALCANCE_EXPLOSAO_INICIAL = 1;
const int ALCANCE_EXPLOSAO_MAXIMO = 5;
const int VIDAS_BASE = 3;
const int PASSO_LOGICA_MS = 16;
const int INTERVALO_INIMIGO_MIN_MS = 420;
const int INTERVALO_INIMIGO_MAX_MS = 780;
const int DISTANCIA_PERSEGUICAO = 5;
const int CHANCE_PERSEGUICAO_FACIL = 25;
const int CHANCE_PERSEGUICAO_MEDIO = 50;
const int CHANCE_PERSEGUICAO_DIFICIL = 75;
const int ALCANCE_CHUTE = 4;
const int TOTAL_FASES = 8;
const wchar_t* NOMES_FASES[] = {L"CASINO AZUL",L"MASMORRA",L"FABRICA NEON",L"RUINAS DE PEDRA",
                              L"CANION CEEDRUN",L"SELVA VIVA",L"LABORATORIO",L"TEMPLO ARCANO"};
const int INDICES_ATLAS_FASES[] = {1,4,0,2,3,5,6,7};
const int DURACAO_MORTE_MS = 1170;
const int VIDA_BOSS = 100;
const int INTERVALO_MOVIMENTO_BOSS_MS = 4000;
const int INTERVALO_ATAQUE_BOSS_MS = 4500;
const int IMUNIDADE_BOSS_MS = 700;
// Formula de pontuacao (secao 9 do PLANO_M2.md), tambem mostrada por extenso
// na tela "Pontuacao" do menu - os mesmos valores em um lugar so.
const int PONTOS_POR_INIMIGO = 100;
const int PONTOS_POR_CAIXA = 20;
const int PONTOS_POR_ITEM = 30;
const int PONTOS_POR_FASE = 500;
const int PONTOS_POR_BOSS = 1000;
const int CUSTO_POR_MOVIMENTO = 1;
const int CUSTO_POR_BOMBA = 5;
// Folhas de sprite direcional (jogador_direcional.png / inimigo_direcional.png,
// fatiadas de novos_perssonagens.png): grade de 3 colunas (frames de
// caminhada) por 4 linhas (DOWN, RIGHT, UP, LEFT), sem espacamento.
const int QUADRO_LARGURA = 16;
const int QUADRO_ALTURA = 24;
const int QUADROS_POR_DIRECAO = 3;
const int TAMANHO_CELULA = 52;
const int ALTURA_CABECALHO = 104;
const int ALTURA_RODAPE = 72;
const int LARGURA_MAPA = COLUNAS * TAMANHO_CELULA;
const int LARGURA_JOGO = LARGURA_MAPA + 300;
const int ALTURA_JOGO = ALTURA_CABECALHO + LINHAS * TAMANHO_CELULA + ALTURA_RODAPE;

enum TipoCelula { VAZIO, PAREDE_SOLIDA, PAREDE_FRAGIL, PAREDE_BONUS, PORTAL };
// DERROTA = perdeu uma vida, mas ainda tem vidas pra tentar de novo nesta
// fase (R reinicia a fase). FIM_DE_JOGO = vidas zeradas, partida encerrada
// de verdade (R comeca uma partida nova do zero).
enum EstadoJogo { JOGANDO, VITORIA, DERROTA, FIM_DE_JOGO };
enum ModoJogo { MENU, MANUAL, AUTOMATICO };
// CHUTE/TELEPORTE sao os itens extras que ja existiam; os outros 6 sao o
// conjunto minimo obrigatorio do enunciado (raio+1, bomba+1, vida extra,
// bomba-relogio, sobreviver a bomba, atravessar bloco quebravel).
enum TipoItem { CHUTE, TELEPORTE, RAIO, BOMBA_EXTRA, VIDA_EXTRA, BOMBA_RELOGIO, IMUNE_BOMBA, ATRAVESSA_BLOCO };
enum Dificuldade { FACIL, MEDIO, DIFICIL };

struct Posicao { int linha, coluna; };
// "especial" marca o inimigo mais inteligente da ultima fase: ele ignora a
// distancia/chance de perseguicao normais e persegue sempre (ver moverInimigos).
struct Inimigo {
    Posicao posicao;
    bool vivo;
    int tempoProximoMovimentoMs;
    bool especial = false;
    Posicao direcao{1, 0};  // ultima direcao de movimento, pra escolher a linha do sprite
    int quadroAnim = 1;     // acompanha a distancia visual; parado = quadro 1
    // Posicao DESENHADA, separada da logica (posicao acima): desliza
    // suavemente ate alcancar "posicao" em vez de teleportar a cada passo.
    // Sincronizada manualmente com "posicao" ao nascer/reiniciar (ver
    // montarFase) - so anda sozinha durante o jogo normal, via avancarVisual.
    float visualL = 0, visualC = 0;
    float distanciaAnimacao = 0;
};
// Agora existe um vetor de bombas (item "bomba+1" permite mais de uma ao
// mesmo tempo); cada bomba guarda sua propria area de explosao calculada,
// entao duas bombas podem estar explodindo em momentos diferentes sem
// interferir uma na outra. "relogio" = bomba-relogio (item): nao explode
// sozinha, so quando o jogador detona (tecla F).
struct Bomba {
    Posicao posicao{0, 0};
    bool ativa = false;
    bool explodindo = false;
    int tempoMs = 0;
    bool relogio = false;
    bool bossAtingido = false;
    vector<Posicao> celulasExplosao;
};
struct Item { Posicao posicao; TipoItem tipo; };
struct BlocoBonus { Posicao posicao; TipoItem tipo; };

// Boss final: corpo 3x2, avancos ortogonais e fogo direcionado como no Insano.
struct Boss {
    Posicao posicao{0, 0};
    bool vivo = false;
    int vida = 0;
    int tempoProximoMovimentoMs = 0;
    int tempoProximoAtaqueMs = 0;
    float visualL = 0, visualC = 0;
    Posicao destino{0,0};
    bool movendo = false, horizontal = true;
    int imunidadeMs = 0;
    bool investindo = false, investidaPendente = false;
    int avisoInvestidaMs = 0, animAtaqueMs = 0, sequenciaAtaque = 0;
};

// Contadores da partida em andamento; usados tanto no HUD quanto no calculo
// de pontuacao e no registro salvo em arquivo ao final da partida.
struct EstatisticasPartida {
    int movimentos = 0;
    int bombasUsadas = 0;
    int itensColetados = 0;
    int caixasDestruidas = 0;
    int inimigosAbatidos = 0;
};

// Uma linha do arquivo de ranking: data, jogador, pontuacao, bombas e
// movimentos, exatamente os campos pedidos no enunciado.
struct RegistroPontuacao {
    string data;
    string jogador;
    int pontuacao = 0;
    int bombas = 0;
    int movimentos = 0;
};

TipoCelula mapa[LINHAS][COLUNAS];
Posicao jogador{1, 1};
Posicao direcaoJogador{0, 1};
vector<Inimigo> inimigos;
vector<BlocoBonus> blocosBonus;
vector<Item> itensNoChao;
vector<Bomba> bombas;
EstadoJogo estado = JOGANDO;
ModoJogo modo = MENU;
int pontos = 0;
int alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
int tempoMovimentoMs = 0;
bool executando = true;
bool possuiCartaChute = false;
bool possuiCartaTeleporte = false;
bool teclaPressionada[256]{};
unsigned long long ordemTecla[256]{}, proximaOrdemTecla=0;
EstatisticasPartida estatisticas;
bool registroSalvo = false;
wstring nomeJogador = L"";
// As telas do menu (fora do jogo em si): principal, escolha de dificuldade
// (antes de comecar a jogar), ranking e as 3 telas so-informativas pedidas
// pelo enunciado (como jogar, dificuldades, pontuacao).
enum TelaMenu { MENU_PRINCIPAL, MENU_DIFICULDADE, MENU_RANKING, MENU_COMO_JOGAR, MENU_DIFICULDADES_INFO, MENU_PONTUACAO, MENU_PERSONAGENS, MENU_FASES };
int faseSelecionada = 1;
int personagemP1 = 0, personagemP2 = 1;
int jogadorEmSelecao = 1;
const wchar_t* nomesPersonagens[] = {L"BRANCO", L"PRETO", L"AZUL", L"VERMELHO"};
unique_ptr<Gdiplus::Image> personagens[4], posesVitoria[4], posesDerrota[4];
unique_ptr<Gdiplus::Image> spriteItens;
TelaMenu telaMenu = MENU_PRINCIPAL;
Dificuldade dificuldade = FACIL;
int faseAtual = 1;
ModoJogo modoPendente = MANUAL;
// Segundo jogador (cooperativo local, so no modo manual): setas + Enter.
// Vidas, pontos, itens e o limite de bombas sao compartilhados com o
// jogador 1 - time unico, nao e um "versus". So a posicao/direcao/cooldown
// de movimento sao proprios de cada jogador.
bool doisJogadores = false;
Posicao jogador2{1, 2};
Posicao direcaoJogador2{0, 1};
int tempoMovimentoMs2 = 0;
int quadroAnimJogador = 0;
int quadroAnimJogador2 = 0;
// Posicao visual dos jogadores (ver comentario em Inimigo::visualL/visualC).
float jogadorVisualL = 1, jogadorVisualC = 1;
float jogador2VisualL = 1, jogador2VisualC = 2;
float distanciaAnimacaoJogador = 0, distanciaAnimacaoJogador2 = 0;
int tempoFimMs = 0;
int jogadoresAtingidos = 0;
int tempoPartidaMs = 0;
unique_ptr<Gdiplus::Image> spriteVitoria, spriteDerrota;
bool portalAtivo = false;
Posicao portalPosicao{0, 0};
enum PoderBoss { MACHADO, ENERGIA, VENENO, CHUVA_FOGO };
const wchar_t* NOMES_BOSSES[]={L"GOLEM DA MASMORRA",L"EGG-HVC / DEMOLIDOR",L"HIDRA DA SELVA",L"GUARDIAO ARCANO"};
const wchar_t* PODERES_BOSSES[]={L"Machados e facoes giratorios",L"Energia e investida: saia da frente!",L"Rajada de veneno: evite as pocas",L"Fogo: desvie dos projeteis"};
int indiceBoss(){return max(0,min(3,faseAtual/2-1));}
int vidaMaximaBoss(){const int vidas[]={48,64,80,VIDA_BOSS};return vidas[indiceBoss()];}
struct AreaBoss { Posicao posicao; int avisoMs, ativoMs; PoderBoss poder; };
vector<AreaBoss> areasBoss;
struct ProjetilBoss { float l,c,vl,vc,idade,vida; PoderBoss poder; int avisoMs; int arma = 0; };
struct ImpactoBoss { float l,c,idade; PoderBoss poder; };
vector<ProjetilBoss> projeteisBoss;
vector<ImpactoBoss> impactosBoss;
// Mesmo Actor e parametros do Mario do estudo; aqui ele e aliado da campanha.
struct MarioAliado {
    Posicao posicao{0,0}; float l=0,c=0;
    int direcao=0,hp=0,movendo=-1;
    float pensar=0,imunidade=0,distancia=0,recarga=0,pose=0;
    bool ativo=false,andando=false,escondido=false;
    Posicao bloco{0,0};
} marioAliado;
struct MacaMario { float sx,sy,tx,ty,idade=0,duracao=1; };
vector<MacaMario> macasMario;
vector<Posicao> macasNoChao;
const int MARIO_DC[]={0,-1,0,1},MARIO_DL[]={1,0,-1,0};
struct FogoBoss { float l, c, vl, vc, vida; };
vector<FogoBoss> fogosBoss;
Boss boss;
vector<Posicao> perigoBoss;
int tempoPerigoBossMs = 0;
bool bossIniciado = false;
bool bossConcluido = false;
// Itens obrigatorios (secao 8 do PLANO_M2.md): cumulativos durante a fase,
// resetados em montarFase() quando a fase muda (regra do enunciado).
int bombasExtras = 0;
bool temBombaRelogio = false;
bool imuneAPropriaBomba = false;
bool atravessaBlocoQuebravel = false;
int vidas = VIDAS_BASE;
// Garante que uma unica morte so desconta 1 vida, mesmo que atualizar()
// rode varias vezes com estado == DERROTA antes do jogador apertar R.
bool vidaPerdida = false;
mt19937 gerador(random_device{}());
HWND janelaPrincipal = nullptr;
HANDLE processoInsano = nullptr;
ULONG_PTR tokenGdiPlus = 0;
unique_ptr<Gdiplus::Image> spriteBoss;
unique_ptr<Gdiplus::Image> spriteFogoBoss;
unique_ptr<Gdiplus::Bitmap> spritesBossCampanha[3];
unique_ptr<Gdiplus::Bitmap> spriteMachadoBoss, facoesBoss[5], posesEggBoss[2], energiaBoss[4], venenoBoss[4], impactoBoss[4];
unique_ptr<Gdiplus::Bitmap> quadrosMario[4][3],spriteMacaMario;
Gdiplus::Image* imagemBossAtual(){int i=indiceBoss();
    if(i==1 && boss.investindo && posesEggBoss[1])return posesEggBoss[1].get();
    if(i==1 && boss.movendo && (int(boss.visualC*3+boss.visualL*3)%2) && posesEggBoss[0])return posesEggBoss[0].get();
    return i<3?spritesBossCampanha[i].get():spriteBoss.get();}
// Bomba/explosao animadas (4 quadros lineares de 16x16, fatiados de
// BombsExplosionsv2.png - ver fatiar_explosao.py). O relogio global
// (relogioAnimacaoMs) so controla o "pisca-pisca" da bomba parada; a
// explosao usa o proprio tempoMs da bomba, entao cada uma mostra o quadro
// certo pro seu progresso, nao importa quando comecou.
unique_ptr<Gdiplus::Image> spriteBombaAnimada;
unique_ptr<Gdiplus::Image> spriteExplosaoAnimada;
int relogioAnimacaoMs = 0;
// Folhas direcionais (ver QUADRO_LARGURA/ALTURA acima): 3 quadros x 4 direcoes,
// recortadas na hora de desenhar via DrawImage com retangulo de origem (nao
// precisam do cache de pre-escala, ja que o GDI+ recorta+escala num unico
// blit, do mesmo jeito que ja desenha o sprite inteiro hoje).
unique_ptr<Gdiplus::Image> spriteInimigoDirecional;
// Tilesets de chao/parede/caixa por fase - coordenadas de recorte validadas
// em D:\algoritmos_prog_2\estudos_sprite\batalha.cpp (funcao sourceTiles),
// reaproveitadas aqui so pelas coordenadas, sem copiar codigo daquele
// projeto (que e so um estudo visual, nao faz parte do jogo).
unique_ptr<Gdiplus::Image> spriteTemaFase1; // SNES Stage 2-5 (Casino Azul)
unique_ptr<Gdiplus::Image> spriteTemaFase3; // SNES Stage 6-3 (Fabrica Neon)
unique_ptr<Gdiplus::Image> spriteArenasEstudo;
// Fase 2 usa o tileset Dungeon do GameCube, que e gigante (2054x2903px, ~23MB
// descomprimido) - o estudo em estudos_sprite/EXPLICACAO_SPRITES_CPP.md (secao
// 8) mediu 3-4s por redesenho de terreno ao recortar repetidas vezes direto
// dessa folha. Em vez disso, recortamos as 3 casinhas pequenas UMA vez no
// carregamento (prepararTemaFase2Compacto) e descartamos a folha gigante.
unique_ptr<Gdiplus::Bitmap> spriteTemaFase2Chao;
unique_ptr<Gdiplus::Bitmap> spriteTemaFase2Parede;
unique_ptr<Gdiplus::Bitmap> spriteTemaFase2Caixa;
float escalaDpi = 1.0f;
int margemJanelaX = 0, margemJanelaY = 0;

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
unique_ptr<Gdiplus::Bitmap> cacheSpriteBoss;

void invalidarCacheMapa() { cacheMapaSujo = true; }

// --- Persistencia em arquivo (ranking.csv ao lado do executavel) ---------

string pastaDoExecutavelUtf8() {
    char caminho[MAX_PATH];
    DWORD tamanho = GetModuleFileNameA(nullptr, caminho, MAX_PATH);
    string pasta(caminho, tamanho);
    size_t separador = pasta.find_last_of("\\/");
    return separador == string::npos ? "." : pasta.substr(0, separador);
}

string caminhoRankingUtf8() { return pastaDoExecutavelUtf8() + "\\ranking.csv"; }

string dataAtual() {
    time_t agora = time(nullptr);
    tm local{};
    localtime_s(&local, &agora);
    ostringstream saida;
    saida << setfill('0') << setw(2) << local.tm_mday << '/' << setw(2) << (local.tm_mon + 1)
          << '/' << (local.tm_year + 1900);
    return saida.str();
}

string wparaStr(const wstring& texto) {
    string resultado;
    resultado.reserve(texto.size());
    for (wchar_t c : texto) resultado += static_cast<char>(c);
    return resultado;
}

// Sobrecarga de verdade (duas assinaturas distintas, nao parametro default):
// a versao com caminho explicito permite gravar em um arquivo alternativo
// (por exemplo em testes), e a versao sem caminho usa o ranking padrao do jogo.
void salvarRegistroPartida(const RegistroPontuacao& registro, const string& caminhoArquivo) {
    ofstream arquivo(caminhoArquivo, ios::app);
    if (!arquivo) return;
    arquivo << registro.data << ';' << registro.jogador << ';' << registro.pontuacao << ';'
            << registro.bombas << ';' << registro.movimentos << '\n';
}

void salvarRegistroPartida(const RegistroPontuacao& registro) {
    salvarRegistroPartida(registro, caminhoRankingUtf8());
}

// Parametro default (distinto da sobrecarga acima): caminho vazio = usar o
// ranking padrao do jogo.
vector<RegistroPontuacao> lerRanking(const string& caminhoArquivo = "") {
    vector<RegistroPontuacao> registros;
    string caminho = caminhoArquivo.empty() ? caminhoRankingUtf8() : caminhoArquivo;
    ifstream arquivo(caminho);
    if (!arquivo) return registros;
    string linha;
    while (getline(arquivo, linha)) {
        if (linha.empty()) continue;
        istringstream ss(linha);
        string campo;
        RegistroPontuacao registro;
        if (!getline(ss, registro.data, ';')) continue;
        if (!getline(ss, registro.jogador, ';')) continue;
        if (!getline(ss, campo, ';')) continue;
        registro.pontuacao = atoi(campo.c_str());
        if (!getline(ss, campo, ';')) continue;
        registro.bombas = atoi(campo.c_str());
        if (!getline(ss, campo, ';')) continue;
        registro.movimentos = atoi(campo.c_str());
        registros.push_back(registro);
    }
    sort(registros.begin(), registros.end(),
         [](const RegistroPontuacao& a, const RegistroPontuacao& b) { return a.pontuacao > b.pontuacao; });
    return registros;
}

void digitarNomeJogador(wchar_t caractere) {
    if (caractere == L'\b') {
        if (!nomeJogador.empty()) nomeJogador.pop_back();
        return;
    }
    bool valido = (caractere >= L'A' && caractere <= L'Z') || (caractere >= L'a' && caractere <= L'z') ||
                  (caractere >= L'0' && caractere <= L'9') || caractere == L' ';
    if (valido && nomeJogador.size() < 16) nomeJogador += caractere;
}

bool iguais(Posicao a, Posicao b) {
    return a.linha == b.linha && a.coluna == b.coluna;
}

// Desliza (visualL,visualC) suavemente em direcao a "alvo", a
// VELOCIDADE_VISUAL_CELS_SEG celulas por segundo, sem nunca ultrapassar.
// A posicao inteira reserva a casa de destino para navegacao na matriz.
// O contato e o dano usam a posicao atual interpolada, igual ao desenho:
// escolher um destino nao significa ja ter chegado nele.
bool visualNoDestino(float visualL, float visualC, Posicao alvo) {
    return abs(visualL - alvo.linha) < 0.0001f && abs(visualC - alvo.coluna) < 0.0001f;
}

float avancarVisual(float& visualL, float& visualC, Posicao alvo, int tempoMs) {
    float dl = static_cast<float>(alvo.linha) - visualL;
    float dc = static_cast<float>(alvo.coluna) - visualC;
    float distancia = sqrtf(dl * dl + dc * dc);
    if (distancia < 0.0001f) { visualL = static_cast<float>(alvo.linha); visualC = static_cast<float>(alvo.coluna); return 0; }
    float passo = min(distancia, VELOCIDADE_VISUAL_CELS_SEG * tempoMs / 1000.0f);
    visualL += dl / distancia * passo;
    visualC += dc / distancia * passo;
    return passo;
}

// A pose acompanha a distancia visual, inclusive entre duas casas.
int quadroCaminhada(float distancia, bool andando) {
    const int ciclo[] = {0, 1, 0, 2};
    return andando ? ciclo[static_cast<int>(distancia * 2.5f) % 4] : 1;
}

bool dentro(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

bool livre(int linha, int coluna) {
    return dentro(linha, coluna) && (mapa[linha][coluna] == VAZIO || mapa[linha][coluna] == PORTAL);
}

// Unico ponto que altera "pontos": aplica o delta e nunca deixa a pontuacao
// ficar negativa (regra explicada na tela "Pontuacao" do menu).
void ajustarPontos(int delta) { pontos = max(0, pontos + delta); }

bool temInimigo(Posicao p, int ignorar = -1) {
    for (int i = 0; i < static_cast<int>(inimigos.size()); i++) {
        if (i != ignorar && inimigos[i].vivo && iguais(inimigos[i].posicao, p)) return true;
    }
    return false;
}

bool naExplosao(Posicao p) {
    for (const Bomba& b : bombas) {
        if (!b.explodindo) continue;
        for (const Posicao& parte : b.celulasExplosao) if (iguais(parte, p)) return true;
    }
    return false;
}

bool segundoJogadorAtivo() { return doisJogadores && modo == MANUAL; }

// Caixa corporal central de meia celula, independente da escala da janela.
// Espaco vazio do recorte, sombra e capacete nao aumentam o alcance do contato.
const float MEIA_CAIXA_PERSONAGEM = .25f;

bool personagensEmContato(float l1, float c1, float l2, float c2) {
    return abs(l1-l2) < 2*MEIA_CAIXA_PERSONAGEM && abs(c1-c2) < 2*MEIA_CAIXA_PERSONAGEM;
}

bool corpoNaCelula(float l, float c, Posicao celula) {
    return abs(l-celula.linha) < .5f+MEIA_CAIXA_PERSONAGEM &&
           abs(c-celula.coluna) < .5f+MEIA_CAIXA_PERSONAGEM;
}

bool corpoNaExplosao(float l, float c) {
    for (const Bomba& b : bombas)
        if (b.explodindo)
            for (Posicao p : b.celulasExplosao) if (corpoNaCelula(l,c,p)) return true;
    return false;
}

// Corpo de 3x2 casas, medido na posicao atual como no estudo.
bool celulaBoss(int l, int c) {
    return boss.vivo && c+1>boss.visualC+.001f && c<boss.visualC+3-.001f &&
           l+1>boss.visualL+.001f && l<boss.visualL+2-.001f;
}
bool bloqueadoBoss(Posicao p) {
    if (celulaBoss(p.linha,p.coluna)) return true;
    if (boss.investindo) return false; // Investida reserva so o corpo atual: permite esquiva.
    return boss.vivo && boss.movendo &&
        p.coluna>=min(boss.posicao.coluna,boss.destino.coluna) &&
        p.coluna<max(boss.posicao.coluna,boss.destino.coluna)+3 &&
        p.linha>=min(boss.posicao.linha,boss.destino.linha) &&
        p.linha<max(boss.posicao.linha,boss.destino.linha)+2;
}
bool contatoComInimigo(float l, float c) {
    for (const Inimigo& inimigo : inimigos)
        if (inimigo.vivo && personagensEmContato(l,c,inimigo.visualL,inimigo.visualC)) return true;
    return boss.vivo && c+.75f>boss.visualC && c+.25f<boss.visualC+3 &&
           l+.75f>boss.visualL && l+.25f<boss.visualL+2;
}

void verificarContatoJogadores() {
    if (contatoComInimigo(jogadorVisualL,jogadorVisualC)) { estado=DERROTA; jogadoresAtingidos|=1; }
    if (segundoJogadorAtivo() && contatoComInimigo(jogador2VisualL,jogador2VisualC)) { estado=DERROTA; jogadoresAtingidos|=2; }
}

// Celula ocupada por alguma bomba ativa (qualquer uma, agora que podem
// existir varias ao mesmo tempo) - usado como obstaculo para inimigos.
bool bombaEm(Posicao p) {
    for (const Bomba& b : bombas) if (b.ativa && iguais(b.posicao, p)) return true;
    return false;
}

int limiteBombas() { return 1 + bombasExtras; }

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
    // Os 6 itens obrigatorios do enunciado + os 2 extras que ja existiam
    // (chute/teleporte); embaralhados pra nao sair sempre na mesma ordem.
    vector<TipoItem> tipos = {RAIO, BOMBA_EXTRA, VIDA_EXTRA, BOMBA_RELOGIO,
                               IMUNE_BOMBA, ATRAVESSA_BLOCO, CHUTE, TELEPORTE};
    shuffle(tipos.begin(), tipos.end(), gerador);
    int total = min(static_cast<int>(tipos.size()), static_cast<int>(candidatos.size()));
    for (int i = 0; i < total; i++) {
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

int quantidadeInimigosNormais() {
    switch (dificuldade) {
        case FACIL: return 3;
        case MEDIO: return 5;
        case DIFICIL: return 7;
    }
    return 3;
}

// Celula livre qualquer (fora do jogador/inimigos), usada para o inimigo
// especial da ultima fase e para o portal - ao contrario de
// sortearPosicoesIniciais, nao depende das 8 posicoes fixas de canto/borda.
Posicao posicaoLivreAleatoria() {
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (mapa[l][c] == VAZIO && !iguais({l, c}, jogador) && !temInimigo({l, c}) &&
                !(doisJogadores && iguais({l, c}, jogador2)))
                livres.push_back({l, c});
    if (livres.empty()) return jogador;
    uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
    return livres[sorteio(gerador)];
}

void colocarPortal() {
    if (portalAtivo) return;
    portalPosicao = posicaoLivreAleatoria();
    mapa[portalPosicao.linha][portalPosicao.coluna] = PORTAL;
    portalAtivo = true;
    invalidarCacheMapa();
}

void iniciarBoss() {
    bossIniciado = true;
    boss = Boss{};
    boss.vivo = true; boss.vida = vidaMaximaBoss();
    boss.posicao = {1,6};
    // Evita aparecer sobre um jogador que esteja no alto da arena.
    bool localEncontrado=false;
    for (int l=1;l<LINHAS-2 && !localEncontrado;++l) for (int c=1;c<COLUNAS-3;++c) {
        auto sobrepoe=[&](float jl,float jc){return jc+.85f>c && jc+.15f<c+3 && jl+.85f>l && jl+.15f<l+2;};
        bool segredo=marioAliado.escondido && marioAliado.bloco.coluna>=c && marioAliado.bloco.coluna<c+3 && marioAliado.bloco.linha>=l && marioAliado.bloco.linha<l+2;
        if (!segredo && (!marioAliado.ativo || marioAliado.hp<=0 || !sobrepoe(marioAliado.l,marioAliado.c)) && !sobrepoe(jogadorVisualL,jogadorVisualC) &&
            (!segundoJogadorAtivo() || !sobrepoe(jogador2VisualL,jogador2VisualC))) {boss.posicao={l,c};localEncontrado=true;break;}
    }
    boss.destino = boss.posicao;
    boss.visualL = static_cast<float>(boss.posicao.linha);
    boss.visualC = static_cast<float>(boss.posicao.coluna);
    for(int l=boss.posicao.linha;l<boss.posicao.linha+2;++l)
        for(int c=boss.posicao.coluna;c<boss.posicao.coluna+3;++c) mapa[l][c]=VAZIO;
    boss.tempoProximoMovimentoMs = INTERVALO_MOVIMENTO_BOSS_MS;
    boss.tempoProximoAtaqueMs = 5000;
    perigoBoss.clear(); fogosBoss.clear(); areasBoss.clear(); projeteisBoss.clear(); impactosBoss.clear();
    invalidarCacheMapa();
}

// Prepara o tabuleiro da fase atual (mapa, jogador, inimigos da dificuldade
// e, na ultima fase, o inimigo especial). NAO mexe em pontos/estatisticas:
// essas sao da partida inteira, nao da fase (ver novaPartida/reiniciarFase).
void montarFase() {
    tempoFimMs = 0;
    jogadoresAtingidos = 0;
    fill(begin(teclaPressionada),end(teclaPressionada),false);
    distanciaAnimacaoJogador = distanciaAnimacaoJogador2 = 0;
    quadroAnimJogador = quadroAnimJogador2 = 1;
    criarMapa();
    vector<Posicao> spawns;
    sortearPosicoesIniciais(spawns);
    posicionarBlocosBonus();
    invalidarCacheMapa();
    jogador = spawns[0];
    direcaoJogador = {0, 1};
    jogadorVisualL = static_cast<float>(jogador.linha);
    jogadorVisualC = static_cast<float>(jogador.coluna);
    inimigos.clear();
    uniform_int_distribution<int> sorteioIntervalo(INTERVALO_INIMIGO_MIN_MS, INTERVALO_INIMIGO_MAX_MS);
    int totalNormais = quantidadeInimigosNormais();
    for (int i = 1; i <= totalNormais && i < static_cast<int>(spawns.size()); i++)
        inimigos.push_back({spawns[i], true, sorteioIntervalo(gerador)});
    if (faseAtual == TOTAL_FASES) {
        Posicao posEspecial = posicaoLivreAleatoria();
        limparArea(posEspecial.linha, posEspecial.coluna);
        Inimigo especial{posEspecial, true, sorteioIntervalo(gerador)};
        especial.especial = true;
        inimigos.push_back(especial);
    }
    for (Inimigo& inimigo : inimigos) {
        inimigo.visualL = static_cast<float>(inimigo.posicao.linha);
        inimigo.visualC = static_cast<float>(inimigo.posicao.coluna);
        if (dificuldade == FACIL) inimigo.tempoProximoMovimentoMs += 900;
    }
    itensNoChao.clear();
    possuiCartaChute = false;
    possuiCartaTeleporte = false;
    alcanceExplosao = ALCANCE_EXPLOSAO_INICIAL;
    bombas.clear();
    // Bonus resetam ao trocar de fase (regra explicita do enunciado).
    bombasExtras = 0;
    temBombaRelogio = false;
    imuneAPropriaBomba = false;
    atravessaBlocoQuebravel = false;
    // NAO mexe em "vidas" aqui: isso e responsabilidade de quem chama
    // montarFase (avancarFase/novaPartida zeram pro valor base; reiniciarFase
    // preserva o que sobrou, ja que repetir a fase e o que custa uma vida).
    portalAtivo = false;
    bossIniciado = false;
    bossConcluido = false;
    boss = Boss{};
    perigoBoss.clear(); fogosBoss.clear(); areasBoss.clear(); projeteisBoss.clear(); impactosBoss.clear();
    tempoMovimentoMs = 0;
    vidaPerdida = false;
    estado = JOGANDO;
    // Jogador 2 (cooperativo) so existe no modo manual; em posicao livre
    // qualquer, igual ao inimigo especial/boss, pra nao depender de sobrar
    // um dos 8 slots fixos de canto/borda com dificuldades mais cheias.
    if (doisJogadores && modo == MANUAL) {
        jogador2 = posicaoLivreAleatoria();
        limparArea(jogador2.linha, jogador2.coluna);
        direcaoJogador2 = {0, 1};
        tempoMovimentoMs2 = 0;
        jogador2VisualL = static_cast<float>(jogador2.linha);
        jogador2VisualC = static_cast<float>(jogador2.coluna);
    }
    marioAliado=MarioAliado{};macasMario.clear();macasNoChao.clear();
    if(faseAtual%2==0){
        vector<Posicao> candidatos;
        for(int l=1;l<LINHAS-1;++l)for(int c=1;c<COLUNAS-1;++c)
            if(mapa[l][c]==PAREDE_FRAGIL)candidatos.push_back({l,c});
        if(!candidatos.empty()){
            uniform_int_distribution<int> sorteio(0,int(candidatos.size())-1);
            marioAliado.bloco=candidatos[sorteio(gerador)];marioAliado.escondido=true;
        }
    }

}

// "R" depois de morrer com vidas restantes: repete a fase atual (bonus da
// fase se perdem, vidas restantes continuam valendo), preservando pontuacao
// e estatisticas acumuladas da partida.
void reiniciarFase() {
    montarFase();
}

void avancarFase() {
    if (faseAtual < TOTAL_FASES) faseAtual++;
    vidas = VIDAS_BASE;
    montarFase();
}

// Comeca uma partida do zero (a partir do menu, ou "R" apos Game Over): zera
// fase, pontos, estatisticas e vidas - so essa funcao faz isso.
void novaPartida(ModoJogo novoModo, Dificuldade novaDificuldade) {
    modo = novoModo;
    dificuldade = novaDificuldade;
    telaMenu = MENU_PRINCIPAL;
    faseAtual = faseSelecionada;
    pontos = 0;
    vidas = VIDAS_BASE;
    estatisticas = EstatisticasPartida{};
    registroSalvo = false;
    tempoPartidaMs = 0;
    montarFase();
}

void coletarItensEm(Posicao pos) {
    for (size_t i = 0; i < itensNoChao.size(); i++) {
        if (!iguais(itensNoChao[i].posicao, pos)) continue;
        switch (itensNoChao[i].tipo) {
            case CHUTE: possuiCartaChute = true; break;
            case TELEPORTE: possuiCartaTeleporte = true; break;
            case RAIO: if (alcanceExplosao < ALCANCE_EXPLOSAO_MAXIMO) alcanceExplosao++; break;
            case BOMBA_EXTRA: bombasExtras++; break;
            case VIDA_EXTRA: vidas++; break;
            case BOMBA_RELOGIO: temBombaRelogio = true; break;
            case IMUNE_BOMBA: imuneAPropriaBomba = true; break;
            case ATRAVESSA_BLOCO: atravessaBlocoQuebravel = true; break;
        }
        itensNoChao.erase(itensNoChao.begin() + i);
        estatisticas.itensColetados++;
        ajustarPontos(PONTOS_POR_ITEM);
        return;
    }
}

void coletarItens() { coletarItensEm(jogador); }

// Compartilhados entre moverJogador (P1) e moverJogador2 (P2, cooperativo):
// os dois times "um so" - mesma vida, pontos e progresso de fase.
bool podeAndarPara(Posicao destino) {
    if (bloqueadoBoss(destino) || bombaEm(destino)) return false;
    return livre(destino.linha, destino.coluna) ||
           (atravessaBlocoQuebravel && dentro(destino.linha, destino.coluna) &&
            mapa[destino.linha][destino.coluna] == PAREDE_FRAGIL);
}

void atingirSePerigoso(float l, float c) {
    if (contatoComInimigo(l,c) || (corpoNaExplosao(l,c) && !imuneAPropriaBomba)) estado = DERROTA;
}

void avancarOuVencerSePortal(Posicao pos) {
    if (estado == JOGANDO && mapa[pos.linha][pos.coluna] == PORTAL) {
        ajustarPontos(PONTOS_POR_FASE);
        if (faseAtual < TOTAL_FASES) avancarFase();
        else estado = VITORIA;
    }
}

void moverJogador(int dl, int dc) {
    if (estado != JOGANDO || tempoMovimentoMs > 0 ||
        !visualNoDestino(jogadorVisualL, jogadorVisualC, jogador)) return;
    direcaoJogador = {dl, dc};
    Posicao origem = jogador;
    Posicao destino{jogador.linha + dl, jogador.coluna + dc};
    if (podeAndarPara(destino)) jogador = destino;
    if (!iguais(origem, jogador)) {
        estatisticas.movimentos++;
        ajustarPontos(-CUSTO_POR_MOVIMENTO);
        quadroAnimJogador = (quadroAnimJogador + 1) % QUADROS_POR_DIRECAO;
    }
    atingirSePerigoso(jogadorVisualL,jogadorVisualC);
    tempoMovimentoMs = iguais(origem,jogador) ? 0 : INTERVALO_MOVIMENTO_MS;
}

// Jogador 2 (cooperativo): mesmo comportamento do jogador 1, so que grava em
// jogador2/direcaoJogador2/tempoMovimentoMs2. Vida, pontos, itens e fase sao
// os mesmos do time (atingirSePerigoso/coletarItensEm/avancarOuVencerSePortal
// ja sao genericos o bastante pra servir aos dois).
void moverJogador2(int dl, int dc) {
    if (estado != JOGANDO || tempoMovimentoMs2 > 0 ||
        !visualNoDestino(jogador2VisualL, jogador2VisualC, jogador2)) return;
    direcaoJogador2 = {dl, dc};
    Posicao origem = jogador2;
    Posicao destino{jogador2.linha + dl, jogador2.coluna + dc};
    if (podeAndarPara(destino)) jogador2 = destino;
    if (!iguais(origem, jogador2)) {
        estatisticas.movimentos++;
        ajustarPontos(-CUSTO_POR_MOVIMENTO);
        quadroAnimJogador2 = (quadroAnimJogador2 + 1) % QUADROS_POR_DIRECAO;
    }
    atingirSePerigoso(jogador2VisualL,jogador2VisualC);
    tempoMovimentoMs2 = iguais(origem,jogador2) ? 0 : INTERVALO_MOVIMENTO_MS;
}

void iniciarExplosaoBomba(Bomba& b);

void colocarBombaEm(Posicao pos) {
    if (estado != JOGANDO || static_cast<int>(bombas.size()) >= limiteBombas()) return;
    Bomba nova;
    nova.posicao = pos;
    nova.ativa = true;
    nova.tempoMs = TEMPO_BOMBA_MS;
    nova.relogio = temBombaRelogio;
    bombas.push_back(nova);
    estatisticas.bombasUsadas++;
    ajustarPontos(-CUSTO_POR_BOMBA);
}

void colocarBomba() { colocarBombaEm(jogador); }
void colocarBomba2() { colocarBombaEm(jogador2); }

// Bomba-relogio (item): nao explode sozinha, so quando o jogador aperta a
// tecla de detonar - todas as bombas-relogio ativas do jogador explodem
// juntas, como no Bomberman classico.
void detonarBombasRelogio() {
    for (Bomba& b : bombas) if (b.ativa && !b.explodindo && b.relogio) iniciarExplosaoBomba(b);
}

void chutarBomba() {
    if (estado != JOGANDO || !possuiCartaChute) return;
    Posicao esperado{jogador.linha + direcaoJogador.linha, jogador.coluna + direcaoJogador.coluna};
    for (Bomba& b : bombas) {
        if (!b.ativa || b.explodindo || !iguais(b.posicao, esperado)) continue;
        Posicao destinoFinal = b.posicao;
        for (int passo = 1; passo <= ALCANCE_CHUTE; passo++) {
            Posicao tentativa{b.posicao.linha + direcaoJogador.linha * passo,
                              b.posicao.coluna + direcaoJogador.coluna * passo};
            if (!livre(tentativa.linha, tentativa.coluna) || temInimigo(tentativa)) break;
            destinoFinal = tentativa;
        }
        b.posicao = destinoFinal;
        possuiCartaChute = false;
        return;
    }
}

void teletransportarJogador() {
    if (estado != JOGANDO || !possuiCartaTeleporte) return;
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (livre(l, c) && !temInimigo({l, c}) && !bombaEm({l, c}) &&
                !(doisJogadores && iguais({l, c}, jogador2)))
                livres.push_back({l, c});
    if (livres.empty()) return;
    uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
    jogador = livres[sorteio(gerador)];
    // Teleporte "pula" de verdade - a posicao visual tem que acompanhar na
    // hora, nao deslizar visivelmente atravessando o mapa inteiro.
    jogadorVisualL = static_cast<float>(jogador.linha);
    jogadorVisualC = static_cast<float>(jogador.coluna);
    atingirSePerigoso(jogadorVisualL,jogadorVisualC);
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

// Recursiva de proposito (criterio tecnico do trabalho): a cada chamada
// avanca uma celula na direcao (dl,dc) e decide se para (parede solida ou
// alcance esgotado) ou continua. Isso implementa a regra do enunciado "a
// explosao passara por tantos blocos quebraveis quanto o nivel do fogo
// permitir" - ao contrario da versao antiga (que sempre parava no primeiro
// bloco fragil), aqui o mesmo alcanceExplosao tambem conta quantos blocos
// frageis em sequencia a chama consegue atravessar/destruir.
void libertarMario(Posicao p) {
    if(!marioAliado.escondido || !iguais(p,marioAliado.bloco))return;
    marioAliado.escondido=false;marioAliado.ativo=true;marioAliado.hp=3;
    marioAliado.posicao=p;marioAliado.l=float(p.linha);marioAliado.c=float(p.coluna);
    marioAliado.imunidade=2.f; // A bomba que liberta nao machuca o aliado.
}
void propagarExplosao(vector<Posicao>& celulas, int dl, int dc, Posicao posAtual, int alcanceRestante) {
    if (alcanceRestante <= 0) return;
    Posicao proxima{posAtual.linha + dl, posAtual.coluna + dc};
    if (!dentro(proxima.linha, proxima.coluna) || mapa[proxima.linha][proxima.coluna] == PAREDE_SOLIDA) return;
    celulas.push_back(proxima);
    if (mapa[proxima.linha][proxima.coluna] == PAREDE_FRAGIL) {
        mapa[proxima.linha][proxima.coluna] = VAZIO;
        bool libertando=marioAliado.escondido && iguais(proxima,marioAliado.bloco);
        libertarMario(proxima);
        if(marioAliado.ativo && !libertando && gerador()%3==0)macasNoChao.push_back(proxima);
        invalidarCacheMapa();
        estatisticas.caixasDestruidas++;
        ajustarPontos(PONTOS_POR_CAIXA);
    } else if (mapa[proxima.linha][proxima.coluna] == PAREDE_BONUS) {
        mapa[proxima.linha][proxima.coluna] = VAZIO;
        destruirBlocoBonus(proxima.linha, proxima.coluna);
        invalidarCacheMapa();
        estatisticas.caixasDestruidas++;
        ajustarPontos(PONTOS_POR_CAIXA);
    }
    propagarExplosao(celulas, dl, dc, proxima, alcanceRestante - 1);
}

void matarInimigo(Inimigo& inimigo, int& pontuacao) {
    inimigo.vivo = false;
    pontuacao += PONTOS_POR_INIMIGO;
    estatisticas.inimigosAbatidos++;
}

void atingirPersonagens(vector<Inimigo>& listaInimigos,
                        EstadoJogo& estadoAtual, int& pontuacao) {
    // Os dados recebidos por referencia sao alterados diretamente.
    if (corpoNaExplosao(jogadorVisualL,jogadorVisualC) && !imuneAPropriaBomba) { estadoAtual = DERROTA; jogadoresAtingidos|=1; }
    if (segundoJogadorAtivo() && corpoNaExplosao(jogador2VisualL,jogador2VisualC) && !imuneAPropriaBomba) { estadoAtual = DERROTA; jogadoresAtingidos|=2; }
    for (Inimigo& inimigo : listaInimigos) {
        if (inimigo.vivo && corpoNaExplosao(inimigo.visualL,inimigo.visualC)) matarInimigo(inimigo, pontuacao);
    }
}

// Separado de atingirPersonagens porque o boss tem vida (varios acertos) em
// vez de morrer no primeiro toque. O flag "bossAtingido" fica na propria
// bomba (nao mais global) porque agora varias bombas podem estar explodindo
// ao mesmo tempo; cada explosao conta no maximo 1 acerto, mesmo durando
// varios ticks de logica seguidos.
void atingirBoss() {
    if (!boss.vivo || boss.imunidadeMs>0) return;
    for (Bomba& b : bombas) {
        if (!b.explodindo || b.bossAtingido) continue;
        bool atingiu = false;
        for (const Posicao& p : b.celulasExplosao) if (celulaBoss(p.linha,p.coluna)) { atingiu = true; break; }
        if (!atingiu) continue;
        b.bossAtingido = true;
        boss.vida=max(0,boss.vida-4);
        boss.imunidadeMs=IMUNIDADE_BOSS_MS;
        if (boss.vida <= 0) {
            boss.vivo = false;
            bossConcluido = true;
            pontos += PONTOS_POR_BOSS;
            colocarPortal();
        }
        break;
    }
}

void iniciarExplosaoBomba(Bomba& b) {
    b.explodindo = true;
    b.tempoMs = TEMPO_EXPLOSAO_MS;
    b.celulasExplosao.clear();
    b.celulasExplosao.push_back(b.posicao);
    propagarExplosao(b.celulasExplosao, -1, 0, b.posicao, alcanceExplosao);
    propagarExplosao(b.celulasExplosao, 1, 0, b.posicao, alcanceExplosao);
    propagarExplosao(b.celulasExplosao, 0, -1, b.posicao, alcanceExplosao);
    propagarExplosao(b.celulasExplosao, 0, 1, b.posicao, alcanceExplosao);
    atingirPersonagens(inimigos, estado, pontos);
    atingirBoss();
}

void atualizarBombas(int tempoMs) {
    for (Bomba& b : bombas) {
        if (!b.ativa || b.relogio || b.explodindo) continue;
        b.tempoMs -= tempoMs;
        if (b.tempoMs <= 0) iniciarExplosaoBomba(b);
    }
    bool alguemExplodindo = false;
    for (Bomba& b : bombas) if (b.explodindo) { alguemExplodindo = true; break; }
    if (alguemExplodindo) { atingirPersonagens(inimigos, estado, pontos); atingirBoss(); }
    for (size_t i = 0; i < bombas.size();) {
        Bomba& b = bombas[i];
        if (b.explodindo) {
            b.tempoMs -= tempoMs;
            if (b.tempoMs <= 0) { bombas.erase(bombas.begin() + i); continue; }
        }
        i++;
    }
}

// Template de proposito (criterio tecnico do trabalho): as tres buscas que
// existiam nesta secao (perseguicao de inimigo/boss, busca de alvo seguro do
// bot, fuga do bot) eram a mesma busca em largura copiada tres vezes, so
// variando "quando parar" e "por quais celulas dá pra passar". Em vez de
// manter três copias quase identicas, a busca vira uma unica funcao
// parametrizada pelos dois criterios - cada chamador passa suas proprias
// lambdas, e o compilador gera uma versao especializada pra cada combinacao
// (por isso precisa ser template: cada lambda tem um tipo proprio).
template <typename EhAlvo, typename PodeAndarPor>
Posicao bfsProximoPasso(Posicao origem, EhAlvo ehAlvo, PodeAndarPor podeAndarPor) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool visitado[LINHAS][COLUNAS]{};
    Posicao anterior[LINHAS][COLUNAS];
    queue<Posicao> fila;
    fila.push(origem);
    visitado[origem.linha][origem.coluna] = true;
    Posicao objetivo = origem;
    bool encontrou = false;

    while (!fila.empty() && !encontrou) {
        Posicao atual = fila.front();
        fila.pop();
        if (ehAlvo(atual)) { objetivo = atual; encontrou = true; break; }
        for (const auto& direcao : direcoes) {
            Posicao proxima{atual.linha + direcao[0], atual.coluna + direcao[1]};
            if (!podeAndarPor(proxima) || visitado[proxima.linha][proxima.coluna]) continue;
            visitado[proxima.linha][proxima.coluna] = true;
            anterior[proxima.linha][proxima.coluna] = atual;
            fila.push(proxima);
        }
    }

    if (!encontrou || iguais(objetivo, origem)) return origem;
    while (!iguais(anterior[objetivo.linha][objetivo.coluna], origem))
        objetivo = anterior[objetivo.linha][objetivo.coluna];
    return objetivo;
}

bool podeAndarLivre(Posicao p) { return livre(p.linha, p.coluna) && !bloqueadoBoss(p); }
bool podeAndarLivreSemInimigo(Posicao p) { return podeAndarLivre(p) && !temInimigo(p); }

Posicao proximoPassoParaAlvo(Posicao origem, Posicao alvo) {
    return bfsProximoPasso(
        origem, [&](Posicao p) { return iguais(p, alvo); }, podeAndarLivre);
}

// Sempre 1 celula por decisao (nao mais uma rajada de 1-3): rajadas faziam o
// inimigo "pular" varias celulas entre uma renderizacao e outra, o que lido
// junto da interpolacao visual podia parecer um movimento diagonal/errático
// quando a direcao mudava no meio de uma rajada. Um passo por decisao, no
// ritmo de INTERVALO_INIMIGO_MIN/MAX_MS, fica mais previsivel e mais facil
// de acompanhar (pedido do usuario).
void moverIndividualAleatorio(Inimigo& inimigo, int indice) {
    const int direcoes[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    uniform_int_distribution<int> sorteioDirecao(0, 3);
    int inicio = sorteioDirecao(gerador);
    for (int tentativa = 0; tentativa < 4; tentativa++) {
        int d = (inicio + tentativa) % 4;
        Posicao destino{inimigo.posicao.linha + direcoes[d][0], inimigo.posicao.coluna + direcoes[d][1]};
        if (!livre(destino.linha, destino.coluna)) continue;
        if (bombaEm(destino)) continue;
        if (temInimigo(destino, indice)) continue;
        inimigo.posicao = destino;
        inimigo.direcao = {direcoes[d][0], direcoes[d][1]};
        inimigo.quadroAnim = (inimigo.quadroAnim + 1) % QUADROS_POR_DIRECAO;
        // Contato e chamas sao verificados apos avancar as posicoes atuais.
        return;
    }
}

// Alvo de perseguicao: o jogador 2 mais proximo quando ele existe e esta
// mais perto do inimigo do que o jogador 1 (senao os inimigos ignorariam o
// segundo jogador e so perseguiriam o primeiro).
Posicao alvoDePerseguicaoPara(Posicao origemInimigo) {
    if (!segundoJogadorAtivo()) return jogador;
    int distJ1 = abs(origemInimigo.linha - jogador.linha) + abs(origemInimigo.coluna - jogador.coluna);
    int distJ2 = abs(origemInimigo.linha - jogador2.linha) + abs(origemInimigo.coluna - jogador2.coluna);
    return distJ2 < distJ1 ? jogador2 : jogador;
}

// Tambem sempre 1 celula por decisao agora - ver comentario de
// moverIndividualAleatorio.
void moverIndividualPerseguicao(Inimigo& inimigo, int indice) {
    Posicao origemPasso = inimigo.posicao;
    Posicao alvo = proximoPassoParaAlvo(inimigo.posicao, alvoDePerseguicaoPara(inimigo.posicao));
    bool bloqueado = iguais(alvo, inimigo.posicao) || temInimigo(alvo, indice) || bombaEm(alvo);
    if (bloqueado) return;
    inimigo.posicao = alvo;
    inimigo.direcao = {alvo.linha - origemPasso.linha, alvo.coluna - origemPasso.coluna};
    inimigo.quadroAnim = (inimigo.quadroAnim + 1) % QUADROS_POR_DIRECAO;
    // Reservar a casa do jogador nao causa dano antes do contato visual.
}

void moverInimigos(int tempoMs) {
    // Cada inimigo tem seu proprio relogio (sorteado em INTERVALO_INIMIGO_MIN/MAX_MS),
    // entao eles nao andam mais todos em sincronia. Perto do jogador, ha uma
    // chance de perseguir usando o caminho mais curto (mapeado com BFS, 1
    // celula por decisao); caso contrario, mantem o passeio aleatorio
    // original. A chance e o alcance dependem da dificuldade (secao "Fases,
    // dificuldades e boss" do plano); o inimigo especial da ultima fase
    // ignora os dois e persegue sempre.
    uniform_int_distribution<int> sorteioIntervalo(INTERVALO_INIMIGO_MIN_MS, INTERVALO_INIMIGO_MAX_MS);
    uniform_int_distribution<int> sorteioChance(0, 99);
    for (int i = 0; i < static_cast<int>(inimigos.size()); i++) {
        Inimigo& inimigo = inimigos[i];
        if (!inimigo.vivo) continue;
        inimigo.tempoProximoMovimentoMs -= tempoMs;
        if (inimigo.tempoProximoMovimentoMs > 0 ||
            !visualNoDestino(inimigo.visualL, inimigo.visualC, inimigo.posicao)) continue;
        inimigo.tempoProximoMovimentoMs = sorteioIntervalo(gerador) + (dificuldade == FACIL ? 350 : 0);

        bool perseguir;
        if (inimigo.especial) {
            perseguir = true;
        } else {
            int distancia = abs(inimigo.posicao.linha - jogador.linha) + abs(inimigo.posicao.coluna - jogador.coluna);
            int chance = dificuldade == FACIL ? CHANCE_PERSEGUICAO_FACIL
                       : dificuldade == MEDIO ? CHANCE_PERSEGUICAO_MEDIO
                                               : CHANCE_PERSEGUICAO_DIFICIL;
            int alcance = dificuldade == FACIL ? DISTANCIA_PERSEGUICAO : LINHAS + COLUNAS;
            perseguir = distancia <= alcance && sorteioChance(gerador) < chance;
        }
        if (perseguir) moverIndividualPerseguicao(inimigo, i);
        else moverIndividualAleatorio(inimigo, i);
    }
}

// Limpa somente o interior percorrido; bordas e surpresa do Mario permanecem.
void abrirPassagemBoss(Posicao alvo) {
    int top=min(alvo.linha,boss.posicao.linha),bottom=max(alvo.linha,boss.posicao.linha)+2;
    int left=min(alvo.coluna,boss.posicao.coluna),right=max(alvo.coluna,boss.posicao.coluna)+3;
    for(int l=top;l<bottom;++l)for(int c=left;c<right;++c){
        if(mapa[l][c]!=VAZIO)impactosBoss.push_back({l+.5f,c+.5f,0,MACHADO});
        mapa[l][c]=VAZIO;
    }
    auto noCaminho=[&](Posicao p){return p.linha>=top && p.linha<bottom && p.coluna>=left && p.coluna<right;};
    itensNoChao.erase(remove_if(itensNoChao.begin(),itensNoChao.end(),[&](const Item& i){return noCaminho(i.posicao);}),itensNoChao.end());
    blocosBonus.erase(remove_if(blocosBonus.begin(),blocosBonus.end(),[&](const BlocoBonus& b){return noCaminho(b.posicao);}),blocosBonus.end());
    invalidarCacheMapa();
}
// Avanco normal preserva as regras do boss do Insano.
bool iniciarAvancoBoss(int dl,int dc,int distancia) {
    Posicao alvo{boss.posicao.linha+dl*distancia,boss.posicao.coluna+dc*distancia};
    if(alvo.linha<1 || alvo.coluna<1 || alvo.linha+2>LINHAS-1 || alvo.coluna+3>COLUNAS-1) return false;
    int top=min(alvo.linha,boss.posicao.linha), bottom=max(alvo.linha,boss.posicao.linha)+2;
    int left=min(alvo.coluna,boss.posicao.coluna), right=max(alvo.coluna,boss.posicao.coluna)+3;
    auto ocupa=[&](float l,float c){return c+.85f>left && c+.15f<right && l+.85f>top && l+.15f<bottom;};
    if(marioAliado.escondido && marioAliado.bloco.coluna>=left && marioAliado.bloco.coluna<right && marioAliado.bloco.linha>=top && marioAliado.bloco.linha<bottom)return false;
    if(marioAliado.ativo && marioAliado.hp>0 && (ocupa(marioAliado.l,marioAliado.c)||ocupa(marioAliado.posicao.linha,marioAliado.posicao.coluna)))return false;
    if(ocupa(jogadorVisualL,jogadorVisualC)||ocupa(jogador.linha,jogador.coluna)) return false;
    if(segundoJogadorAtivo() && (ocupa(jogador2VisualL,jogador2VisualC)||ocupa(jogador2.linha,jogador2.coluna))) return false;
    for(const Bomba& b:bombas) if(b.ativa && b.posicao.linha>=top && b.posicao.linha<bottom && b.posicao.coluna>=left && b.posicao.coluna<right) return false;
    abrirPassagemBoss(alvo);boss.destino=alvo;boss.movendo=true;boss.horizontal=!boss.horizontal;
    return true;
}
void ferirMario();
bool prepararInvestidaBoss() {
    if(boss.movendo || boss.investindo)return false;
    float alvoL=jogadorVisualL+.5f,alvoC=jogadorVisualC+.5f;
    float origemL=boss.visualL+1,origemC=boss.visualC+1.5f;
    auto considerar=[&](float l,float c){if(hypot(l+.5f-origemL,c+.5f-origemC)<hypot(alvoL-origemL,alvoC-origemC)){alvoL=l+.5f;alvoC=c+.5f;}};
    if(segundoJogadorAtivo())considerar(jogador2VisualL,jogador2VisualC);
    if(marioAliado.ativo && marioAliado.hp>0)considerar(marioAliado.l,marioAliado.c);
    int dl=0,dc=0;
    if(abs(alvoC-origemC)>=abs(alvoL-origemL))dc=alvoC>=origemC?1:-1;
    else dl=alvoL>=origemL?1:-1;
    Posicao destino=boss.posicao;
    for(int passo=1;passo<=4;++passo){Posicao p{boss.posicao.linha+dl*passo,boss.posicao.coluna+dc*passo};
        if(p.linha<1 || p.coluna<1 || p.linha+2>LINHAS-1 || p.coluna+3>COLUNAS-1)break;
        if(marioAliado.escondido && marioAliado.bloco.linha>=p.linha && marioAliado.bloco.linha<p.linha+2 && marioAliado.bloco.coluna>=p.coluna && marioAliado.bloco.coluna<p.coluna+3)break;
        destino=p;
    }
    if(iguais(destino,boss.posicao))return false;
    boss.destino=destino;boss.investindo=true;boss.avisoInvestidaMs=1000;
    return true;
}
void criarPoderBoss() {
    boss.animAtaqueMs=700;
    ++boss.sequenciaAtaque;
    auto lancar=[&](float l,float c,PoderBoss poder,float desvio=0.f){
        float origemL=boss.visualL+.8f,origemC=boss.visualC+1.5f;
        float angulo=atan2(l+.5f-origemL,c+.5f-origemC)+desvio;
        float velocidade=poder==MACHADO?2.8f:poder==ENERGIA?3.6f:3.f;
        projeteisBoss.push_back({origemL,origemC,sin(angulo)*velocidade,cos(angulo)*velocidade,0,4.5f,poder,700});
        if(poder==MACHADO)projeteisBoss.back().arma=boss.sequenciaAtaque%2;
    };
    auto participantes=[&](auto atacar){
        atacar(jogadorVisualL,jogadorVisualC);
        if(segundoJogadorAtivo())atacar(jogador2VisualL,jogador2VisualC);
        if(marioAliado.ativo && marioAliado.hp>0)atacar(marioAliado.l,marioAliado.c);
    };
    switch(indiceBoss()) {
        case MACHADO:
            participantes([&](float l,float c){lancar(l,c,MACHADO);});break;
        case ENERGIA:
            participantes([&](float l,float c){lancar(l,c,ENERGIA);});
            if(boss.movendo)boss.investidaPendente=true;else prepararInvestidaBoss();
            break;
        case VENENO:
            participantes([&](float l,float c){for(float desvio:{-.24f,0.f,.24f})lancar(l,c,VENENO,desvio);});break;
        case CHUVA_FOGO:
            // O fogo da fase 8 conserva os parametros do estudo.
            participantes([&](float l,float c){float origemL=boss.visualL+.8f,origemC=boss.visualC+1.5f;
                float dl=l+.5f-origemL,dc=c+.5f-origemC,n=max(.1f,hypot(dl,dc));
                fogosBoss.push_back({origemL,origemC,dl/n*3.2f,dc/n*3.2f,4.f});});break;
    }
}
void terminarProjetilBoss(ProjetilBoss& p) {
    impactosBoss.push_back({p.l,p.c,0,p.poder});p.vida=0;
    int l=int(p.l),c=int(p.c);
    if(p.poder==VENENO && dentro(l,c) && mapa[l][c]==VAZIO)
        areasBoss.push_back({{l,c},0,2200,VENENO});
}
void atualizarProjeteisBoss(int tempoMs) {
    float dt=tempoMs/1000.f;
    for(auto& i:impactosBoss)i.idade+=dt;
    for(auto& p:projeteisBoss){
        float restante=dt;
        if(p.avisoMs>0){int aviso=p.avisoMs;p.avisoMs=max(0,p.avisoMs-tempoMs);restante=max(0.f,(tempoMs-aviso)/1000.f);}
        // Subpassos impedem atravessar paredes/corpos quando um quadro atrasa.
        while(restante>0 && p.vida>0){float passo=min(restante,.025f);restante-=passo;
            float antesL=p.l,antesC=p.c;p.l+=p.vl*passo;p.c+=p.vc*passo;p.idade+=passo;p.vida-=passo;
            int l=int(p.l),c=int(p.c);
            if(!dentro(l,c)){p.vida=0;break;}
            if(mapa[l][c]==PAREDE_SOLIDA || (marioAliado.escondido && iguais({l,c},marioAliado.bloco))){p.l=antesL;p.c=antesC;terminarProjetilBoss(p);break;}
            if(mapa[l][c]==PAREDE_FRAGIL || mapa[l][c]==PAREDE_BONUS){
                if(mapa[l][c]==PAREDE_BONUS)destruirBlocoBonus(l,c);
                mapa[l][c]=VAZIO;invalidarCacheMapa();terminarProjetilBoss(p);break;
            }
            auto tocou=[&](float l,float c){return hypot(p.l-l-.5f,p.c-c-.5f)<.48f;};
            bool acertou=false;
            if(tocou(jogadorVisualL,jogadorVisualC)){estado=DERROTA;jogadoresAtingidos|=1;acertou=true;}
            if(segundoJogadorAtivo() && tocou(jogador2VisualL,jogador2VisualC)){estado=DERROTA;jogadoresAtingidos|=2;acertou=true;}
            if(marioAliado.ativo && marioAliado.hp>0 && tocou(marioAliado.l,marioAliado.c)){ferirMario();acertou=true;}
            if(acertou || p.vida<=0){terminarProjetilBoss(p);break;}
        }
    }
    projeteisBoss.erase(remove_if(projeteisBoss.begin(),projeteisBoss.end(),[](const ProjetilBoss& p){return p.vida<=0;}),projeteisBoss.end());
    impactosBoss.erase(remove_if(impactosBoss.begin(),impactosBoss.end(),[](const ImpactoBoss& i){return i.idade>=.4f;}),impactosBoss.end());
}
void atualizarBoss(int tempoMs) {
    if(!boss.vivo) return;
    float dt=tempoMs/1000.f;
    boss.imunidadeMs=max(0,boss.imunidadeMs-tempoMs);
    boss.animAtaqueMs=max(0,boss.animAtaqueMs-tempoMs);
    if(boss.avisoInvestidaMs>0){
        boss.avisoInvestidaMs=max(0,boss.avisoInvestidaMs-tempoMs);
        if(boss.avisoInvestidaMs==0){abrirPassagemBoss(boss.destino);boss.movendo=true;}
    } else if(boss.movendo) {
        float dl=boss.destino.linha-boss.visualL, dc=boss.destino.coluna-boss.visualC;
        float distancia=hypot(dl,dc), passo=min(distancia,dt*(boss.investindo?4.8f:1.5f));
        if(distancia>0){boss.visualL+=dl*passo/distancia;boss.visualC+=dc*passo/distancia;}
        if(distancia-passo<.0001f){boss.posicao=boss.destino;boss.visualL=float(boss.posicao.linha);boss.visualC=float(boss.posicao.coluna);boss.movendo=false;boss.investindo=false;boss.tempoProximoMovimentoMs=INTERVALO_MOVIMENTO_BOSS_MS;}
    } else if((boss.tempoProximoMovimentoMs-=tempoMs)<=0) {
        int dl[4]={-1,0,1,0},dc[4]={0,1,0,-1};
        int preferido=boss.horizontal?(gerador()%2?1:3):(gerador()%2?0:2), outro=boss.horizontal?0:1;
        bool moveu=false;
        for(int distancia:{2,1}) {for(int d:{preferido,(preferido+2)%4,outro,(outro+2)%4}) if(iniciarAvancoBoss(dl[d],dc[d],distancia)){moveu=true;break;} if(moveu)break;}
        if(!moveu)boss.tempoProximoMovimentoMs=750;
    }
    if(boss.investidaPendente && !boss.movendo && !boss.investindo){boss.investidaPendente=false;prepararInvestidaBoss();}
    if((boss.tempoProximoAtaqueMs-=tempoMs)<=0) {
        boss.tempoProximoAtaqueMs=INTERVALO_ATAQUE_BOSS_MS;
        criarPoderBoss();
    }
    atualizarProjeteisBoss(tempoMs);
    for(auto& area:areasBoss) {
        if(area.avisoMs>0){int restante=area.avisoMs-tempoMs;area.avisoMs=max(0,restante);if(restante<0)area.ativoMs+=restante;}
        else area.ativoMs-=tempoMs;
        if(area.avisoMs<=0 && area.ativoMs>0){
            if(marioAliado.ativo && hypot(marioAliado.l-area.posicao.linha,marioAliado.c-area.posicao.coluna)<.72f)ferirMario();
            if(hypot(jogadorVisualL-area.posicao.linha,jogadorVisualC-area.posicao.coluna)<.72f){estado=DERROTA;jogadoresAtingidos|=1;}
            if(segundoJogadorAtivo() && hypot(jogador2VisualL-area.posicao.linha,jogador2VisualC-area.posicao.coluna)<.72f){estado=DERROTA;jogadoresAtingidos|=2;}
        }
    }
    areasBoss.erase(remove_if(areasBoss.begin(),areasBoss.end(),[](const AreaBoss& a){return a.ativoMs<=0;}),areasBoss.end());
    for(auto& f:fogosBoss){
        f.l+=f.vl*dt;f.c+=f.vc*dt;f.vida-=dt;
        int l=int(f.l),c=int(f.c);
        if(marioAliado.ativo && marioAliado.hp>0 && hypot(f.l-marioAliado.l-.5f,f.c-marioAliado.c-.5f)<.5f){ferirMario();f.vida=0;}
        if(!dentro(l,c))f.vida=0;
        else if(mapa[l][c]==PAREDE_SOLIDA && l>2)f.vida=0;
        else if(marioAliado.escondido && iguais({l,c},marioAliado.bloco)){f.vida=0;}
        else if(mapa[l][c]==PAREDE_FRAGIL || mapa[l][c]==PAREDE_BONUS){mapa[l][c]=VAZIO;f.vida=0;invalidarCacheMapa();}
        if(hypot(f.l-jogadorVisualL-.5f,f.c-jogadorVisualC-.5f)<.5f){estado=DERROTA;jogadoresAtingidos|=1;f.vida=0;}
        if(segundoJogadorAtivo() && hypot(f.l-jogador2VisualL-.5f,f.c-jogador2VisualC-.5f)<.5f){estado=DERROTA;jogadoresAtingidos|=2;f.vida=0;}
    }
    fogosBoss.erase(remove_if(fogosBoss.begin(),fogosBoss.end(),[](const FogoBoss& f){return f.vida<=0;}),fogosBoss.end());
}

// So "vence de verdade" ao pisar no portal (ver moverJogador); aqui so
// decidimos o que colocar no mapa quando os inimigos da leva atual acabam:
// portal direto nas fases impares, ou o boss nas fases pares (e so depois dele
// derrotado e que colocarPortal() e chamado, por atingirBoss()).
void verificarVitoria() {
    if (estado != JOGANDO || portalAtivo) return;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) return;
    if (faseAtual % 2 == 0 && !bossConcluido) {
        if (!bossIniciado) iniciarBoss();
        return;
    }
    colocarPortal();
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
    for (const Bomba& b : bombas) if (b.ativa && celulaNoAlcance(b.posicao, p)) return true;
    return false;
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
            if (!podeAndarLivre(proxima) || visitado[proxima.linha][proxima.coluna] ||
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
            if (temInimigo({l, c}) || celulaBoss(l,c)) return true;
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
    return bfsProximoPasso(jogador, alvoParaBombaSeguro, podeAndarLivreSemInimigo);
}

// BFS ate a celula segura mais proxima (fora do alcance de qualquer bomba
// ativa). Substitui a fuga antiga, que so olhava 1 passo a frente e podia
// oscilar de volta pra cima da propria bomba.
bool perigoDoBoss(Posicao p) {
    if(!boss.vivo)return false;
    if(boss.investindo){int top=min(boss.posicao.linha,boss.destino.linha),left=min(boss.posicao.coluna,boss.destino.coluna);
        if(p.linha>=top && p.linha<max(boss.posicao.linha,boss.destino.linha)+2 && p.coluna>=left && p.coluna<max(boss.posicao.coluna,boss.destino.coluna)+3)return true;}
    for(const auto& a:areasBoss)if(iguais(a.posicao,p))return true;
    for(const auto& f:projeteisBoss)if(f.avisoMs<=0 && hypot(f.l-p.linha-.5f,f.c-p.coluna-.5f)<1.2f)return true;
    for(const auto& f:fogosBoss)if(hypot(f.l-p.linha-.5f,f.c-p.coluna-.5f)<1.2f)return true;
    return false;
}
Posicao proximoPassoParaFuga() {
    return bfsProximoPasso(
        jogador, [](Posicao p) { return !perigoDaBomba(p) && !perigoDoBoss(p); }, podeAndarLivreSemInimigo);
}

void ferirMario() {
    if(estado!=JOGANDO || !marioAliado.ativo || marioAliado.hp<=0 || marioAliado.imunidade>0)return;
    --marioAliado.hp;marioAliado.imunidade=1.6f;
    if(marioAliado.hp==0){estado=DERROTA;jogadoresAtingidos|=4;}
}
bool marioPodeAndar(Posicao p){return dentro(p.linha,p.coluna) && livre(p.linha,p.coluna) && !bombaEm(p) && !bloqueadoBoss(p);}
int fugaMario(){
    struct No {Posicao p;int primeiro;};queue<No> fila;bool visto[LINHAS][COLUNAS]{};
    fila.push({marioAliado.posicao,-1});visto[marioAliado.posicao.linha][marioAliado.posicao.coluna]=true;
    while(!fila.empty()){
        No n=fila.front();fila.pop();
        if(n.primeiro>=0 && !perigoDaBomba(n.p) && !perigoDoBoss(n.p))return n.primeiro;
        int inicio=gerador()%4;
        for(int k=0;k<4;++k){int d=(inicio+k)%4;Posicao p{n.p.linha+MARIO_DL[d],n.p.coluna+MARIO_DC[d]};
            if(marioPodeAndar(p) && !visto[p.linha][p.coluna]){visto[p.linha][p.coluna]=true;fila.push({p,n.primeiro<0?d:n.primeiro});}}
    }
    return -1;
}
void lancarMacaMario(){
    auto& m=marioAliado;if(!m.ativo || m.hp<=0 || m.recarga>0)return;
    float tx=m.c+MARIO_DC[m.direcao]*3+.5f,ty=m.l+MARIO_DL[m.direcao]*3+.5f,maisPerto=1e9f;
    for(const auto& i:inimigos)if(i.vivo){float d=hypot(i.visualC-m.c,i.visualL-m.l);
        if(d<maisPerto){maisPerto=d;tx=i.posicao.coluna+.5f;ty=i.posicao.linha+.5f;}}
    if(boss.vivo && hypot(m.c-boss.visualC-1,m.l-boss.visualL)<maisPerto){tx=boss.visualC+1.5f;ty=boss.visualL+1;}
    tx=clamp(tx,1.5f,COLUNAS-1.5f);ty=clamp(ty,1.5f,LINHAS-1.5f);
    float sx=m.c+.5f,sy=m.l+.5f;
    macasMario.push_back({sx,sy,tx,ty,0,clamp(hypot(tx-sx,ty-sy)/5,.65f,1.45f)});
    m.recarga=1.05f;m.pose=.28f;
}
void atualizarMario(int tempoMs){
    auto& m=marioAliado;if(!m.ativo || estado!=JOGANDO)return;
    float dt=tempoMs/1000.f;m.imunidade=max(0.f,m.imunidade-dt);m.recarga-=dt;m.pose=max(0.f,m.pose-dt);
    if(m.hp>0){
        m.andando=false;
        if(m.movendo>=0){float dl=m.posicao.linha-m.l,dc=m.posicao.coluna-m.c,n=hypot(dl,dc),passo=min(n,4.2f*dt);
            if(n>0){m.l+=dl*passo/n;m.c+=dc*passo/n;}m.distancia+=passo;m.andando=passo>0;
            if(n-passo<.0001f){m.l=float(m.posicao.linha);m.c=float(m.posicao.coluna);m.movendo=-1;}}
        if(corpoNaExplosao(m.l,m.c))ferirMario();
        if(boss.investindo && contatoComInimigo(m.l,m.c))ferirMario();
        for(const auto& i:inimigos)if(i.vivo && hypot(m.l-i.visualL,m.c-i.visualC)<.55f)ferirMario();
        for(auto it=macasNoChao.begin();it!=macasNoChao.end();)if(iguais(*it,m.posicao)){it=macasNoChao.erase(it);pontos+=100;m.recarga=0;}else ++it;
        m.pensar-=dt;
        if(m.movendo<0 && m.pensar<=0){m.pensar=.04f;int direcao=-1;
            if(perigoDaBomba(m.posicao)||perigoDoBoss(m.posicao))direcao=fugaMario();
            else{float melhor=-1e9f;for(int d=0;d<4;++d){Posicao p{m.posicao.linha+MARIO_DL[d],m.posicao.coluna+MARIO_DC[d]};
                if(!marioPodeAndar(p)||perigoDaBomba(p)||perigoDoBoss(p))continue;
                float valor=(gerador()%10000)/10000.f*3+(d==m.direcao?.5f:0);
                for(Posicao maca:macasNoChao)if(iguais(maca,p))valor+=5;
                if(boss.vivo)valor-=.25f*(abs(p.coluna-boss.visualC-1)+abs(p.linha-boss.visualL));
                for(const auto& i:inimigos)if(i.vivo){float distancia=hypot(float(p.coluna-i.posicao.coluna),float(p.linha-i.posicao.linha));if(distancia<2)valor-=2;else if(distancia<5)valor+=.3f;}
                if(valor>melhor){melhor=valor;direcao=d;}}}
            if(direcao>=0){Posicao p{m.posicao.linha+MARIO_DL[direcao],m.posicao.coluna+MARIO_DC[direcao]};
                if(marioPodeAndar(p)){m.posicao=p;m.movendo=direcao;m.direcao=direcao;}}
        }
        lancarMacaMario();
    }
    for(auto& a:macasMario){a.idade+=dt;if(a.idade<a.duracao)continue;
        Posicao p{int(a.ty),int(a.tx)};bool acertou=false;
        if(dentro(p.linha,p.coluna) && (mapa[p.linha][p.coluna]==PAREDE_FRAGIL || mapa[p.linha][p.coluna]==PAREDE_BONUS)){
            if(mapa[p.linha][p.coluna]==PAREDE_BONUS)destruirBlocoBonus(p.linha,p.coluna);
            mapa[p.linha][p.coluna]=VAZIO;invalidarCacheMapa();pontos+=10;acertou=true;}
        for(auto& i:inimigos)if(i.vivo && hypot(a.tx-i.visualC-.5f,a.ty-i.visualL-.5f)<.95f){matarInimigo(i,pontos);acertou=true;}
        for(auto& b:bombas)if(b.ativa && iguais(b.posicao,p)){b.tempoMs=0;acertou=true;}
        if(celulaBoss(p.linha,p.coluna)){
            boss.vida=max(0,boss.vida-3);boss.imunidadeMs=IMUNIDADE_BOSS_MS;acertou=true;pontos+=50;
            if(boss.vida==0){boss.vivo=false;bossConcluido=true;pontos+=PONTOS_POR_BOSS;colocarPortal();}
        }
        if(acertou)pontos+=25;
    }
    macasMario.erase(remove_if(macasMario.begin(),macasMario.end(),[](const MacaMario& a){return a.idade>=a.duracao;}),macasMario.end());
}

void atualizarBot() {
    // Decide a cada tick de logica (nao mais a cada INTERVALO_BOT_MS), mas o
    // movimento em si continua pausado pelo mesmo cooldown do jogador manual
    // (tempoMovimentoMs, dentro de moverJogador). Isso elimina a dessincronia
    // entre "quando o bot decide" e "quando o jogo deixa mover", que causava
    // o personagem parecendo travar/bater em paredes no modo automatico.
    if (modo != AUTOMATICO || estado != JOGANDO || tempoMovimentoMs > 0) return;
    if (!bombas.empty() || perigoDoBoss(jogador)) {
        if (!perigoDaBomba(jogador) && !perigoDoBoss(jogador)) return;
        Posicao destino = proximoPassoParaFuga();
        if (!iguais(destino, jogador)) moverJogador(destino.linha - jogador.linha, destino.coluna - jogador.coluna);
        return;
    }
    if(portalAtivo) {
        Posicao destino=proximoPassoParaAlvo(jogador,portalPosicao);
        if(!iguais(destino,jogador)) moverJogador(destino.linha-jogador.linha,destino.coluna-jogador.coluna);
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
    // Com o jogador 2 ativo, as setas viram exclusividade dele - senao as
    // duas leituras reagiriam a mesma tecla e os dois andariam juntos.
    auto moverPorTeclas=[&](bool p2) {
        struct Entrada { int tecla,dl,dc; };
        vector<Entrada> entradas;
        const Entrada letras[]={{'W',-1,0},{'S',1,0},{'A',0,-1},{'D',0,1}};
        const Entrada setas[]={{VK_UP,-1,0},{VK_DOWN,1,0},{VK_LEFT,0,-1},{VK_RIGHT,0,1}};
        if(!p2) for(auto e:letras) if(teclaPressionada[e.tecla]) entradas.push_back(e);
        if(p2 || !doisJogadores) for(auto e:setas) if(teclaPressionada[e.tecla]) entradas.push_back(e);
        stable_sort(entradas.begin(),entradas.end(),[](const Entrada& a,const Entrada& b){return ordemTecla[a.tecla]>ordemTecla[b.tecla];});
        for(auto e:entradas) {
            Posicao antes=p2 ? jogador2 : jogador;
            if(p2) moverJogador2(e.dl,e.dc); else moverJogador(e.dl,e.dc);
            if(!iguais(antes,p2 ? jogador2 : jogador) || estado!=JOGANDO) break;
        }
    };
    moverPorTeclas(false);
    if(segundoJogadorAtivo()) moverPorTeclas(true);
}

void atualizar(int tempoMs) {
    relogioAnimacaoMs += tempoMs;
    if (modo == MENU) return;
    if (estado != JOGANDO) {
        tempoFimMs += tempoMs;
        if (modo == AUTOMATICO && tempoFimMs >= 1400) {
            if (estado == DERROTA) reiniciarFase();
            else if (estado == FIM_DE_JOGO) novaPartida(AUTOMATICO, dificuldade);
        }
        return;
    }
    tempoPartidaMs += tempoMs;
    tempoMovimentoMs = max(0, tempoMovimentoMs - tempoMs);
    if (doisJogadores) tempoMovimentoMs2 = max(0, tempoMovimentoMs2 - tempoMs);
    processarMovimentoContinuo();
    if (modo == AUTOMATICO) atualizarBot();
    atualizarBombas(tempoMs);
    moverInimigos(tempoMs);
    atualizarBoss(tempoMs);
    atualizarMario(tempoMs);
    // Avanca os corpos ate os destinos reservados antes de verificar contato.
    float deslocamentoJogador = avancarVisual(jogadorVisualL, jogadorVisualC, jogador, tempoMs);
    distanciaAnimacaoJogador += deslocamentoJogador;
    quadroAnimJogador = quadroCaminhada(distanciaAnimacaoJogador, deslocamentoJogador>0);
    if (doisJogadores) {
        float passo=avancarVisual(jogador2VisualL, jogador2VisualC, jogador2, tempoMs);
        distanciaAnimacaoJogador2 += passo;
        quadroAnimJogador2 = quadroCaminhada(distanciaAnimacaoJogador2,passo>0);
    }
    for (Inimigo& inimigo : inimigos) {
        if (!inimigo.vivo) continue;
        float passo=avancarVisual(inimigo.visualL, inimigo.visualC, inimigo.posicao, tempoMs);
        inimigo.distanciaAnimacao += passo;
        inimigo.quadroAnim = quadroCaminhada(inimigo.distanciaAnimacao,passo>0);
    }
    // O boss interpola seu proprio avanco a 1.5 casas/s.
    atingirPersonagens(inimigos,estado,pontos);
    atingirBoss();
    verificarContatoJogadores();
    if(estado==JOGANDO && visualNoDestino(jogadorVisualL,jogadorVisualC,jogador)) {
        coletarItens(); avancarOuVencerSePortal(jogador);
    }
    if(estado==JOGANDO && segundoJogadorAtivo() && visualNoDestino(jogador2VisualL,jogador2VisualC,jogador2)) {
        coletarItensEm(jogador2); avancarOuVencerSePortal(jogador2);
    }
    verificarVitoria();
    // Funil unico pra descontar vida: roda exatamente uma vez por morte (no
    // mesmo tick em que o estado vira DERROTA), nao importa qual checagem
    // acima foi a responsavel. Zerou as vidas -> fim de jogo de verdade.
    if (estado == DERROTA && !vidaPerdida) {
        if (!jogadoresAtingidos) jogadoresAtingidos=1;
        vidaPerdida = true;
        vidas--;
        if (vidas <= 0) estado = FIM_DE_JOGO;
    }
    // So gravamos partidas manuais no ranking, e so quando a partida de fato
    // termina (fim de jogo ou vitoria) - "derrota" sozinha agora e so perder
    // uma vida e tentar de novo, nao e o fim da partida.
    if ((estado == FIM_DE_JOGO || estado == VITORIA) && modo == MANUAL && !registroSalvo) {
        registroSalvo = true;
        RegistroPontuacao registro;
        registro.data = dataAtual();
        registro.jogador = nomeJogador.empty() ? "Jogador" : wparaStr(nomeJogador);
        registro.pontuacao = pontos;
        registro.bombas = estatisticas.bombasUsadas;
        registro.movimentos = estatisticas.movimentos;
        salvarRegistroPartida(registro);
    }
}

int inimigosVivos() {
    int total = 0;
    for (const Inimigo& inimigo : inimigos) if (inimigo.vivo) total++;
    return total;
}

int px(int valor) { return static_cast<int>(valor * escalaDpi + 0.5f); }
float px(float valor) { return valor * escalaDpi; }

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

// Desenha um recorte de origem (x,y,sw,sh) de "origem" num bitmap novo,
// pequeno, so do tamanho do recorte - usado pra nao guardar/recortar
// repetidamente de uma folha gigante (ver comentario de spriteTemaFase2Chao).
unique_ptr<Gdiplus::Bitmap> recortarEmBitmapCompacto(Gdiplus::Image* origem, int sx, int sy, int sw, int sh) {
    auto destino = make_unique<Gdiplus::Bitmap>(sw, sh, PixelFormat32bppARGB);
    Gdiplus::Graphics g(destino.get());
    g.DrawImage(origem, Gdiplus::Rect(0, 0, sw, sh), sx, sy, sw, sh, Gdiplus::UnitPixel);
    return destino;
}

void prepararTemaFase2Compacto() {
    auto dungeon = carregarSprite(L"GameCube - Bomberman Land 2 - Tilesets - Survival Bomberman Tiles (Dungeon).png");
    if (!dungeon) return;
    spriteTemaFase2Chao = recortarEmBitmapCompacto(dungeon.get(), 315, 760, 32, 32);
    spriteTemaFase2Parede = recortarEmBitmapCompacto(dungeon.get(), 345, 585, 32, 32);
    spriteTemaFase2Caixa = recortarEmBitmapCompacto(dungeon.get(), 696, 696, 32, 32);
    // "dungeon" (unique_ptr local, ~23MB decodificados) e liberado ao sair
    // desta funcao - nunca fica guardado inteiro em memoria.
}

// Recortes compactos dos sprites fornecidos, sem manter as folhas grandes.
void carregarBossesCampanha() {
    auto ff=carregarSprite(L"SNES - Final Fantasy V - Enemies & Bosses - Enemies.png");
    auto egg=carregarSprite(L"Sega CD - Sonic the Hedgehog CD - Bosses - Palmtree Panic Boss - EGG-HVC-001.png");
    auto recorte=[&](Gdiplus::Image* folha,int x,int y,int w,int h){
        auto b=recortarEmBitmapCompacto(folha,x,y,w,h);
        Gdiplus::Color fundo;b->GetPixel(0,0,&fundo);
        // As folhas usam fundo e retangulos de organizacao em cores solidas.
        for(UINT yy=0;yy<b->GetHeight();++yy)for(UINT xx=0;xx<b->GetWidth();++xx){
            Gdiplus::Color cor;b->GetPixel(xx,yy,&cor);
            if(cor.GetValue()==fundo.GetValue() || (cor.GetR()==0 && cor.GetG()==44 && cor.GetB()==76) || (cor.GetR()==16 && cor.GetG()==112 && cor.GetB()==132) || (cor.GetR()==65 && cor.GetG()==169 && cor.GetB()==184))
                b->SetPixel(xx,yy,Gdiplus::Color(0,0,0,0));
        }
        return b;
    };
    if(ff){spritesBossCampanha[0]=recorte(ff.get(),656,16,40,64);spritesBossCampanha[2]=recorte(ff.get(),496,296,80,64);}
    if(ff)spriteMachadoBoss=recorte(ff.get(),536,216,32,36);
    auto soldado=carregarSprite(L"Neo Geo _ NGCD - Metal Slug 2 _ Metal Slug X - Enemies & Bosses - Ajirabian Soldier.png");
    if(soldado){
        // Cinco poses de arremesso isoladas na propria folha, sem soldado junto.
        const int caixas[5][4]={{4,598,35,14},{42,593,31,25},{76,586,19,36},{98,586,18,36},{119,594,30,26}};
        for(int f=0;f<5;++f){const auto& r=caixas[f];auto arma=recorte(soldado.get(),r[0],r[1],r[2],r[3]);
            facoesBoss[f]=make_unique<Gdiplus::Bitmap>(40,40,PixelFormat32bppARGB);
            Gdiplus::Graphics g(facoesBoss[f].get());g.Clear(Gdiplus::Color(0,0,0,0));
            g.DrawImage(arma.get(),(40-r[2])/2,(40-r[3])/2,r[2],r[3]);
        }
    }
    if(egg){spritesBossCampanha[1]=recorte(egg.get(),869,424,112,124);
        posesEggBoss[0]=recorte(egg.get(),781,424,80,124);posesEggBoss[1]=recorte(egg.get(),989,424,104,132);}
    auto fx=carregarSprite(L"BombsExplosionsv2.png");
    if(fx)for(int f=0;f<4;++f){
        energiaBoss[f]=recorte(fx.get(),32,528+16*f,32,16);
        venenoBoss[f]=recorte(fx.get(),160,272+16*f,32,16);
        impactoBoss[f]=recorte(fx.get(),32,16+16*f,32,16);
        // O atlas de efeitos usa verde puro como chave, diferente das folhas de boss.
        for(auto* b:{energiaBoss[f].get(),venenoBoss[f].get(),impactoBoss[f].get()})
            for(UINT y=0;y<b->GetHeight();++y)for(UINT x=0;x<b->GetWidth();++x){Gdiplus::Color c;b->GetPixel(x,y,&c);
                if(c.GetR()==10 && c.GetB()==10 && c.GetG()==203)b->SetPixel(x,y,Gdiplus::Color(0,0,0,0));}
    }
}
void carregarAssets() {
    carregarBossesCampanha();
    auto marioFolha=carregarSprite(L"mario.png");
    if(marioFolha)for(int d=0;d<4;++d)for(int f=0;f<3;++f){
        int x=(d==0?60:d==1?114:6)+f*17,y=d==2?24:6;
        quadrosMario[d][f]=recortarEmBitmapCompacto(marioFolha.get(),x,y,16,16);
        for(int yy=0;yy<16;++yy)for(int xx=0;xx<16;++xx){Gdiplus::Color cor;quadrosMario[d][f]->GetPixel(xx,yy,&cor);
            if(cor.GetR()==56 && cor.GetG()==135 && cor.GetB()==0)quadrosMario[d][f]->SetPixel(xx,yy,Gdiplus::Color(0,0,0,0));}
    }
    spriteMacaMario=make_unique<Gdiplus::Bitmap>(12,14,PixelFormat32bppARGB);
    {Gdiplus::Graphics g(spriteMacaMario.get());g.Clear(Gdiplus::Color(0,0,0,0));
        Gdiplus::SolidBrush contorno(Gdiplus::Color(255,103,28,38)),vermelho(Gdiplus::Color(255,241,61,57)),luz(Gdiplus::Color(255,255,147,114)),folha(Gdiplus::Color(255,107,220,75)),talo(Gdiplus::Color(255,114,73,42));
        g.FillRectangle(&talo,6,0,2,4);g.FillRectangle(&folha,8,1,3,2);g.FillRectangle(&contorno,2,4,8,9);g.FillRectangle(&contorno,1,6,10,5);
        g.FillRectangle(&vermelho,3,4,3,8);g.FillRectangle(&vermelho,7,4,2,8);g.FillRectangle(&vermelho,2,6,8,5);g.FillRectangle(&vermelho,4,11,4,2);g.FillRectangle(&luz,3,5,2,3);}

    spriteArenasEstudo = carregarSprite(L"arenas_estudo.png");
    const wchar_t* arquivos[] = {L"branco", L"preto", L"azul", L"vermelho"};
    for (int i=0;i<4;++i) {
        personagens[i] = carregarSprite(wstring(L"personagem_")+arquivos[i]+L".png");
        posesVitoria[i] = carregarSprite(wstring(arquivos[i])+L"_vitoria.png");
        posesDerrota[i] = carregarSprite(wstring(arquivos[i])+L"_derrota.png");
    }
    spriteItens = carregarSprite(L"itens_especiais.png");
    spriteVitoria = carregarSprite(L"jogador_vitoria.png");
    spriteDerrota = carregarSprite(L"jogador_derrota.png");
    spriteBoss = carregarSprite(L"boss.png");
    spriteFogoBoss = make_unique<Gdiplus::Bitmap>((pastaDoExecutavel()+L"\\assets\\fire_2.png").c_str());
    if(auto* bitmap=dynamic_cast<Gdiplus::Bitmap*>(spriteFogoBoss.get())) {
        for(UINT y=0;y<bitmap->GetHeight();++y)for(UINT x=0;x<bitmap->GetWidth();++x){
            Gdiplus::Color cor;bitmap->GetPixel(x,y,&cor);
            if((cor.GetR()==84 && cor.GetG()==109 && cor.GetB()==142)||(cor.GetR()==112 && cor.GetG()==154 && cor.GetB()==209))bitmap->SetPixel(x,y,Gdiplus::Color(0,0,0,0));
        }
    }
    spriteBombaAnimada = carregarSprite(L"bomba_animada.png");
    spriteExplosaoAnimada = carregarSprite(L"explosao_animada.png");
    spriteInimigoDirecional = carregarSprite(L"inimigo_direcional.png");
    spriteTemaFase1 = carregarSprite(L"SNES - Super Bomberman - Story Mode Stages - Stage 2-5.png");
    spriteTemaFase3 = carregarSprite(L"SNES - Super Bomberman - Story Mode Stages - Stage 6-3.png");
    prepararTemaFase2Compacto();
}

#pragma pack(push, 1)
struct CabecalhoWav {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t tamanhoArquivo = 0;
    char onda[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t tamanhoFmt = 16;
    uint16_t formato = 1; // PCM
    uint16_t canais = 1;
    uint32_t amostragem = 11025;
    uint32_t bytesPorSegundo = 11025;
    uint16_t alinhamentoBloco = 1;
    uint16_t bitsPorAmostra = 8;
    char dataTag[4] = {'d', 'a', 't', 'a'};
    uint32_t tamanhoDados = 0;
};
#pragma pack(pop)

// Gera uma trilha curta em loop por codigo (onda quadrada, estilo chiptune),
// sem depender de nenhum arquivo de audio externo nem de internet - resolve
// o item "colocar musica no jogo" com um .wav PCM 8 bits montado em memoria.
vector<unsigned char> gerarMusicaProcedural() {
    const int amostragem = 11025;
    const double duracaoNota = 0.17;
    // Melodia curta e simples (G4 A4 C5 A4 G4 F4 G4 C5), repete em loop pelo
    // proprio SND_LOOP do PlaySound - nao precisa duplicar os dados aqui.
    const double notas[] = {392.00, 440.00, 523.25, 440.00, 392.00, 349.23, 392.00, 523.25};
    int amostrasPorNota = static_cast<int>(amostragem * duracaoNota);
    vector<unsigned char> amostras;
    amostras.reserve(amostrasPorNota * size(notas));
    for (double frequencia : notas) {
        for (int i = 0; i < amostrasPorNota; i++) {
            double t = static_cast<double>(i) / amostragem;
            double ciclo = fmod(t * frequencia, 1.0);
            double onda = ciclo < 0.5 ? 1.0 : -1.0;
            // Fade-out suave no fim de cada nota pra evitar estalo na troca.
            double envelope = 1.0 - (static_cast<double>(i) / amostrasPorNota) * 0.4;
            int amostra = static_cast<int>(128 + onda * envelope * 40);
            amostra = max(0, min(255, amostra));
            amostras.push_back(static_cast<unsigned char>(amostra));
        }
    }

    CabecalhoWav cabecalho;
    cabecalho.tamanhoDados = static_cast<uint32_t>(amostras.size());
    cabecalho.tamanhoArquivo = static_cast<uint32_t>(sizeof(CabecalhoWav) - 8 + amostras.size());
    vector<unsigned char> arquivo(sizeof(CabecalhoWav) + amostras.size());
    memcpy(arquivo.data(), &cabecalho, sizeof(CabecalhoWav));
    memcpy(arquivo.data() + sizeof(CabecalhoWav), amostras.data(), amostras.size());
    return arquivo;
}

// Precisa ficar viva enquanto a musica toca (PlaySound com SND_MEMORY le
// direto deste buffer, inclusive repetidamente por causa do SND_LOOP).
vector<unsigned char> bufferMusica;

void iniciarMusica() {
    bufferMusica = gerarMusicaProcedural();
    PlaySoundW(reinterpret_cast<LPCWSTR>(bufferMusica.data()), nullptr,
              SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
}

void pararMusica() {
    PlaySoundW(nullptr, nullptr, 0);
}

void liberarBuffer() {
    if (bufferDC) { DeleteDC(bufferDC); bufferDC = nullptr; }
    if (bufferBitmap) { DeleteObject(bufferBitmap); bufferBitmap = nullptr; }
    bufferLargura = 0;
    bufferAltura = 0;
}

void liberarAssets() {
    spriteArenasEstudo.reset();
    for(int i=0;i<4;++i) { personagens[i].reset(); posesVitoria[i].reset(); posesDerrota[i].reset(); }
    spriteItens.reset();
    spriteVitoria.reset();
    spriteDerrota.reset();
    spriteBoss.reset(); spriteFogoBoss.reset();
    for(auto& sprite:spritesBossCampanha)sprite.reset();
    spriteMachadoBoss.reset();
    for(auto& p:facoesBoss)p.reset();
    for(auto& p:posesEggBoss)p.reset();
    for(auto& p:energiaBoss)p.reset();
    for(auto& p:venenoBoss)p.reset();
    for(auto& p:impactoBoss)p.reset();
    for(auto& direcao:quadrosMario)for(auto& quadro:direcao)quadro.reset();
    spriteMacaMario.reset();
    spriteBombaAnimada.reset();
    spriteExplosaoAnimada.reset();
    spriteInimigoDirecional.reset();
    spriteTemaFase1.reset();
    spriteTemaFase2Chao.reset();
    spriteTemaFase2Parede.reset();
    spriteTemaFase2Caixa.reset();
    spriteTemaFase3.reset();
    cacheFontes.clear();
    cacheMapa.reset();
    cacheMapaSujo = true;
    cacheSpriteBoss.reset();
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
    const wchar_t* nomeFonte = L"Segoe UI";
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
                        px(LARGURA_JOGO - static_cast<int>(x) - (centralizado ? static_cast<int>(x) : 12)), px(50));
    g.DrawString(texto.c_str(), -1, fonte, area, &formato, &pincel);
}

void painel(Gdiplus::Graphics& g, int x, int y, int largura, int altura,
            Gdiplus::Color cor = Gdiplus::Color(255, 25, 34, 50)) {
    Gdiplus::SolidBrush pincel(cor);
    g.FillRectangle(&pincel, px(x), px(y), px(largura), px(altura));
}

// Recorte generico para montagens e poses finais, preservando proporcao.
void desenharRecorte(Gdiplus::Graphics& g, Gdiplus::Image* folha, int sx, int sy,
                    int largura, int altura, int x, int y, int lado) {
    if (!folha) return;
    float escala = static_cast<float>(lado) / max(largura, altura);
    int w = static_cast<int>(largura * escala), h = static_cast<int>(altura * escala);
    g.DrawImage(folha, Gdiplus::Rect(px(x + (lado-w)/2), px(y + (lado-h)/2), px(w), px(h)),
                sx, sy, largura, altura, Gdiplus::UnitPixel);
}

void fundoInterface(Gdiplus::Graphics& g) {
    g.Clear(Gdiplus::Color(255, 11, 16, 28));
    painel(g, 0, 0, LARGURA_JOGO, 5, Gdiplus::Color(255, 252, 187, 65));
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
                            float linha, float coluna, const wchar_t* fallback) {
    int xCelula = static_cast<int>(coluna * TAMANHO_CELULA * escalaDpi);
    int yCelula = static_cast<int>((ALTURA_CABECALHO + linha * TAMANHO_CELULA) * escalaDpi);
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

// DOWN=linha0, RIGHT=linha1, UP=linha2, LEFT=linha3 na folha direcional -
// mesma ordem usada no fatiamento de novos_perssonagens.png (ver estudos
// em D:\algoritmos_prog_2\estudos_sprite\batalha.cpp).
int linhaDaFolhaParaDirecao(Posicao direcao) {
    if (direcao.linha == 1) return 0;
    if (direcao.coluna == 1) return 1;
    if (direcao.linha == -1) return 2;
    return 3;
}

// Desenha um quadro especifico (linha=direcao, coluna=passo da caminhada) de
// uma folha direcional 3x4, recortando e escalando num unico DrawImage - sem
// precisar do cache de pre-escala, ja que e um blit so (mesmo custo de
// desenharSpriteNaCelula desenhando a imagem inteira).
void desenharQuadroNaCelula(Gdiplus::Graphics& g, Gdiplus::Image* folha, float linha, float coluna,
                            int linhaFolha, int quadro, const wchar_t* fallback) {
    int xCelula = static_cast<int>(coluna * TAMANHO_CELULA * escalaDpi);
    int yCelula = static_cast<int>((ALTURA_CABECALHO + linha * TAMANHO_CELULA) * escalaDpi);
    if (!folha) {
        desenharTexto(g, fallback, coluna * TAMANHO_CELULA + 4,
                      ALTURA_CABECALHO + linha * TAMANHO_CELULA + 10, 18,
                      Gdiplus::Color(255, 255, 255, 255));
        return;
    }
    // Referencia: 40x60 em celulas de 46. Aqui 45x68 em celulas de 52.
    // Os pes ficam ancorados ao chao; a arte pode ultrapassar a casa sem
    // aumentar a caixa de contato ou permitir atravessar paredes.
    int largura = px(45), altura = px(68);
    int x = xCelula + (px(TAMANHO_CELULA) - largura) / 2;
    int y = yCelula + px(TAMANHO_CELULA-3) - altura;
    Gdiplus::SolidBrush sombra(Gdiplus::Color(105,0,0,0));
    g.FillEllipse(&sombra,xCelula+px(9),yCelula+px(41),px(34),px(11));
    Gdiplus::Rect destino(x, y, largura, altura);
    g.DrawImage(folha, destino, quadro * QUADRO_LARGURA, linhaFolha * QUADRO_ALTURA,
               QUADRO_LARGURA, QUADRO_ALTURA, Gdiplus::UnitPixel);
}

// Mesma ideia de desenharQuadroNaCelula, mas pra folhas lineares (1 fileira
// de N quadros quadrados) como bomba_animada.png/explosao_animada.png, em
// vez da grade 3x4 das folhas direcionais de personagem.
void desenharQuadroLinearNaCelula(Gdiplus::Graphics& g, Gdiplus::Image* folha, int linha, int coluna,
                                  int quadro, int tamanhoQuadro, const wchar_t* fallback) {
    int xCelula = px(coluna * TAMANHO_CELULA);
    int yCelula = px(ALTURA_CABECALHO + linha * TAMANHO_CELULA);
    if (!folha) {
        desenharTexto(g, fallback, coluna * TAMANHO_CELULA + 4,
                      ALTURA_CABECALHO + linha * TAMANHO_CELULA + 10, 18,
                      Gdiplus::Color(255, 255, 255, 255));
        return;
    }
    int margem = px(4);
    int lado = px(TAMANHO_CELULA) - margem * 2;
    int x = xCelula + margem;
    int y = yCelula + margem;
    g.DrawImage(folha, Gdiplus::Rect(x, y, lado, lado), quadro * tamanhoQuadro, 0,
               tamanhoQuadro, tamanhoQuadro, Gdiplus::UnitPixel);
}

// Tile de chao/parede/caixa de uma fase: cada um e uma folha + retangulo de
// origem proprios (nao necessariamente a mesma folha - fase 2 usa 3 bitmaps
// pequenos ja recortados, ver prepararTemaFase2Compacto). Coordenadas
// validadas no estudo visual (ver comentario dos globais spriteTemaFaseN);
// sem folha (asset faltando), cai no retangulo colorido antigo.
struct TemaFase {
    Gdiplus::Image* chaoFolha; Gdiplus::Rect chao;
    Gdiplus::Image* paredeFolha; Gdiplus::Rect parede;
    Gdiplus::Image* caixaFolha; Gdiplus::Rect caixa;
};

TemaFase temaDaFase(int fase) {
    fase = max(1,min(TOTAL_FASES,fase));
    if (fase>3) {
        auto* f=spriteArenasEstudo.get();
        int x=INDICES_ATLAS_FASES[fase-1]*46;
        return {f,Gdiplus::Rect(x,0,46,46),f,Gdiplus::Rect(x,46,46,46),f,Gdiplus::Rect(x,92,46,46)};
    }
    if (fase <= 1) {
        Gdiplus::Image* f = spriteTemaFase1.get();
        return {f, Gdiplus::Rect(96, 48, 16, 16), f, Gdiplus::Rect(16, 16, 16, 16),
               f, Gdiplus::Rect(48, 32, 16, 16)};
    }
    if (fase == 2) {
        return {spriteTemaFase2Chao.get(), Gdiplus::Rect(0, 0, 32, 32),
               spriteTemaFase2Parede.get(), Gdiplus::Rect(0, 0, 32, 32),
               spriteTemaFase2Caixa.get(), Gdiplus::Rect(0, 0, 32, 32)};
    }
    Gdiplus::Image* f = spriteTemaFase3.get();
    return {f, Gdiplus::Rect(64, 16, 16, 16), f, Gdiplus::Rect(0, 0, 16, 16),
           f, Gdiplus::Rect(48, 32, 16, 16)};
}

// So o terreno (paredes/chao), sem a sobreposicao de explosao: isso e o que
// vai para o bitmap cacheado, ja que muda raramente (so quando uma bomba
// destroi uma parede ou o jogo reinicia). Coordenadas relativas ao proprio
// cache (sem o deslocamento do cabecalho).
void desenharTerrenoCelula(Gdiplus::Graphics& g, int l, int c) {
    int x = px(c * TAMANHO_CELULA);
    int y = px(l * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    TipoCelula tipo = mapa[l][c];
    TemaFase tema = temaDaFase(faseAtual);

    Gdiplus::Image* folhaTextura = nullptr;
    Gdiplus::Rect origemTextura;
    if (tipo == PAREDE_SOLIDA) { folhaTextura = tema.paredeFolha; origemTextura = tema.parede; }
    else if (tipo == PAREDE_FRAGIL || tipo == PAREDE_BONUS) { folhaTextura = tema.caixaFolha; origemTextura = tema.caixa; }
    else { folhaTextura = tema.chaoFolha; origemTextura = tema.chao; } // VAZIO e PORTAL usam o chao
    bool temTextura = folhaTextura != nullptr;

    if (temTextura) {
        g.DrawImage(folhaTextura, Gdiplus::Rect(x, y, lado, lado), origemTextura.X, origemTextura.Y,
                   origemTextura.Width, origemTextura.Height, Gdiplus::UnitPixel);
    } else {
        Gdiplus::Color cor;
        if (tipo == PAREDE_SOLIDA) cor = Gdiplus::Color(255, 105, 110, 115);
        else if (tipo == PAREDE_FRAGIL || tipo == PAREDE_BONUS) cor = Gdiplus::Color(255, 45, 105, 180);
        else if (tipo == PORTAL) cor = Gdiplus::Color(255, 150, 40, 200);
        else cor = Gdiplus::Color(255, 38, 128, 67);
        Gdiplus::SolidBrush fundo(cor);
        g.FillRectangle(&fundo, x, y, lado, lado);
    }
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
    if (tipo == PAREDE_SOLIDA && !temTextura) {
        Gdiplus::Pen detalhe(Gdiplus::Color(150, 210, 215, 220), max(1, px(1)));
        g.DrawLine(&detalhe, x + px(5), y + lado / 2, x + lado - px(5), y + lado / 2);
    }
    // PAREDE_BONUS fica visualmente identico a PAREDE_FRAGIL de proposito -
    // o item escondido dentro e pra ser surpresa, nao dar pra adivinhar qual
    // caixa tem bonus so de olhar o mapa (pedido explicito do usuario).
    if (tipo == PORTAL) {
        // Aneis concentricos simples pra sugerir um portal, sem precisar de
        // sprite - a celula ja fica cacheada junto do resto do terreno.
        Gdiplus::Pen anel1(Gdiplus::Color(230, 230, 180, 255), max(1, px(3)));
        Gdiplus::Pen anel2(Gdiplus::Color(255, 255, 255, 255), max(1, px(2)));
        g.DrawEllipse(&anel1, x + px(6), y + px(6), lado - px(12), lado - px(12));
        g.DrawEllipse(&anel2, x + px(16), y + px(16), lado - px(32), lado - px(32));
    }
}

// A explosao muda a cada frame (avanca, expande, termina), entao nao pode
// entrar no cache do terreno; e desenhada por cima, mas so nas poucas
// celulas realmente afetadas (no maximo ~25), nao no grid inteiro.
void desenharExplosaoOverlay(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush pincelContorno(Gdiplus::Color(255, 255, 158, 35));
    Gdiplus::Pen grade(Gdiplus::Color(100, 20, 35, 25), max(1, px(1)));
    int lado = px(TAMANHO_CELULA);
    for (const Bomba& b : bombas) {
        if (!b.explodindo) continue;
        // Quadro ligado ao progresso da PROPRIA bomba (nao a um relogio
        // global) - cada explosao mostra o estagio certo independente de
        // quando comecou. tempoMs conta de TEMPO_EXPLOSAO_MS ate 0.
        int quadro = 3 - (b.tempoMs * 4) / TEMPO_EXPLOSAO_MS;
        quadro = max(0, min(3, quadro));
        for (const Posicao& p : b.celulasExplosao) {
            int x = px(p.coluna * TAMANHO_CELULA);
            int y = px(ALTURA_CABECALHO + p.linha * TAMANHO_CELULA);
            if (spriteExplosaoAnimada) {
                float pulso = sinf((1.f - static_cast<float>(b.tempoMs)/TEMPO_EXPLOSAO_MS) * 3.14159f);
                Gdiplus::SolidBrush brilho(Gdiplus::Color(static_cast<BYTE>(55 + pulso*75),255,120,30));
                g.FillEllipse(&brilho, x-px(3), y-px(3), lado+px(6), lado+px(6));
                g.DrawImage(spriteExplosaoAnimada.get(), Gdiplus::Rect(x, y, lado, lado),
                           quadro * 16, 0, 16, 16, Gdiplus::UnitPixel);
            } else {
                g.FillRectangle(&pincelContorno, x, y, lado, lado);
                g.DrawRectangle(&grade, x, y, lado - 1, lado - 1);
            }
        }
    }
}

// Quadros recortados das folhas existentes, animados durante voo e impacto.
Gdiplus::Bitmap* quadroPoderBoss(PoderBoss poder,int quadro,int arma=0){
    if(poder==MACHADO)return arma==1?facoesBoss[quadro%5].get():spriteMachadoBoss.get();
    return poder==ENERGIA?energiaBoss[quadro%4].get():venenoBoss[quadro%4].get();
}
void desenharSpritePoder(Gdiplus::Graphics& g,Gdiplus::Image* sprite,float x,float y,float lado,float angulo=0.f){
    if(!sprite)return;
    auto salvo=g.Save();g.TranslateTransform(px(x),px(y));g.RotateTransform(angulo);
    float w=lado,h=lado;
    if(sprite->GetHeight()>sprite->GetWidth())w*=float(sprite->GetWidth())/sprite->GetHeight();
    else h*=float(sprite->GetHeight())/sprite->GetWidth();
    g.DrawImage(sprite,Gdiplus::RectF(px(-w/2),px(-h/2),px(w),px(h)),0.f,0.f,float(sprite->GetWidth()),float(sprite->GetHeight()),Gdiplus::UnitPixel);
    g.Restore(salvo);
}
void desenharPerigoBossOverlay(Gdiplus::Graphics& g) {
    if(!boss.vivo)return;
    if(boss.investindo){
        float x=(boss.visualC+1.5f)*TAMANHO_CELULA,y=ALTURA_CABECALHO+(boss.visualL+1)*TAMANHO_CELULA;
        float tx=(boss.destino.coluna+1.5f)*TAMANHO_CELULA,ty=ALTURA_CABECALHO+(boss.destino.linha+1)*TAMANHO_CELULA;
        if(boss.avisoInvestidaMs>0){
            Gdiplus::Pen trilha(Gdiplus::Color(180,255,150,75),px(3.f));trilha.SetDashStyle(Gdiplus::DashStyleDash);
            g.DrawLine(&trilha,px(x),px(y),px(tx),px(ty));
            float angulo=atan2(ty-y,tx-x);
            for(float t=.25f;t<=1;t+=.25f){float cx=x+(tx-x)*t,cy=y+(ty-y)*t;
                Gdiplus::Pen seta(Gdiplus::Color(235,255,195,100),px(3.f));
                g.DrawLine(&seta,px(cx),px(cy),px(cx-cos(angulo-.65f)*13),px(cy-sin(angulo-.65f)*13));
                g.DrawLine(&seta,px(cx),px(cy),px(cx-cos(angulo+.65f)*13),px(cy-sin(angulo+.65f)*13));}
        } else {
            Gdiplus::Pen rastro(Gdiplus::Color(95,100,210,255),px(8.f));
            float dl=boss.destino.linha-boss.posicao.linha,dc=boss.destino.coluna-boss.posicao.coluna,n=max(1.f,hypot(dl,dc));
            for(int i=-1;i<=1;++i)g.DrawLine(&rastro,px(x-dc/n*75+i*14),px(y-dl/n*75+i*14),px(x),px(y));
        }
    }
    for(const auto& area:areasBoss){
        float x=(area.posicao.coluna+.5f)*TAMANHO_CELULA,y=ALTURA_CABECALHO+(area.posicao.linha+.5f)*TAMANHO_CELULA;
        int q=(relogioAnimacaoMs/85)%4;
        Gdiplus::SolidBrush poca(Gdiplus::Color(100,70,155,35));
        g.FillEllipse(&poca,px(x-29),px(y-18),px(58.f),px(36.f));
        // Cinco labaredas verdes pulsantes formam uma poca organica, sem casas pintadas.
        for(int i=0;i<5;++i){float angulo=i*1.2566f+relogioAnimacaoMs*.0005f;
            float dx=i==0?0:cos(angulo)*15,dy=i==0?0:sin(angulo)*8;
            desenharSpritePoder(g,venenoBoss[(q+i)%4].get(),x+dx,y+dy-5,32.f+(i==0?8:0));}
    }
    for(const auto& p:projeteisBoss){
        float x=p.c*TAMANHO_CELULA,y=ALTURA_CABECALHO+p.l*TAMANHO_CELULA;
        int q=int(p.idade*14);
        float angulo=p.poder==MACHADO?(p.arma==1?((q%10)>=5?180.f:0.f):p.idade*720):atan2(p.vl,p.vc)*180/3.1415926f;
        if(p.avisoMs>0){
            float pulso=1.f+.12f*sin(relogioAnimacaoMs*.02f);
            desenharSpritePoder(g,quadroPoderBoss(p.poder,0,p.arma),x,y-28,32*pulso,angulo);
            continue;
        }
        Gdiplus::SolidBrush sombra(Gdiplus::Color(75,0,0,0));
        g.FillEllipse(&sombra,px(x-13),px(y+17),px(26.f),px(9.f));
        if(p.poder!=MACHADO){
            for(int i=3;i>=1;--i)desenharSpritePoder(g,quadroPoderBoss(p.poder,(q+i)%4),x-p.vc*i*4,y-p.vl*i*4,18.f-i*2,angulo);
        }
        desenharSpritePoder(g,quadroPoderBoss(p.poder,q,p.arma),x,y,p.poder==MACHADO?46.f:38.f,angulo);
    }
    for(const auto& i:impactosBoss){
        int q=min(3,int(i.idade*10));float lado=26+sin(i.idade/.4f*3.1415926f)*30;
        auto* sprite=i.poder==VENENO?venenoBoss[q].get():i.poder==ENERGIA?energiaBoss[q].get():impactoBoss[q].get();
        desenharSpritePoder(g,sprite,i.c*TAMANHO_CELULA,ALTURA_CABECALHO+i.l*TAMANHO_CELULA,lado,i.idade*180);
    }
    for(const auto& f:fogosBoss){
        int x=int(f.c*TAMANHO_CELULA),y=int(ALTURA_CABECALHO+f.l*TAMANHO_CELULA);
        Gdiplus::SolidBrush brilho(Gdiplus::Color(75,255,125,25));
        g.FillEllipse(&brilho,px(x-24),px(y-24),px(48),px(48));
        if(spriteFogoBoss)g.DrawImage(spriteFogoBoss.get(),Gdiplus::Rect(px(x-16),px(y-29),px(32),px(56)),5+((relogioAnimacaoMs*12/1000)%4)*40,4,31,55,Gdiplus::UnitPixel);
    }
}

// Cor e letra de cada tipo de item - usado tanto no bloco bonus quanto no
// item solto no chao, so pra visual provisorio ate a etapa de polimento
// trocar isso por sprites de verdade (assets/icons_blocos.png).
void infoItem(TipoItem tipo, Gdiplus::Color& cor, const wchar_t*& letra) {
    cor = Gdiplus::Color(255, 200, 200, 200);
    letra = L"?";
    switch (tipo) {
        case CHUTE: cor = Gdiplus::Color(255, 235, 120, 40); letra = L"E"; break;
        case TELEPORTE: cor = Gdiplus::Color(255, 80, 200, 235); letra = L"T"; break;
        case RAIO: cor = Gdiplus::Color(255, 255, 90, 40); letra = L"F"; break;
        case BOMBA_EXTRA: cor = Gdiplus::Color(255, 80, 80, 90); letra = L"B"; break;
        case VIDA_EXTRA: cor = Gdiplus::Color(255, 255, 70, 130); letra = L"V"; break;
        case BOMBA_RELOGIO: cor = Gdiplus::Color(255, 220, 220, 225); letra = L"R"; break;
        case IMUNE_BOMBA: cor = Gdiplus::Color(255, 120, 220, 255); letra = L"I"; break;
        case ATRAVESSA_BLOCO: cor = Gdiplus::Color(255, 190, 130, 255); letra = L"A"; break;
    }
}

void desenharItem(Gdiplus::Graphics& g, const Item& item) {
    int x = px(item.posicao.coluna * TAMANHO_CELULA);
    int y = px(ALTURA_CABECALHO + item.posicao.linha * TAMANHO_CELULA);
    int lado = px(TAMANHO_CELULA);
    Gdiplus::Color cor;
    const wchar_t* letra;
    infoItem(item.tipo, cor, letra);
    if (spriteItens) {
        float oscilacao = sinf(relogioAnimacaoMs * .005f + item.posicao.coluna) * px(2.f);
        Gdiplus::SolidBrush sombra(Gdiplus::Color(100,0,0,0));
        g.FillEllipse(&sombra,x+px(8),y+px(37),px(36),px(10));
        Gdiplus::Pen borda(cor,px(2));
        g.DrawRectangle(&borda,x+px(7),y+px(7),lado-px(14),lado-px(14));
        g.DrawImage(spriteItens.get(),Gdiplus::RectF(x+px(10.f),y+px(10.f)+oscilacao,px(32.f),px(32.f)),
                    static_cast<int>(item.tipo)*32.f,0.f,32.f,32.f,Gdiplus::UnitPixel);
        return;
    }
    Gdiplus::SolidBrush pincel(cor);
    int margem = px(8);
    g.FillEllipse(&pincel, x + margem, y + margem, lado - 2 * margem, lado - 2 * margem);
    desenharTexto(g, letra, item.posicao.coluna * TAMANHO_CELULA + 16,
                  ALTURA_CABECALHO + item.posicao.linha * TAMANHO_CELULA + 12, 18,
                  Gdiplus::Color(255, 20, 20, 20));
}

void desenharJogadorAtual(Gdiplus::Graphics& g, int indice) {
    float l=indice==0 ? jogadorVisualL : jogador2VisualL;
    float c=indice==0 ? jogadorVisualC : jogador2VisualC;
    int skin=indice==0 ? personagemP1 : personagemP2;
    bool morto=(estado==DERROTA || estado==FIM_DE_JOGO) && (jogadoresAtingidos & (1<<indice));
    if (morto && posesDerrota[skin]) {
        auto* folha=posesDerrota[skin].get();
        int quadro=min(static_cast<int>(folha->GetWidth())/16-1,tempoFimMs/130);
        float altura=folha->GetHeight()*(68.f/24.f), largura=16*(68.f/24.f);
        float x=(c+.5f)*TAMANHO_CELULA-largura/2;
        float y=ALTURA_CABECALHO+(l+1)*TAMANHO_CELULA-3-altura;
        g.DrawImage(folha,Gdiplus::RectF(px(x),px(y),px(largura),px(altura)),
                    quadro*16.f,0.f,16.f,static_cast<float>(folha->GetHeight()),Gdiplus::UnitPixel);
        return;
    }
    desenharQuadroNaCelula(g,personagens[skin].get(),l,c,
        linhaDaFolhaParaDirecao(indice==0 ? direcaoJogador : direcaoJogador2),
        indice==0 ? quadroAnimJogador : quadroAnimJogador2,indice==0 ? L"P1" : L"P2");
}

void desenharMarioAliado(Gdiplus::Graphics& g){
    const auto& m=marioAliado;if(!m.ativo || m.hp<=0)return;
    const int ciclo[]={0,1,0,2};int q=m.andando?ciclo[int(m.distancia*2.5f)%4]:1;
    int x=int((m.c+.5f)*TAMANHO_CELULA),y=int(ALTURA_CABECALHO+(m.l+1)*TAMANHO_CELULA-3);
    Gdiplus::SolidBrush sombra(Gdiplus::Color(80,0,0,0));g.FillEllipse(&sombra,px(x-19),px(y-5),px(38),px(12));
    if(m.imunidade>0){Gdiplus::Pen escudo(Gdiplus::Color(110,255,145,105),px(2.f));g.DrawEllipse(&escudo,px(x-27),px(y-60),px(54),px(62));}
    desenharRecorte(g,quadrosMario[m.direcao][q].get(),0,0,16,16,x-27,y-53,54);
    desenharTexto(g,L"MARIO",x-24,y-69,10,Gdiplus::Color(255,255,145,110));
    if(m.pose>0)desenharRecorte(g,spriteMacaMario.get(),0,0,12,14,x+12,y-45,21);
}
void desenharMacasMario(Gdiplus::Graphics& g){
    for(Posicao p:macasNoChao)desenharRecorte(g,spriteMacaMario.get(),0,0,12,14,int((p.coluna+.5f)*TAMANHO_CELULA)-12,ALTURA_CABECALHO+int((p.linha+.5f)*TAMANHO_CELULA)-14,28);
    for(const auto& a:macasMario){float u=clamp(a.idade/a.duracao,0.f,1.f);
        float x=(a.sx+(a.tx-a.sx)*u)*TAMANHO_CELULA,chao=ALTURA_CABECALHO+(a.sy+(a.ty-a.sy)*u)*TAMANHO_CELULA;
        float altura=sin(u*3.1415926f)*TAMANHO_CELULA*1.7f+(1-u)*20;
        Gdiplus::SolidBrush sombra(Gdiplus::Color(80,0,0,0));g.FillEllipse(&sombra,px(x-9),px(chao-3),px(18.f),px(8.f));
        auto salvo=g.Save();g.TranslateTransform(px(x),px(chao-altura));g.RotateTransform(u*400);
        if(spriteMacaMario)g.DrawImage(spriteMacaMario.get(),Gdiplus::RectF(px(-12.f),px(-14.f),px(24.f),px(28.f)),0.f,0.f,12.f,14.f,Gdiplus::UnitPixel);
        g.Restore(salvo);
        Gdiplus::Pen alvo(Gdiplus::Color(85,255,120,90),px(1.f));g.DrawEllipse(&alvo,px(a.tx*TAMANHO_CELULA-12),px(ALTURA_CABECALHO+a.ty*TAMANHO_CELULA-5),px(24.f),px(10.f));
    }
}
void desenharMapa(Gdiplus::Graphics& g) {
    // O terreno (195 celulas) so muda quando uma bomba destroi parede ou o
    // jogo reinicia (cacheMapaSujo). Nos demais frames (a grande maioria)
    // reaproveita o bitmap ja pronto: 1 DrawImage em vez de ~400 chamadas
    // GDI+ por frame, que era o gargalo real de CPU/lentidao.
    int larguraGrid = px(LARGURA_MAPA);
    int alturaGrid = px(LINHAS * TAMANHO_CELULA);
    if (cacheMapaSujo || !cacheMapa || static_cast<int>(cacheMapa->GetWidth()) != larguraGrid ||
        static_cast<int>(cacheMapa->GetHeight()) != alturaGrid) {
        cacheMapa = make_unique<Gdiplus::Bitmap>(larguraGrid, alturaGrid, PixelFormat32bppPARGB);
        Gdiplus::Graphics gCache(cacheMapa.get());
        gCache.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        gCache.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        gCache.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        for (int l = 0; l < LINHAS; l++)
            for (int c = 0; c < COLUNAS; c++) desenharTerrenoCelula(gCache, l, c);
        cacheMapaSujo = false;
    }
    g.DrawImage(cacheMapa.get(), 0, px(ALTURA_CABECALHO));

    desenharExplosaoOverlay(g);
    for (const Item& item : itensNoChao) desenharItem(g, item);
    for (const Bomba& b : bombas) {
        if (!b.ativa || b.explodindo) continue;
        float x = px(b.posicao.coluna * TAMANHO_CELULA + TAMANHO_CELULA / 2);
        float y = px(ALTURA_CABECALHO + b.posicao.linha * TAMANHO_CELULA + TAMANHO_CELULA / 2);
        float pulso = 1 + sinf(relogioAnimacaoMs * (b.tempoMs < 800 ? .030f : .010f)) * .08f;
        float lado = px(36) * pulso;
        Gdiplus::SolidBrush sombra(Gdiplus::Color(110, 0, 0, 0));
        g.FillEllipse(&sombra, x - px(17.f), y + px(9.f), px(34.f), px(12.f));
        if (spriteBombaAnimada)
            g.DrawImage(spriteBombaAnimada.get(), Gdiplus::RectF(x-lado/2, y-lado/2, lado, lado),
                        ((relogioAnimacaoMs / 125) % 4) * 16.f, 0.f, 16.f, 16.f, Gdiplus::UnitPixel);
        else desenharQuadroLinearNaCelula(g, nullptr, b.posicao.linha, b.posicao.coluna, 0, 16, L"BO");
        Gdiplus::Pen arco(b.tempoMs < 800 && !b.relogio ? Gdiplus::Color(255,255,105,70) : Gdiplus::Color(255,98,235,199), px(2));
        float progresso = b.relogio ? 1.f : static_cast<float>(b.tempoMs) / TEMPO_BOMBA_MS;
        g.DrawArc(&arco, x-px(23.f), y-px(23.f), px(46.f), px(46.f), -90.f, 360.f*progresso);
    }
    struct AtorDesenho { float linha; int tipo, indice; };
    vector<AtorDesenho> ordem={{jogadorVisualL,0,0}};
    if(segundoJogadorAtivo()) ordem.push_back({jogador2VisualL,0,1});
    for(int i=0;i<static_cast<int>(inimigos.size());++i)
        if(inimigos[i].vivo) ordem.push_back({inimigos[i].visualL,1,i});
    if(marioAliado.ativo && marioAliado.hp>0)ordem.push_back({marioAliado.l,3,0});
    if(boss.vivo) ordem.push_back({boss.visualL+1,2,0});
    stable_sort(ordem.begin(),ordem.end(),[](const AtorDesenho& a,const AtorDesenho& b){return a.linha<b.linha;});
    for(const AtorDesenho& ator:ordem) {
        if(ator.tipo==0) desenharJogadorAtual(g,ator.indice);
        else if(ator.tipo==3)desenharMarioAliado(g);
        else if(ator.tipo==1) {
            const Inimigo& i=inimigos[ator.indice];
            desenharQuadroNaCelula(g,spriteInimigoDirecional.get(),i.visualL,i.visualC,
                linhaDaFolhaParaDirecao(i.direcao),i.quadroAnim,i.especial ? L"IN*" : L"IN");
        } else {
            int x=static_cast<int>((boss.visualC+1.5f)*TAMANHO_CELULA-55);
            int y=static_cast<int>(ALTURA_CABECALHO+(boss.visualL+.6f)*TAMANHO_CELULA-70+sin(relogioAnimacaoMs*.003f)*5);
            if(boss.animAtaqueMs>0 && indiceBoss()!=3)y-=int(sin((700-boss.animAtaqueMs)/700.f*3.1415926f)*9);
            if(boss.avisoInvestidaMs>0)desenharTexto(g,L"INVESTIDA!",x-2,y-18,12,Gdiplus::Color(255,255,185,80));
            Gdiplus::SolidBrush sombra(Gdiplus::Color(80,0,0,0));
            g.FillEllipse(&sombra,px(x+15),px(y+98),px(80),px(22));
            desenharRecorte(g,imagemBossAtual(),0,0,imagemBossAtual()?imagemBossAtual()->GetWidth():1,imagemBossAtual()?imagemBossAtual()->GetHeight():1,x,y,110);
            if(boss.imunidadeMs>200){Gdiplus::Pen dano(Gdiplus::Color(230,255,155,65),px(3.f));g.DrawEllipse(&dano,px(x),px(y),px(110),px(110));}
        }
    }
    desenharPerigoBossOverlay(g);
    desenharMacasMario(g);
}

void desenharHUD(Gdiplus::Graphics& g) {
    painel(g, 0, 0, LARGURA_JOGO, ALTURA_CABECALHO, Gdiplus::Color(255,11,16,28));
    painel(g, 0, 0, LARGURA_JOGO, 5, Gdiplus::Color(255,252,187,65));
    desenharTexto(g, L"BOMBER / UNIVALI", 22, 14, 30, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"FASE " + to_wstring(faseAtual) + L" / " + to_wstring(TOTAL_FASES) + L"   -   " + NOMES_FASES[faseAtual-1], 24, 58, 14,
                  Gdiplus::Color(255,110,157,186));
    desenharTexto(g, modo == AUTOMATICO ? L"COMPUTADOR JOGANDO" : doisJogadores ? L"COOPERATIVO LOCAL" : L"VOCE NO CONTROLE",
                  800, 22, 16, Gdiplus::Color(255,98,235,199));
    desenharTexto(g, (dificuldade == FACIL ? L"FACIL" : dificuldade == MEDIO ? L"MEDIO" : L"DIFICIL") +
                  wstring(L"   /   ") + to_wstring(tempoPartidaMs/1000) + L"s", 800, 53, 14,
                  Gdiplus::Color(255,144,164,190));
}

void desenharPainelLateral(Gdiplus::Graphics& g) {
    painel(g, 794, ALTURA_CABECALHO, 272, LINHAS*TAMANHO_CELULA);
    desenharTexto(g, L"PONTUACAO", 812, 121, 13, Gdiplus::Color(255,144,164,190));
    desenharTexto(g, to_wstring(pontos), 810, 147, 38, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"VIDAS  " + to_wstring(vidas) + L"    INIMIGOS  " + to_wstring(inimigosVivos()),
                  812, 207, 14, Gdiplus::Color(255,98,235,199));
    desenharTexto(g, L"FOGO  " + to_wstring(alcanceExplosao) + L"    BOMBAS  " + to_wstring(bombas.size()) +
                  L" / " + to_wstring(limiteBombas()), 812, 239, 14, Gdiplus::Color(255,252,187,65));
    painel(g, 812, 278, 236, 1, Gdiplus::Color(255,55,68,86));
    desenharTexto(g, boss.vivo ? NOMES_BOSSES[indiceBoss()] : L"OBJETIVO DA FASE", 812, 298, 14,
                  Gdiplus::Color(255,252,187,65));
    if (boss.vivo) {
        desenharTexto(g,PODERES_BOSSES[indiceBoss()],812,568,12,Gdiplus::Color(255,255,190,100));
        desenharRecorte(g, imagemBossAtual(), 0, 0, imagemBossAtual()?imagemBossAtual()->GetWidth():1, imagemBossAtual()?imagemBossAtual()->GetHeight():1, 842, 330, 172);
        painel(g, 812, 510, 236, 10, Gdiplus::Color(255,60,25,35));
        painel(g, 812, 510, 236*boss.vida/vidaMaximaBoss(), 10, Gdiplus::Color(255,255,120,75));
        desenharTexto(g, L"VIDA DO CHEFE  " + to_wstring(boss.vida) + L" / " + to_wstring(vidaMaximaBoss()), 812, 532, 13,
                      Gdiplus::Color(255,245,220,185));
    } else {
        desenharTexto(g, portalAtivo ? L"Portal aberto! Entre para avancar." : L"Elimine os inimigos para abrir o portal.",
                      812, 334, 16, Gdiplus::Color(255,220,230,240));
        desenharTexto(g, L"Destrua caixas e descubra os itens escondidos.", 812, 403, 14,
                      Gdiplus::Color(255,144,164,190));
    }
    if(marioAliado.ativo)desenharTexto(g,L"MARIO ALIADO  /  VIDA "+to_wstring(marioAliado.hp)+L" / 3",812,747,12,Gdiplus::Color(255,255,145,110));
    desenharTexto(g, L"SUA PARTIDA", 812, 581, 13, Gdiplus::Color(255,98,235,199));
    desenharTexto(g, L"Movimentos   " + to_wstring(estatisticas.movimentos), 812, 610, 14, Gdiplus::Color(255,220,230,240));
    desenharTexto(g, L"Bombas usadas   " + to_wstring(estatisticas.bombasUsadas), 812, 639, 14, Gdiplus::Color(255,220,230,240));
    desenharTexto(g, L"Itens coletados   " + to_wstring(estatisticas.itensColetados), 812, 668, 14, Gdiplus::Color(255,220,230,240));
    wstring itens;
    if (possuiCartaChute) itens += L"E: chute  ";
    if (possuiCartaTeleporte) itens += L"T: teleporte  ";
    if (temBombaRelogio) itens += L"F: detonar  ";
    if (imuneAPropriaBomba) itens += L"Escudo  ";
    if (atravessaBlocoQuebravel) itens += L"Atravessar caixas";
    desenharTexto(g, itens, 812, 710, 12, Gdiplus::Color(255,252,187,65));
}

void desenharRodape(Gdiplus::Graphics& g) {
    int y = ALTURA_CABECALHO + LINHAS*TAMANHO_CELULA;
    painel(g, 0, y, LARGURA_JOGO, ALTURA_RODAPE, Gdiplus::Color(255,11,16,28));
    desenharTexto(g, doisJogadores && modo == MANUAL
                  ? L"P1  WASD + ESPACO      P2  SETAS + ENTER"
                  : L"WASD / SETAS  mover       ESPACO  bomba", 24, y+12, 14, Gdiplus::Color(255,220,230,240));
    desenharTexto(g, L"E  chute     T  teleporte     F  detonar     R  reiniciar     M  menu     ESC  sair",
                  24, y+39, 12, Gdiplus::Color(255,144,164,190));
}

void desenharFimPartida(Gdiplus::Graphics& g) {
    if (estado == JOGANDO) return;
    if (estado!=VITORIA && tempoFimMs<DURACAO_MORTE_MS) return;
    painel(g, 0, 0, LARGURA_JOGO, ALTURA_JOGO, Gdiplus::Color(185,5,9,18));
    painel(g, 230, 180, 620, 490, Gdiplus::Color(255,25,34,50));
    painel(g, 230, 180, 620, 4, Gdiplus::Color(255,252,187,65));
    bool venceu = estado == VITORIA;
    auto poseFinal = [&](int personagem, int x, int lado) {
        Gdiplus::Image* folha = personagem == 0 ? (venceu ? spriteVitoria.get() : spriteDerrota.get())
                                             : (venceu ? posesVitoria[personagem].get() : posesDerrota[personagem].get());
        if (!folha) return;
        int total = static_cast<int>(folha->GetWidth()) / 16;
        int quadro = venceu ? (tempoFimMs/180)%total : min(total-1,tempoFimMs/130);
        if (venceu && personagem == 0)
            quadro = tempoFimMs < 720 ? tempoFimMs/180 : 4+((tempoFimMs-720)/180)%5;
        desenharRecorte(g,folha,quadro*16,0,16,folha->GetHeight(),x,220,lado);
    };
    if (doisJogadores && modo == MANUAL) { poseFinal(personagemP1,365,164); poseFinal(personagemP2,550,164); }
    else poseFinal(personagemP1,448,184);
    desenharTexto(g, venceu ? L"VOCE VENCEU!" : jogadoresAtingidos==4 ? L"MARIO FOI DERROTADO" : estado == FIM_DE_JOGO ? L"GAME OVER" : L"MAIS UMA CHANCE",
                  240, 423, 34, venceu ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,255,140,110), true);
    desenharTexto(g, venceu ? L"Campanha concluida!" : L"Vidas restantes: " + to_wstring(vidas),
                  240, 480, 18, Gdiplus::Color(255,220,230,240), true);
    desenharTexto(g, L"PONTOS  " + to_wstring(pontos), 240, 523, 22, Gdiplus::Color(255,252,187,65), true);
    desenharTexto(g, estado == DERROTA ? L"R  tentar a fase novamente     M  menu" : L"R  nova partida     M  menu",
                  240, 602, 16, Gdiplus::Color(255,144,164,190), true);
}

void desenharMenu(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"ALGORITMOS E PROGRAMACAO II / UNIVALI", 44, 35, 13, Gdiplus::Color(255,110,157,186));
    desenharTexto(g, L"BOMBERMAN", 40, 70, 46, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"Oito arenas. Escolha onde comecar sua campanha.", 44, 135, 17, Gdiplus::Color(255,98,235,199));
    painel(g, 44, 192, 470, 75);
    desenharTexto(g, L"SEU NOME NO RANKING", 60, 203, 11, Gdiplus::Color(255,144,164,190));
    desenharTexto(g, (nomeJogador.empty() ? L"Digite seu nome" : nomeJogador) + wstring((relogioAnimacaoMs/500)%2 ? L" |" : L""),
                  60, 228, 20, Gdiplus::Color(255,246,247,251));
    const wchar_t* titulos[] = {L"1   JOGAR", L"2   MODO AUTOMATICO", L"3   COMO JOGAR", L"4   DIFICULDADES", L"5   PONTUACAO", L"6   RANKING"};
    for (int i=0; i<6; ++i) {
        int y=287+i*57;
        painel(g, 44, y, 470, 47);
        painel(g, 44, y, 3, 47, i==0 ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,110,157,186));
        desenharTexto(g, titulos[i], 62, y+10, 18, i==0 ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,220,230,240));
    }
    painel(g, 550, 192, 486, 348, Gdiplus::Color(255,18,27,43));
    // Montagem com os mesmos tiles e sprites usados na partida.
    TemaFase tema = temaDaFase(1);
    for(int l=0;l<7;++l) for(int c=0;c<8;++c) {
        bool parede=l==0 || l==6 || c==0 || c==7 || (l%2==0 && c%2==0);
        bool caixa=!parede && (l+c)%4==0;
        Gdiplus::Image* folha=parede ? tema.paredeFolha : caixa ? tema.caixaFolha : tema.chaoFolha;
        Gdiplus::Rect r=parede ? tema.parede : caixa ? tema.caixa : tema.chao;
        if(folha) g.DrawImage(folha, Gdiplus::Rect(px(618+c*44),px(208+l*44),px(44),px(44)),r.X,r.Y,r.Width,r.Height,Gdiplus::UnitPixel);
    }
    int q=(relogioAnimacaoMs/180)%3;
    desenharRecorte(g, personagens[personagemP1].get(), q*16,0,16,24,638,280,112);
    desenharRecorte(g, spriteInimigoDirecional.get(), q*16,0,16,24,844,390,84);
    desenharRecorte(g, spriteBombaAnimada.get(), ((relogioAnimacaoMs/125)%4)*16,0,16,16,770,350,64);
    desenharTexto(g, L"EXPLORE. DESCUBRA. SOBREVIVA.", 570, 551, 20, Gdiplus::Color(255,252,187,65));
    painel(g,550,604,486,55,Gdiplus::Color(255,59,28,42));
    desenharTexto(g,L"9   MODO INSANO",570,611,22,Gdiplus::Color(255,255,135,105));
    desenharTexto(g,L"O jogo completo original da batalha de sprites",570,638,12,Gdiplus::Color(255,220,230,240));
    painel(g,550,666,486,61);
    desenharTexto(g,L"8   SELETOR DE FASES",570,677,18,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"Oito arenas para explorar e comparar",570,704,12,Gdiplus::Color(255,144,164,190));
    painel(g,550,738,486,58);
    desenharTexto(g,L"7   ESCOLHER PERSONAGENS",570,746,18,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"P1: "+wstring(nomesPersonagens[personagemP1])+L"   /   P2: "+nomesPersonagens[personagemP2],570,775,12,Gdiplus::Color(255,144,164,190));
    desenharTexto(g, doisJogadores ? L"P   COOPERATIVO LIGADO" : L"P   ATIVAR SEGUNDO JOGADOR", 44, 664, 16, Gdiplus::Color(255,98,235,199));
    desenharTexto(g, L"P1: WASD + Espaco   /   P2: Setas + Enter", 44, 695, 13, Gdiplus::Color(255,144,164,190));
    desenharTexto(g, L"Escolha 1 ou 2 para selecionar a dificuldade.", 44, 754, 14, Gdiplus::Color(255,144,164,190));
    desenharTexto(g, L"Q / ESC   sair", 44, 796, 14, Gdiplus::Color(255,220,230,240));
}

void desenharPersonagens(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g,L"ESCOLHA SEU PERSONAGEM",44,48,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,L"Selecionando: JOGADOR "+to_wstring(jogadorEmSelecao),44,110,20,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"TAB troca entre P1 e P2. As quatro opcoes tem as mesmas habilidades.",44,151,15,Gdiplus::Color(255,144,164,190));
    int selecionado=jogadorEmSelecao==1 ? personagemP1 : personagemP2;
    for(int i=0;i<4;++i) {
        int x=44+i*254;
        painel(g,x,230,232,330);
        Gdiplus::Color cor=i==selecionado ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,55,68,86);
        painel(g,x,230,232,4,cor);
        desenharTexto(g,to_wstring(i+1)+L"   "+nomesPersonagens[i],x+18,259,20,Gdiplus::Color(255,220,230,240));
        desenharRecorte(g,personagens[i].get(),((relogioAnimacaoMs/180)%3)*16,0,16,24,x+40,310,152);
        desenharTexto(g,i==selecionado ? L"SELECIONADO" : L"ESCOLHER",x+38,505,15,cor);
    }
    desenharTexto(g,L"P1: "+wstring(nomesPersonagens[personagemP1])+L"     /     P2: "+nomesPersonagens[personagemP2],44,626,20,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"1 / 2 / 3 / 4  escolher     TAB  trocar jogador     ENTER  escolher fase     ESC  menu",44,790,15,Gdiplus::Color(255,144,164,190));
}

void desenharSeletorFases(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g,L"ESCOLHA SUA ARENA",44,36,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,L"A campanha continua da fase escolhida ate o Templo Arcano, onde esta o chefe final.",44,91,16,Gdiplus::Color(255,144,164,190));
    for(int i=0;i<TOTAL_FASES;++i) {
        int x=44+(i%4)*254,y=150+(i/4)*254;
        painel(g,x,y,232,232);
        bool selecionada=faseSelecionada==i+1;
        painel(g,x,y,232,3,selecionada ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,55,68,86));
        TemaFase t=temaDaFase(i+1);
        for(int l=0;l<5;++l) for(int c=0;c<7;++c) {
            bool parede=l==0 || l==4 || c==0 || c==6 || (l==2 && c%2==0);
            bool caixa=!parede && (l+c)%3==0;
            auto* folha=parede ? t.paredeFolha : caixa ? t.caixaFolha : t.chaoFolha;
            Gdiplus::Rect r=parede ? t.parede : caixa ? t.caixa : t.chao;
            if(folha) g.DrawImage(folha,Gdiplus::Rect(px(x+11+c*30),px(y+17+l*30),px(30),px(30)),r.X,r.Y,r.Width,r.Height,Gdiplus::UnitPixel);
        }
        desenharTexto(g,to_wstring(i+1)+L"  "+NOMES_FASES[i],x+11,y+180,16,Gdiplus::Color(255,220,230,240));
        desenharTexto(g,selecionada ? L"SELECIONADA" : i<3 ? L"ARENA ORIGINAL" : L"ARENA DO ESTUDO",x+11,y+207,11,
                      selecionada ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,144,164,190));
    }
    painel(g,44,683,992,76);
    desenharTexto(g,L"INICIAR NA FASE "+to_wstring(faseSelecionada)+L"  /  "+NOMES_FASES[faseSelecionada-1],62,701,22,Gdiplus::Color(255,252,187,65));
    if(faseSelecionada%2==0)desenharTexto(g,NOMES_BOSSES[faseSelecionada/2-1],62,734,15,Gdiplus::Color(255,255,135,85));
    desenharTexto(g,L"1-8 / SETAS  selecionar     ENTER  iniciar     M  personagens     ESC  menu",44,790,15,Gdiplus::Color(255,144,164,190));
}

void desenharRanking(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"RANKING", 20, 40, 32, Gdiplus::Color(255, 65, 210, 235), true);
    vector<RegistroPontuacao> registros = lerRanking();
    if (registros.empty()) {
        desenharTexto(g, L"Nenhuma partida manual salva ainda.", 20, 110, 18,
                      Gdiplus::Color(255, 205, 210, 215));
    } else {
        int y = 110;
        int posicao = 1;
        for (const RegistroPontuacao& registro : registros) {
            if (posicao > 10) break;
            wstring jogadorW(registro.jogador.begin(), registro.jogador.end());
            wstring linha = to_wstring(posicao) + L". " + jogadorW + L" - " +
                             to_wstring(registro.pontuacao) + L" pts  (" +
                             to_wstring(registro.movimentos) + L" mov, " +
                             to_wstring(registro.bombas) + L" bombas) - " +
                             wstring(registro.data.begin(), registro.data.end());
            desenharTexto(g, linha, 20, static_cast<float>(y), 15,
                          Gdiplus::Color(255, 235, 235, 240));
            y += 28;
            posicao++;
        }
    }
    desenharTexto(g, L"[M ou ESC] Voltar ao menu", 20, ALTURA_JOGO - 40.0f, 15,
                  Gdiplus::Color(255, 160, 165, 170), true);
}

void desenharEscolhaDificuldade(Gdiplus::Graphics& g) {
    fundoInterface(g);
    bool informativa = telaMenu == MENU_DIFICULDADES_INFO;
    desenharTexto(g, informativa ? L"CONHECA O DESAFIO" : L"ESCOLHA SEU DESAFIO", 44, 48, 34, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"O mesmo mapa. Tres formas de enfrentar a arena.", 44, 106, 17, Gdiplus::Color(255,110,157,186));
    const wchar_t* nomes[] = {L"1   FACIL", L"2   MEDIO", L"3   DIFICIL"};
    const wchar_t* ritmos[] = {L"TEMPO PARA EXPLORAR", L"FIQUE ATENTO", L"CADA PASSO CONTA"};
    Gdiplus::Color cores[] = {Gdiplus::Color(255,98,235,199),Gdiplus::Color(255,252,187,65),Gdiplus::Color(255,255,120,100)};
    for(int i=0;i<3;++i) {
        int x=44+i*338;
        painel(g,x,182,316,380);
        painel(g,x,182,316,4,cores[i]);
        desenharTexto(g,nomes[i],x+18,210,26,cores[i]);
        desenharTexto(g,ritmos[i],x+18,259,13,Gdiplus::Color(255,144,164,190));
        desenharRecorte(g,spriteInimigoDirecional.get(),((relogioAnimacaoMs/180)%3)*16,0,16,24,x+109,299,96);
        desenharTexto(g,to_wstring(i==0 ? 3 : i==1 ? 5 : 7)+L" inimigos por fase",x+18,414,17,Gdiplus::Color(255,220,230,240));
        desenharTexto(g,i==0 ? L"Perseguicao ate 5 celulas" : L"Perseguicao a qualquer distancia",x+18,457,14,Gdiplus::Color(255,144,164,190));
        desenharTexto(g,to_wstring(i==0 ? 25 : i==1 ? 50 : 75)+L"% de chance de perseguir",x+18,490,14,cores[i]);
    }
    desenharTexto(g,L"8 FASES   /   PORTAIS   /   ITENS ESCONDIDOS",44,610,22,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"Na ultima fase: inimigo especial e o chefe final com 5 pontos de vida.",44,658,17,Gdiplus::Color(255,220,230,240));
    desenharTexto(g,informativa ? L"M / ESC   voltar ao menu" : L"1 / 2 / 3   iniciar partida       M / ESC   voltar",44,790,16,Gdiplus::Color(255,144,164,190));
}

void desenharComoJogar(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"COMO JOGAR", 20, 30, 28, Gdiplus::Color(255, 65, 210, 235), true);
    struct Linha { const wchar_t* texto; int y; bool titulo; };
    const Linha linhas[] = {
        {L"Objetivo: limpe os inimigos das 8 fases da campanha, pise no", 80, false},
        {L"portal que aparece; fases 2, 4, 6 e 8 tem chefes.", 98, false},
        {L"", 118, false},
        {L"Controles", 130, true},
        {L"WASD ou setas: mover   Espaco: colocar bomba", 156, false},
        {L"E: chutar bomba (precisa do item)   T: teleportar (precisa do item)", 174, false},
        {L"F: detonar bombas-relogio (precisa do item)", 192, false},
        {L"R: reiniciar a fase atual   M: voltar ao menu   Q/ESC: sair", 210, false},
        {L"", 230, false},
        {L"Bombas e explosao", 242, true},
        {L"A bomba explode sozinha apos alguns segundos. A chama se espalha", 268, false},
        {L"em cruz e atravessa quantos blocos frageis o nivel do fogo permitir,", 286, false},
        {L"destruindo cada um deles no caminho (ate parar numa parede solida).", 304, false},
        {L"", 324, false},
        {L"Vidas", 336, true},
        {L"Voce comeca cada fase com um numero base de vidas. Morrer custa 1", 362, false},
        {L"vida e reinicia a fase atual; zerar as vidas termina a partida.", 380, false},
    };
    for (const Linha& l : linhas) {
        if (l.texto[0] == L'\0') continue;
        desenharTexto(g, l.texto, 20, static_cast<float>(l.y), l.titulo ? 18.0f : 14.0f,
                      l.titulo ? Gdiplus::Color(255, 255, 210, 85) : Gdiplus::Color(255, 220, 225, 230),
                      l.titulo);
    }
    desenharTexto(g, L"[M ou ESC] Voltar", 20, ALTURA_JOGO - 40.0f, 15,
                  Gdiplus::Color(255, 160, 165, 170), true);
}

void desenharPontuacaoInfo(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"PONTUACAO", 20, 30, 28, Gdiplus::Color(255, 65, 210, 235), true);
    desenharTexto(g, L"A pontuacao nunca fica negativa (fica parada em 0).", 20, 74, 14,
                  Gdiplus::Color(255, 205, 210, 215));
    struct Linha { wstring texto; bool positivo; };
    const Linha linhas[] = {
        {L"+ " + to_wstring(PONTOS_POR_INIMIGO) + L" por inimigo abatido", true},
        {L"+ " + to_wstring(PONTOS_POR_CAIXA) + L" por caixa (bloco fragil) destruida", true},
        {L"+ " + to_wstring(PONTOS_POR_ITEM) + L" por item coletado", true},
        {L"+ " + to_wstring(PONTOS_POR_FASE) + L" por fase concluida (pisar no portal)", true},
        {L"+ " + to_wstring(PONTOS_POR_BOSS) + L" de bonus por derrotar o boss (fase final)", true},
        {L"- " + to_wstring(CUSTO_POR_MOVIMENTO) + L" por movimento", false},
        {L"- " + to_wstring(CUSTO_POR_BOMBA) + L" por bomba usada", false},
    };
    int y = 110;
    for (const Linha& l : linhas) {
        desenharTexto(g, l.texto, 20, static_cast<float>(y), 17,
                      l.positivo ? Gdiplus::Color(255, 120, 230, 140) : Gdiplus::Color(255, 240, 120, 120),
                      true);
        y += 32;
    }
    desenharTexto(g, L"Esses mesmos valores sao usados de verdade no calculo", 20,
                  static_cast<float>(y + 10), 13, Gdiplus::Color(255, 180, 225, 255));
    desenharTexto(g, L"durante a partida e no que fica salvo no ranking.", 20,
                  static_cast<float>(y + 28), 13, Gdiplus::Color(255, 180, 225, 255));
    desenharTexto(g, L"[M ou ESC] Voltar", 20, ALTURA_JOGO - 40.0f, 15,
                  Gdiplus::Color(255, 160, 165, 170), true);
}

void ajustarAreaDesenho(int largura, int altura) {
    if(largura<=0 || altura<=0) return;
    float escala = min(static_cast<float>(largura)/LARGURA_JOGO,static_cast<float>(altura)/ALTURA_JOGO);
    if(abs(escala-escalaDpi)>.0001f) {
        escalaDpi=escala;
        cacheFontes.clear();
        invalidarCacheMapa();
        cacheSpriteBoss.reset();
    }
    margemJanelaX=(largura-px(LARGURA_JOGO))/2;
    margemJanelaY=(altura-px(ALTURA_JOGO))/2;
}

void desenhar(HWND janela) {
    PAINTSTRUCT ps;
    HDC destino = BeginPaint(janela, &ps);
    RECT cliente;
    GetClientRect(janela, &cliente);
    ajustarAreaDesenho(cliente.right,cliente.bottom);
    garantirBuffer(destino, cliente.right, cliente.bottom);
    Gdiplus::Graphics g(bufferDC);
    g.TranslateTransform(static_cast<float>(margemJanelaX),static_cast<float>(margemJanelaY));
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    fundoInterface(g);
    if (modo == MENU) {
        switch (telaMenu) {
            case MENU_FASES: desenharSeletorFases(g); break;
            case MENU_PERSONAGENS: desenharPersonagens(g); break;
            case MENU_RANKING: desenharRanking(g); break;
            case MENU_DIFICULDADE: desenharEscolhaDificuldade(g); break;
            case MENU_DIFICULDADES_INFO: desenharEscolhaDificuldade(g); break;
            case MENU_COMO_JOGAR: desenharComoJogar(g); break;
            case MENU_PONTUACAO: desenharPontuacaoInfo(g); break;
            default: desenharMenu(g); break;
        }
    } else {
        desenharHUD(g);
        desenharMapa(g);
        desenharPainelLateral(g);
        desenharRodape(g);
        desenharFimPartida(g);
    }
    BitBlt(destino, 0, 0, cliente.right, cliente.bottom, bufferDC, 0, 0, SRCCOPY);
    EndPaint(janela, &ps);
}

void iniciarModoInsano() {
    if(processoInsano) return;
    wstring caminho=pastaDoExecutavel()+L"\\modo_insano.exe";
    wstring comando=L"\""+caminho+L"\"";
    vector<wchar_t> argumentos(comando.begin(),comando.end()); argumentos.push_back(0);
    STARTUPINFOW inicio{}; inicio.cb=sizeof(inicio);
    PROCESS_INFORMATION processo{};
    wstring pasta=pastaDoExecutavel();
    if(!CreateProcessW(caminho.c_str(),argumentos.data(),nullptr,nullptr,FALSE,0,nullptr,pasta.c_str(),&inicio,&processo)) {
        MessageBoxW(janelaPrincipal,L"Nao foi possivel abrir o Modo Insano. Execute compilar.ps1 para gerar os dois jogos.",L"Modo Insano",MB_OK|MB_ICONERROR);
        return;
    }
    CloseHandle(processo.hThread); processoInsano=processo.hProcess;
    pararMusica();
    fill(begin(teclaPressionada),end(teclaPressionada),false);
    ShowWindow(janelaPrincipal,SW_HIDE);
}

void lerTeclado(WPARAM tecla, LPARAM lParam) {
    // Bit 30 do lParam indica auto-repeticao do SO; ignoramos para acoes
    // pontuais (senao segurar R/espaco dispara a acao varias vezes por
    // segundo na taxa de repeticao do teclado, fora do controle do jogo).
    bool repetida = (lParam & (1 << 30)) != 0;
    if (repetida) return;
    if (modo == MENU) {
        if (telaMenu==MENU_FASES) {
            int escolha=tecla>='1' && tecla<='8' ? static_cast<int>(tecla-'0')
                       : tecla>=VK_NUMPAD1 && tecla<=VK_NUMPAD8 ? static_cast<int>(tecla-VK_NUMPAD1+1) : 0;
            if(escolha) faseSelecionada=escolha;
            else if(tecla==VK_LEFT) faseSelecionada=(faseSelecionada+TOTAL_FASES-2)%TOTAL_FASES+1;
            else if(tecla==VK_RIGHT) faseSelecionada=faseSelecionada%TOTAL_FASES+1;
            else if(tecla==VK_UP || tecla==VK_DOWN) faseSelecionada=(faseSelecionada+3)%TOTAL_FASES+1;
            else if(tecla==VK_RETURN) novaPartida(modoPendente,dificuldade);
            else if(tecla=='M') telaMenu=MENU_PERSONAGENS;
            else if(tecla==VK_ESCAPE) telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu == MENU_PERSONAGENS) {
            int escolha = tecla >= '1' && tecla <= '4' ? static_cast<int>(tecla-'1')
                        : tecla >= VK_NUMPAD1 && tecla <= VK_NUMPAD4 ? static_cast<int>(tecla-VK_NUMPAD1) : -1;
            if (escolha>=0) { if(jogadorEmSelecao==1) personagemP1=escolha; else personagemP2=escolha; }
            else if(tecla==VK_TAB) jogadorEmSelecao=3-jogadorEmSelecao;
            else if(tecla==VK_RETURN) telaMenu=MENU_FASES;
            else if(tecla=='M' || tecla==VK_ESCAPE) telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu == MENU_DIFICULDADE) {
            // Aceita tanto os numeros da fileira superior quanto o teclado
            // numerico (numpad), que usa codigos de tecla virtuais diferentes.
            if (tecla == '1' || tecla == VK_NUMPAD1) { dificuldade=FACIL; telaMenu=MENU_PERSONAGENS; }
            else if (tecla == '2' || tecla == VK_NUMPAD2) { dificuldade=MEDIO; telaMenu=MENU_PERSONAGENS; }
            else if (tecla == '3' || tecla == VK_NUMPAD3) { dificuldade=DIFICIL; telaMenu=MENU_PERSONAGENS; }
            else if (tecla == 'M' || tecla == VK_ESCAPE) telaMenu = MENU_PRINCIPAL;
            return;
        }
        if (telaMenu != MENU_PRINCIPAL) {
            // Telas so-informativas (ranking, como jogar, dificuldades,
            // pontuacao): qualquer uma dessas duas teclas volta ao menu.
            if (tecla == 'M' || tecla == VK_ESCAPE) telaMenu = MENU_PRINCIPAL;
            return;
        }
        if (tecla == '1' || tecla == VK_NUMPAD1) { modoPendente = MANUAL; telaMenu = MENU_DIFICULDADE; }
        else if (tecla == '2' || tecla == VK_NUMPAD2) { modoPendente = AUTOMATICO; telaMenu = MENU_DIFICULDADE; }
        else if (tecla == '3') telaMenu = MENU_COMO_JOGAR;
        else if (tecla == '4') telaMenu = MENU_DIFICULDADES_INFO;
        else if (tecla == '5') telaMenu = MENU_PONTUACAO;
        else if (tecla == '6') telaMenu = MENU_RANKING;
        else if (tecla == '7' || tecla == VK_NUMPAD7) { modoPendente=MANUAL; telaMenu=MENU_PERSONAGENS; jogadorEmSelecao=1; }
        else if (tecla == '8' || tecla == VK_NUMPAD8) { modoPendente=MANUAL; telaMenu=MENU_FASES; }
        else if (tecla == '9' || tecla == VK_NUMPAD9) iniciarModoInsano();
        else if (tecla == 'P') doisJogadores = !doisJogadores;
        else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
        return;
    }
    if (tecla == 'M') modo = MENU;
    else if (tecla == ' ') colocarBomba();
    else if (tecla == 'E') chutarBomba();
    else if (tecla == 'T') teletransportarJogador();
    else if (tecla == 'F') detonarBombasRelogio();
    else if (tecla == VK_RETURN) { if (doisJogadores) colocarBomba2(); }
    else if (tecla == 'R') {
        if ((estado==DERROTA || estado==FIM_DE_JOGO) && tempoFimMs<DURACAO_MORTE_MS) return;
        if (estado == FIM_DE_JOGO || estado == VITORIA) novaPartida(modo, dificuldade);
        else reiniciarFase();
    }
    else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
}

void atualizarEscalaDpi(HWND janela) {
    float escalaMonitor = (janela ? GetDpiForWindow(janela) : GetDpiForSystem()) / 96.0f;
    MONITORINFO monitor{sizeof(MONITORINFO), {}, {}, 0};
    GetMonitorInfoW(MonitorFromWindow(janela, MONITOR_DEFAULTTOPRIMARY), &monitor);
    float larguraDisponivel = static_cast<float>(monitor.rcWork.right-monitor.rcWork.left-32);
    float alturaDisponivel = static_cast<float>(monitor.rcWork.bottom-monitor.rcWork.top-64);
    escalaDpi = min(escalaMonitor, min(larguraDisponivel/LARGURA_JOGO, alturaDisponivel/ALTURA_JOGO));
    cacheFontes.clear();
    invalidarCacheMapa();
    cacheSpriteBoss.reset();
}

LRESULT CALLBACK processarMensagem(HWND janela, UINT mensagem, WPARAM wParam, LPARAM lParam) {
    static auto ultimoInstante = chrono::steady_clock::now();
    switch (mensagem) {
    case WM_CREATE:
        janelaPrincipal = janela;
        atualizarEscalaDpi(janela);
        carregarAssets();
        iniciarMusica();
        SetTimer(janela, 1, PASSO_LOGICA_MS, nullptr);
        return 0;
    case WM_TIMER: {
        if(processoInsano) {
            if(WaitForSingleObject(processoInsano,0)==WAIT_OBJECT_0) {
                CloseHandle(processoInsano); processoInsano=nullptr;
                ShowWindow(janela,SW_SHOW);
                iniciarMusica();
                ultimoInstante=chrono::steady_clock::now();
            } else return 0;
        }
        auto agora = chrono::steady_clock::now();
        int decorrido = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(agora - ultimoInstante).count());
        ultimoInstante = agora;
        // Passos fixos pequenos: uma pausa de desenho nao muda a velocidade.
        static int acumuladoMs = 0;
        acumuladoMs += min(decorrido, 100);
        while (acumuladoMs >= 8) {
            atualizar(8);
            acumuladoMs -= 8;
        }
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam < 256) {
            if(!teclaPressionada[wParam]) ordemTecla[wParam]=++proximaOrdemTecla;
            teclaPressionada[wParam] = true;
        }
        lerTeclado(wParam, lParam);
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    case WM_KEYUP:
        if (wParam < 256) teclaPressionada[wParam] = false;
        return 0;
    case WM_LBUTTONUP: {
        // Os cartoes visuais tambem podem ser escolhidos com o mouse.
        int x = static_cast<int>((static_cast<short>(LOWORD(lParam))-margemJanelaX) / escalaDpi);
        int y = static_cast<int>((static_cast<short>(HIWORD(lParam))-margemJanelaY) / escalaDpi);
        if (modo == MENU && telaMenu == MENU_PRINCIPAL) {
            if(x>=550 && x<1036 && y>=604 && y<659) { lerTeclado('9',0); return 0; }
            if(x>=550 && x<1036 && y>=666 && y<727) { lerTeclado('8',0); return 0; }
            if(x>=550 && x<1036 && y>=738 && y<808) lerTeclado('7',0);
            else if (x >= 44 && x < 514 && y >= 287 && y < 629) {
                int linha = (y-287)/57;
                if ((y-287)%57 < 47) lerTeclado('1'+linha, 0);
            } else if (x >= 44 && x < 514 && y >= 654 && y < 724) lerTeclado('P',0);
            else if (x >= 44 && x < 210 && y >= 785 && y < 830) lerTeclado(VK_ESCAPE,0);
        } else if (modo == MENU && telaMenu == MENU_FASES) {
            if(x>=44 && x<1038 && y>=150 && y<636) {
                int coluna=(x-44)/254,linha=(y-150)/254;
                if(coluna<4 && linha<2 && (x-44)%254<232 && (y-150)%254<232) faseSelecionada=linha*4+coluna+1;
            } else if(x>=44 && x<1036 && y>=683 && y<759) lerTeclado(VK_RETURN,0);
            else if(y>=780) lerTeclado('M',0);
        } else if (modo == MENU && telaMenu == MENU_PERSONAGENS) {
            if(x>=44 && x<1038 && y>=230 && y<560) {
                int coluna=(x-44)/254;
                if(coluna<4 && (x-44)%254<232) lerTeclado('1'+coluna,0);
            } else if(y>=100 && y<180) lerTeclado(VK_TAB,0);
            else if(y>=780) lerTeclado(VK_RETURN,0);
        } else if (modo == MENU && telaMenu == MENU_DIFICULDADE && y >= 182 && y < 562) {
            int coluna = (x-44)/338;
            if (x>=44 && coluna<3 && (x-44)%338<316) lerTeclado('1'+coluna,0);
        }
        InvalidateRect(janela,nullptr,FALSE);
        return 0;
    }
    case WM_CHAR:
        if (modo == MENU && telaMenu == MENU_PRINCIPAL)
            digitarNomeJogador(static_cast<wchar_t>(wParam));
        InvalidateRect(janela, nullptr, FALSE);
        return 0;
    case WM_PAINT:
        desenhar(janela);
        return 0;
    case WM_SIZE:
        if(wParam != SIZE_MINIMIZED) {
            ajustarAreaDesenho(LOWORD(lParam),HIWORD(lParam));
            InvalidateRect(janela,nullptr,FALSE);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limites=reinterpret_cast<MINMAXINFO*>(lParam);
        limites->ptMinTrackSize={640,520};
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED: {
        RECT* sugerido = reinterpret_cast<RECT*>(lParam);
        atualizarEscalaDpi(janela);
        RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
        if(!IsZoomed(janela)) SetWindowPos(janela, nullptr, sugerido->left, sugerido->top,
                     area.right-area.left, area.bottom-area.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_KILLFOCUS:
        fill(begin(teclaPressionada), end(teclaPressionada), false);
        return 0;
    case WM_DESTROY:
        if(processoInsano) { CloseHandle(processoInsano); processoInsano=nullptr; }
        executando = false;
        KillTimer(janela, 1);
        pararMusica();
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
    atualizarEscalaDpi(nullptr);
    Gdiplus::GdiplusStartupInput entradaGdiPlus;
    if (Gdiplus::GdiplusStartup(&tokenGdiPlus, &entradaGdiPlus, nullptr) != Gdiplus::Ok) return 1;

    const wchar_t CLASSE[] = L"BombermanUnivali";
    const wchar_t TITULO[] = L"Bomberman - Algoritmos e Programacao II";
    WNDCLASSW classe{};
    classe.lpfnWndProc = processarMensagem;
    classe.hInstance = instancia;
    classe.lpszClassName = CLASSE;
    classe.hCursor = LoadCursor(nullptr, IDC_ARROW);
    classe.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&classe);

    RECT area{0, 0, px(LARGURA_JOGO), px(ALTURA_JOGO)};
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    HWND janela = CreateWindowExW(0, CLASSE, TITULO,
        WS_OVERLAPPEDWINDOW,
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
