/*
Algoritmos e programação II, Trabalho M1
Desenvolvedores: Diego Silva |  Gabriel Bianchessi

Feito com matriz, structs, vetores e sub-rotinas. A interface usa Win32 e
GDI+ para abrir uma janela propria e desenhar os sprites PNG.

WASD/setas: mover | Espaco: bomba | R: reiniciar | Q ou ESC: sair
*/

#include <windows.h>
#include <mmsystem.h>
#include <digitalv.h>
#include <gdiplus.h>
#include <dinput.h>
#include <xinput.h>
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
#include "bosses_animacoes.h"

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
const int TOTAL_FASES = 10;
const wchar_t* NOMES_FASES[] = {L"CASINO AZUL",L"MASMORRA",L"FABRICA NEON",L"RUINAS DE PEDRA",
                              L"CANION CEEDRUN",L"SELVA VIVA",L"CAVERNA CONGO",L"TEMPLO ARCANO",L"MUNDO BOMBER",L"VALE LANGRISSER"};
const int INDICES_ATLAS_FASES[] = {1,4,0,2,3,5,6,7};
const int DURACAO_MORTE_MS = 1170;
const int VIDA_BOSS = 100;
const float FATOR_FORCA_BOSS = 1.15f;
// 10% mais lento que o ajuste anterior (.85).
const float FATOR_MARIO = .765f;
const int INTERVALO_MOVIMENTO_BOSS_MS = int(4000 / FATOR_FORCA_BOSS);
const int INTERVALO_ATAQUE_BOSS_MS = int(4500 / FATOR_FORCA_BOSS);
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
enum EtapaHornet { H_IDLE,H_CORRIDA,H_PREP_AGULHA,H_LANCAR,H_VOO_AGULHA,H_RECOLHER,
    H_PREP_DASH,H_DASH,H_RECUP_DASH,H_PREP_SALTO,H_SALTO,H_PREP_GIRO,H_GIRO,H_POUSO,
    H_PREP_AEREO,H_AEREO,H_RECUP_AEREO,H_GUARDA,H_CONTRA,H_RECUP_CONTRA,H_DANO,H_RECUP_DANO };
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
    // Medio: metade dos inimigos solta bombas; Dificil: 75% (ver montarFase).
    bool soltaBomba = false;
    int recargaBombaMs = 0;
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
    int dono = 0; // 0 = P1 (ou bot), 1 = P2, DONO_INIMIGO = bomba de inimigo
};
const int DONO_INIMIGO = 2;
const int ALCANCE_BOMBA_INIMIGO = 2;
const int RECARGA_BOMBA_INIMIGO_MIN_MS = 3500, RECARGA_BOMBA_INIMIGO_MAX_MS = 6000;
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
    int acaoEspecial = 0, tempoEspecialMs = 0, relogioPoseMs = 0, morteMs = 0;
    bool olhandoDireita = false;
    EtapaHornet etapaHornet=H_IDLE;
    int tempoEtapaHornetMs=0,duracaoEtapaHornetMs=0;
    float alturaHornet=0;
    bool contraPendenteHornet=false;
};

// Contadores da partida em andamento; usados tanto no HUD quanto no calculo
// de pontuacao e no registro salvo em arquivo ao final da partida.
struct EstatisticasPartida {
    int movimentos = 0;
    int bombasUsadas = 0;
    int itensColetados = 0;
    int caixasDestruidas = 0;
    int inimigosAbatidos = 0;
    int movimentosFase = 0;
    int bombasFase = 0;
    // Parte de cada jogador, gravada no ranking quando jogam dois.
    int movimentosJogador[2]{};
    int bombasJogador[2]{};
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
int tempoMovimentoMs = 0;
bool executando = true;
bool teclaPressionada[256]{};
unsigned long long ordemTecla[256]{}, proximaOrdemTecla=0;
EstatisticasPartida estatisticas;
bool registroSalvo = false;
bool erroSalvarRanking = false;
wstring nomeJogador = L"";
wstring nomeJogador2 = L"";
// Campo editado na tela de nomes (1 = P1, 2 = P2).
int campoNome = 1;
bool editandoNome = false, selecionarNome = false;
size_t cursorNome = 0;
wstring avisoNome;
int rankingSelecionado=0;
string nomeConfirmadoRanking, nomeConfirmadoRanking2, nomeOriginalRanking;
wstring nomeAntesEdicaoRanking;
wstring avisoRanking;
// As telas do menu (fora do jogo em si): principal, escolha de dificuldade
// (antes de comecar a jogar), ranking e as 3 telas so-informativas pedidas
// pelo enunciado (como jogar, dificuldades, pontuacao).
enum TelaMenu { MENU_PRINCIPAL, MENU_DIFICULDADE, MENU_RANKING, MENU_COMO_JOGAR, MENU_DIFICULDADES_INFO, MENU_PONTUACAO, MENU_PERSONAGENS, MENU_FASES, MENU_QUANTIDADE_JOGADORES, MENU_ATRIBUICAO_CONTROLES, MENU_NOMES };
int faseSelecionada = 1;
int personagemP1 = 0, personagemP2 = 1;
int jogadorEmSelecao = 1;
const wchar_t* nomesPersonagens[] = {L"BRANCO", L"PRETO", L"AZUL", L"VERMELHO"};
unique_ptr<Gdiplus::Image> personagens[4], posesVitoria[4], posesDerrota[4];
unique_ptr<Gdiplus::Image> spriteItens;
TelaMenu telaMenu = MENU_PRINCIPAL;
// Para onde a tela de nomes segue (atalhos 7/8 voltam ao seletor de fases).
TelaMenu telaAposNomes = MENU_ATRIBUICAO_CONTROLES;
Dificuldade dificuldade = FACIL;
int faseAtual = 1;
ModoJogo modoPendente = MANUAL;
// Segundo jogador (cooperativo local, so no modo manual): setas + Enter.
// Vidas e itens sao do time; cada jogador tem seu proprio limite de bombas.
// No modo INIMIGOS (modoRivais) as bombas ferem o rival e os pontos de cada
// um sao separados em pontosJogador.
bool doisJogadores = false;
// Modo de 2 jogadores: AMIGOS (a bomba de um nao fere o outro) ou INIMIGOS
// (todos contra todos, cada um com sua pontuacao no ranking).
bool modoRivais = false;
int pontosJogador[2]{};
// Tres coisas separadas: quantos jogam (doisJogadores), qual dispositivo cada
// um escolheu na tela de atribuicao e se ha um joystick conectado agora.
// Plugar um controle nunca cria jogador; so a escolha no menu faz isso.
enum Dispositivo { TECLADO, JOYSTICK };
Dispositivo dispositivoP1 = TECLADO, dispositivoP2 = TECLADO;
bool controleConectado = false;
bool controleSwitchSemLeitura = false;
bool botaoBombaControleAnterior = false;
enum OrigemControle { SEM_CONTROLE, CONTROLE_SDL, CONTROLE_XINPUT, CONTROLE_GENERICO };
OrigemControle origemControle = SEM_CONTROLE;
wstring nomeControle;
// bomba = A ou B; confirmar/voltar sao os mesmos botoes lidos nos menus.
struct EntradaControle { int dl=0,dc=0;bool bomba=false,confirmar=false,voltar=false,chute=false,teleporte=false,detonar=false,start=false,select=false; };
EntradaControle entradaControle, controleMenuAnterior;
using FuncaoXInputGetState = DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
HMODULE bibliotecaXInput = nullptr;
FuncaoXInputGetState obterEstadoXInput = nullptr;
bool tentouCarregarXInput = false;
IDirectInput8W* entradaDireta = nullptr;
IDirectInputDevice8W* controleDireto = nullptr;
// EnumDevices do DirectInput varre todos os dispositivos USB/Bluetooth e leva
// ~200 ms: buscar a cada segundo travava o jogo sem joystick. Agora so busca
// ao abrir e quando o Windows avisa que algo foi conectado (WM_DEVICECHANGE).
bool buscarControleDireto = true;
struct FaixaEixo { LONG minimo=-1000,maximo=1000; bool disponivel=true; };
FaixaEixo faixasControle[4];
Posicao jogador2{1, 2};
Posicao direcaoJogador2{0, 1};
int tempoMovimentoMs2 = 0;
int quadroAnimJogador = 0;
int quadroAnimJogador2 = 0;
// Posicao visual dos jogadores (ver comentario em Inimigo::visualL/visualC).
float jogadorVisualL = 1, jogadorVisualC = 1;
float jogador2VisualL = 1, jogador2VisualC = 2;
// Casa de saida preservada mesmo quando o jogador inverte um passo.
Posicao origemPassoJogador[2]{{1,1},{1,2}};
float distanciaAnimacaoJogador = 0, distanciaAnimacaoJogador2 = 0;
int tempoFimMs = 0;
int jogadoresAtingidos = 0;
// Dois jogadores (amigos ou inimigos): 3 vidas para cada um em cada fase.
// Quem cai toca a queda e renasce protegido; sem vidas, fica fora ate a
// proxima fase enquanto o outro continua.
const int VIDAS_POR_JOGADOR = 3;
const int IMUNIDADE_RENASCER_MS = 2000;
bool jogadorCaido[2]{};
int tempoQuedaMs[2]{};
int vidasJogador[2]{VIDAS_POR_JOGADOR, VIDAS_POR_JOGADOR};
// Som de morte (assets/audio/dead.wav, ~1,5 s): toca uma vez no lugar da
// musica. Com um jogador dura a tela de derrota (R volta a musica da fase);
// com dois, cada queda recomeca o som e a fase volta quando ele termina.
// Game over nao volta a musica da fase.
const int DURACAO_SOM_MORTE_MS = 1550;
int quedasComSom = 0, somMorteMs = 0;
int imunidadeJogadorMs[2]{};
int tempoPartidaMs = 0;
unique_ptr<Gdiplus::Image> spriteVitoria, spriteDerrota;
bool portalAtivo = false;
Posicao portalPosicao{0, 0};
enum PoderBoss { MACHADO, ENERGIA, VENENO, CHUVA_FOGO, AGULHA, FIO, FOGO_BOWSER, GOLPE_BOWSER };
const wchar_t* NOMES_BOSSES[]={L"GOLEM DA MASMORRA",L"EGG-HVC / DEMOLIDOR",L"HORNET DA SELVA",L"GUARDIAO ARCANO",L"BOWSER"};
const wchar_t* PODERES_BOSSES[]={L"Machados e facoes giratorios",L"Energia e investida: saia da frente!",L"Agulha / fio / investida",L"Fogo: desvie dos projeteis",L"Fogo / casco / salto"};
int indiceBoss(){return max(0,min(4,faseAtual/2-1));}
bool bossAnimado(){return indiceBoss()==2 || indiceBoss()==4;}
int vidaMaximaBoss(){const int vidas[]={48,64,80,VIDA_BOSS,112};return int(round(vidas[indiceBoss()]*FATOR_FORCA_BOSS));}
int larguraBoss(){return indiceBoss()==2?2:3;}
struct AreaBoss { Posicao posicao; int avisoMs, ativoMs; PoderBoss poder; };
vector<AreaBoss> areasBoss;
struct ProjetilBoss { float l,c,vl,vc,idade,vida; PoderBoss poder; int avisoMs; int arma = 0;
    float origemL=0,origemC=0;bool retornando=false; };
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
    int dono=-1; // Modo inimigos: jogador que o libertou (0/1); ataca o outro.
} marioAliado;
struct MacaMario { float sx,sy,tx,ty,idade=0,duracao=1; };
vector<MacaMario> macasMario;
vector<Posicao> macasNoChao;
const int MARIO_DC[]={0,-1,0,1},MARIO_DL[]={1,0,-1,0};
struct FogoBoss { float l, c, vl, vc, vida; };
vector<FogoBoss> fogosBoss;
Boss boss;
bool hornetNoAr(){return indiceBoss()==2 && boss.alturaHornet>26;}
vector<Posicao> perigoBoss;
int tempoPerigoBossMs = 0;
bool bossIniciado = false;
bool bossConcluido = false;
// Itens obrigatorios (secao 8 do PLANO_M2.md): cumulativos durante a fase,
// resetados em montarFase() quando a fase muda (regra do enunciado).
// Cada jogador tem os seus: o item vale so para quem o pegou (indice = dono
// da bomba: 0 = P1 ou bot, 1 = P2).
struct PoderesJogador {
    int alcance = ALCANCE_EXPLOSAO_INICIAL;
    int bombasExtras = 0;
    bool cartaChute = false, cartaTeleporte = false;
    bool bombaRelogio = false, imuneAPropriaBomba = false, atravessaBloco = false;
};
PoderesJogador poderes[2];
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
unique_ptr<Gdiplus::Image> atlasHornet, atlasBowser, spriteArenasNovas;
unique_ptr<Gdiplus::Image> spriteArenaCongo;
unordered_map<int,unique_ptr<Gdiplus::Bitmap>> cacheQuadrosBoss[2];
unique_ptr<Gdiplus::Bitmap> spritesAgulhaBoss[3], spritesFioBoss[5], spritesFogoBowser[4];
unique_ptr<Gdiplus::Bitmap> fiosAgulhaBoss[3], efeitosDashHornet[5];
Gdiplus::Image* imagemBossAtual(){int i=indiceBoss();
    if(i==2)return atlasHornet.get();
    if(i==4)return atlasBowser.get();
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
unique_ptr<Gdiplus::Bitmap> spriteInimigoVerde;
// Tilesets de chao/parede/caixa por fase - coordenadas de recorte validadas
// em D:\algoritmos_prog_2\estudos_sprite\batalha.cpp (funcao sourceTiles),
// reaproveitadas aqui so pelas coordenadas, sem copiar codigo daquele
// projeto (que e so um estudo visual, nao faz parte do jogo).
unique_ptr<Gdiplus::Image> spriteTemaFase1; // SNES Stage 2-5 (Casino Azul)
unique_ptr<Gdiplus::Image> spriteTemaFase3; // SNES Stage 6-3 (Fabrica Neon)
unique_ptr<Gdiplus::Image> spriteArenasEstudo;
unique_ptr<Gdiplus::Bitmap> spriteCaixaSelva;
unique_ptr<Gdiplus::Bitmap> spritePedraSelva;
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

// Leitura de joystick em tres caminhos, nesta ordem de preferencia:
// 1) SDL2 GameController (SDL2.dll ao lado do exe): le Switch Pro/8BitDo pelo
//    protocolo HID proprio (o mesmo caminho do PCSX2), Xbox, PlayStation etc.
// 2) XInput: controles compativeis com Xbox, se a SDL2.dll faltar.
// 3) DirectInput: outros controles genericos HID/USB.
// So um controle e usado por vez; o menu decide se ele pertence a P1 ou P2.
wstring pastaDoExecutavel();
EntradaControle direcaoDoControle(int x,int y,int limite){
    EntradaControle e;
    if(abs(x)<limite && abs(y)<limite)return e;
    if(abs(x)>abs(y))e.dc=x>0?1:-1;
    else e.dl=y>0?1:-1;
    return e;
}
// Monta a entrada a partir do direcional (prioridade) ou do analogico, mais
// os botoes no layout padrao: A/B bomba, X chute, Y teleporte, L/R detonar.
EntradaControle montarEntrada(bool cima,bool baixo,bool esq,bool dir,int x,int y,int limite,
                              bool a,bool b,bool xb,bool yb,bool ombro,bool start,bool select){
    EntradaControle e;
    if(cima)e.dl=-1;else if(baixo)e.dl=1;else if(esq)e.dc=-1;else if(dir)e.dc=1;
    else e=direcaoDoControle(x,y,limite);
    e.confirmar=a;e.voltar=b;e.bomba=a||b;e.chute=xb;e.teleporte=yb;e.detonar=ombro;e.start=start;e.select=select;
    return e;
}
EntradaControle traduzirXInput(const XINPUT_GAMEPAD& g){
    WORD b=g.wButtons;
    return montarEntrada(b&XINPUT_GAMEPAD_DPAD_UP,b&XINPUT_GAMEPAD_DPAD_DOWN,b&XINPUT_GAMEPAD_DPAD_LEFT,b&XINPUT_GAMEPAD_DPAD_RIGHT,
                         int(g.sThumbLX),-int(g.sThumbLY),10500,b&XINPUT_GAMEPAD_A,b&XINPUT_GAMEPAD_B,b&XINPUT_GAMEPAD_X,b&XINPUT_GAMEPAD_Y,
                         b&(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER),b&XINPUT_GAMEPAD_START,b&XINPUT_GAMEPAD_BACK);
}
EntradaControle traduzirDirectInput(const DIJOYSTATE2& g){
    DWORD pov=g.rgdwPOV[0];
    bool cima=false,baixo=false,esq=false,dir=false;
    if(pov!=0xffffffff){
        if(pov<=4500 || pov>=31500)cima=true;
        else if(pov>=13500 && pov<=22500)baixo=true;
        else if(pov<13500)dir=true;
        else esq=true;
    }
    auto botao=[&](int i){return (g.rgbButtons[i]&0x80)!=0;};
    EntradaControle e=montarEntrada(cima,baixo,esq,dir,int(g.lX),int(g.lY),350,botao(0),botao(1),botao(2),botao(3),
                                    botao(4)||botao(5),botao(9),botao(8));
    if(!e.dl && !e.dc){EntradaControle direita=direcaoDoControle(int(g.lRx),int(g.lRy),350);e.dl=direita.dl;e.dc=direita.dc;}
    return e;
}

// Funcoes da SDL2 carregadas em tempo de execucao: sem a DLL o jogo continua
// funcionando com XInput/DirectInput e o teclado.
struct BibliotecaSDL {
    HMODULE dll=nullptr; bool tentou=false, carregada=false;
    int (*iniciar)(unsigned)=nullptr; void (*encerrar)(unsigned)=nullptr;
    int (*setHint)(const char*,const char*)=nullptr;
    int (*eventosControle)(int)=nullptr; int (*eventosJoystick)(int)=nullptr;
    int (*quantidade)()=nullptr; int (*ehGameController)(int)=nullptr;
    void* (*abrir)(int)=nullptr; void (*fechar)(void*)=nullptr; int (*conectado)(void*)=nullptr;
    const char* (*nome)(void*)=nullptr; void (*atualizar)()=nullptr;
    short (*eixo)(void*,int)=nullptr; unsigned char (*botao)(void*,int)=nullptr;
    void* controle=nullptr; ULONGLONG proximaBusca=0;
} sdl;
template<class F> bool obterFuncaoSDL(const char* nome,F& funcao){
    FARPROC p=GetProcAddress(sdl.dll,nome);
    static_assert(sizeof(funcao)==sizeof(p));
    memcpy(&funcao,&p,sizeof(funcao));
    return funcao!=nullptr;
}
void carregarSDL(){
    if(sdl.tentou)return;
    sdl.tentou=true;
    wstring caminho=pastaDoExecutavel()+L"\\SDL2.dll";
    sdl.dll=LoadLibraryExW(caminho.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!sdl.dll)return;
    bool ok=obterFuncaoSDL("SDL_InitSubSystem",sdl.iniciar) && obterFuncaoSDL("SDL_QuitSubSystem",sdl.encerrar) &&
        obterFuncaoSDL("SDL_SetHint",sdl.setHint) && obterFuncaoSDL("SDL_GameControllerEventState",sdl.eventosControle) &&
        obterFuncaoSDL("SDL_JoystickEventState",sdl.eventosJoystick) && obterFuncaoSDL("SDL_NumJoysticks",sdl.quantidade) &&
        obterFuncaoSDL("SDL_IsGameController",sdl.ehGameController) && obterFuncaoSDL("SDL_GameControllerOpen",sdl.abrir) &&
        obterFuncaoSDL("SDL_GameControllerClose",sdl.fechar) && obterFuncaoSDL("SDL_GameControllerGetAttached",sdl.conectado) &&
        obterFuncaoSDL("SDL_GameControllerName",sdl.nome) && obterFuncaoSDL("SDL_GameControllerUpdate",sdl.atualizar) &&
        obterFuncaoSDL("SDL_GameControllerGetAxis",sdl.eixo) && obterFuncaoSDL("SDL_GameControllerGetButton",sdl.botao);
    const unsigned SDL_INIT_GAMECONTROLLER=0x2000;
    if(ok){
        // O jogo nao tem janela SDL: le o controle sempre e filtra o foco aqui.
        sdl.setHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","1");
        ok=sdl.iniciar(SDL_INIT_GAMECONTROLLER)==0;
    }
    if(!ok){FreeLibrary(sdl.dll);sdl.dll=nullptr;return;}
    // Lemos o estado direto; sem isso a fila de eventos da SDL so cresceria.
    sdl.eventosControle(0);sdl.eventosJoystick(0);
    sdl.carregada=true;
}
bool lerControleSDL(EntradaControle& leitura){
    if(!sdl.carregada)return false;
    sdl.atualizar();
    if(sdl.controle && !sdl.conectado(sdl.controle)){sdl.fechar(sdl.controle);sdl.controle=nullptr;}
    if(!sdl.controle && GetTickCount64()>=sdl.proximaBusca){
        sdl.proximaBusca=GetTickCount64()+500;
        for(int i=0;i<sdl.quantidade() && !sdl.controle;++i)
            if(sdl.ehGameController(i))sdl.controle=sdl.abrir(i);
        if(sdl.controle){
            const char* n=sdl.nome(sdl.controle);string utf8=n?n:"Controle";
            int tamanho=MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,nullptr,0);
            nomeControle.assign(max(1,tamanho),L'\0');
            MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,&nomeControle[0],tamanho);
            nomeControle.resize(wcslen(nomeControle.c_str()));
        }
    }
    if(!sdl.controle)return false;
    auto b=[](int i){return sdl.botao(sdl.controle,i)!=0;};
    // Indices SDL_GameControllerButton/Axis: A0 B1 X2 Y3 BACK4 START6 L9 R10 cima11 baixo12 esq13 dir14.
    leitura=montarEntrada(b(11),b(12),b(13),b(14),sdl.eixo(sdl.controle,0),sdl.eixo(sdl.controle,1),12000,
                          b(0),b(1),b(2),b(3),b(9)||b(10),b(6),b(4));
    return true;
}
bool controleNoJogador(int jogadorNumero){
    if(!controleConectado || modo!=MANUAL)return false;
    if(jogadorNumero==1)return dispositivoP1==JOYSTICK;
    return doisJogadores && dispositivoP2==JOYSTICK;
}
// P2 controlado por joystick (usado nas legendas e no teclado).
bool controleAtivo(){return controleNoJogador(2);}
LONG normalizarEixoControle(LONG valor,const FaixaEixo& faixa){
    if(!faixa.disponivel || faixa.maximo<=faixa.minimo)return 0;
    long long escala=(static_cast<long long>(valor)-faixa.minimo)*2000/(faixa.maximo-faixa.minimo)-1000;
    return static_cast<LONG>(clamp(escala,-1000LL,1000LL));
}
void normalizarEstadoControle(DIJOYSTATE2& estado){
    LONG* eixos[]={&estado.lX,&estado.lY,&estado.lRx,&estado.lRy};
    for(int i=0;i<4;++i)*eixos[i]=normalizarEixoControle(*eixos[i],faixasControle[i]);
}
void carregarXInput(){
    if(tentouCarregarXInput)return;
    tentouCarregarXInput=true;
    for(const wchar_t* dll:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"}){
        HMODULE lib=LoadLibraryW(dll);if(!lib)continue;
        FARPROC procedimento=GetProcAddress(lib,"XInputGetState");
        FuncaoXInputGetState funcao=nullptr;
        static_assert(sizeof(funcao)==sizeof(procedimento));
        memcpy(&funcao,&procedimento,sizeof(funcao));
        if(funcao){bibliotecaXInput=lib;obterEstadoXInput=funcao;break;}
        FreeLibrary(lib);
    }
}
void liberarControleDireto(){if(controleDireto){controleDireto->Unacquire();controleDireto->Release();controleDireto=nullptr;}}
BOOL CALLBACK descobrirControleGenerico(const DIDEVICEINSTANCEW* info,void* contexto){
    // O Switch Pro (inclusive o 8BitDo em modo Switch) exige inicializacao
    // propria do protocolo HID. O DirectInput o enumera, mas entrega sensores
    // e contadores como eixos/botoes: movimento e bombas fantasmas. Ele e lido
    // pela SDL2; aqui so marcamos que existe para avisar se a DLL faltar.
    if(LOWORD(info->guidProduct.Data1)==0x057e && HIWORD(info->guidProduct.Data1)==0x2009){
        controleSwitchSemLeitura=true;
        return DIENUM_CONTINUE;
    }
    auto janela=reinterpret_cast<HWND>(contexto);IDirectInputDevice8W* dispositivo=nullptr;
    if(FAILED(entradaDireta->CreateDevice(info->guidInstance,&dispositivo,nullptr)))return DIENUM_CONTINUE;
    if(FAILED(dispositivo->SetDataFormat(&c_dfDIJoystick2)) ||
       FAILED(dispositivo->SetCooperativeLevel(janela,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND))){dispositivo->Release();return DIENUM_CONTINUE;}
    const DWORD eixos[]={DWORD(DIJOFS_X),DWORD(DIJOFS_Y),DWORD(DIJOFS_RX),DWORD(DIJOFS_RY)};
    for(int i=0;i<4;++i){
        DWORD eixo=eixos[i];
        DIPROPRANGE faixa{{sizeof(DIPROPRANGE),sizeof(DIPROPHEADER),eixo,DIPH_BYOFFSET},-1000,1000};
        if(FAILED(dispositivo->SetProperty(DIPROP_RANGE,&faixa.diph))){
            faixa.lMin=0;faixa.lMax=0;
            if(FAILED(dispositivo->GetProperty(DIPROP_RANGE,&faixa.diph)))faixasControle[i]={0,0,false};
            else faixasControle[i]={faixa.lMin,faixa.lMax,true};
        }else faixasControle[i]={-1000,1000,true};
    }
    dispositivo->Acquire();DIJOYSTATE2 estado{};
    HRESULT resultado=dispositivo->Poll();
    if(SUCCEEDED(resultado))resultado=dispositivo->GetDeviceState(sizeof(estado),&estado);
    if(FAILED(resultado)){dispositivo->Unacquire();dispositivo->Release();return DIENUM_CONTINUE;}
    nomeControle=info->tszProductName;
    controleDireto=dispositivo;return DIENUM_STOP;
}
void atualizarEstadoControle(HWND janela){
    carregarSDL();EntradaControle leitura;OrigemControle origem=SEM_CONTROLE;
    if(lerControleSDL(leitura)){origem=CONTROLE_SDL;liberarControleDireto();}
    if(origem==SEM_CONTROLE){carregarXInput();
        if(obterEstadoXInput){for(DWORD i=0;i<XUSER_MAX_COUNT;++i){XINPUT_STATE estado{};
            if(obterEstadoXInput(i,&estado)==ERROR_SUCCESS){origem=CONTROLE_XINPUT;leitura=traduzirXInput(estado.Gamepad);nomeControle=L"Controle XInput";break;}}}
    }
    if(origem==SEM_CONTROLE){
        if(!entradaDireta)DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&entradaDireta),nullptr);
        if(entradaDireta && !controleDireto && buscarControleDireto){
            buscarControleDireto=false;
            controleSwitchSemLeitura=false;
            entradaDireta->EnumDevices(DI8DEVCLASS_GAMECTRL,descobrirControleGenerico,janela,DIEDFL_ATTACHEDONLY);
        }
        if(controleDireto){DIJOYSTATE2 estado{};HRESULT resultado=controleDireto->Poll();
            if(FAILED(resultado)){controleDireto->Acquire();resultado=controleDireto->Poll();}
            if(SUCCEEDED(resultado))resultado=controleDireto->GetDeviceState(sizeof(estado),&estado);
            if(SUCCEEDED(resultado)){normalizarEstadoControle(estado);origem=CONTROLE_GENERICO;leitura=traduzirDirectInput(estado);}
            else liberarControleDireto();
        }
    }else controleSwitchSemLeitura=false;
    // Com o jogo fora de foco (ex.: PCSX2 aberto) o controle nao age aqui.
    if(janelaPrincipal && GetForegroundWindow()!=janelaPrincipal)leitura=EntradaControle{};
    bool estavaConectado=controleConectado;
    controleConectado=origem!=SEM_CONTROLE;origemControle=origem;entradaControle=leitura;
    if(!controleConectado)nomeControle.clear();
    // Ao conectar, botoes ja apertados nao contam como novo toque.
    if(!controleConectado || !estavaConectado){botaoBombaControleAnterior=leitura.bomba;controleMenuAnterior=leitura;}
}
void encerrarControle(){liberarControleDireto();if(entradaDireta){entradaDireta->Release();entradaDireta=nullptr;}
    if(bibliotecaXInput){FreeLibrary(bibliotecaXInput);bibliotecaXInput=nullptr;}obterEstadoXInput=nullptr;tentouCarregarXInput=false;
    if(sdl.carregada){if(sdl.controle)sdl.fechar(sdl.controle);sdl.encerrar(0x2000);}
    if(sdl.dll)FreeLibrary(sdl.dll);
    sdl=BibliotecaSDL{};}

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
    int n=WideCharToMultiByte(CP_UTF8,0,texto.data(),int(texto.size()),nullptr,0,nullptr,nullptr);
    string resultado(n,'\0');
    if(n)WideCharToMultiByte(CP_UTF8,0,texto.data(),int(texto.size()),resultado.data(),n,nullptr,nullptr);
    return resultado;
}
wstring strParaW(const string& texto){
    int pagina=CP_UTF8,n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,texto.data(),int(texto.size()),nullptr,0);
    if(!n && !texto.empty()){pagina=1252;n=MultiByteToWideChar(pagina,0,texto.data(),int(texto.size()),nullptr,0);}
    wstring resultado(n,L'\0');if(n)MultiByteToWideChar(pagina,0,texto.data(),int(texto.size()),resultado.data(),n);
    return resultado;
}

// Sobrecarga de verdade (duas assinaturas distintas, nao parametro default):
// a versao com caminho explicito permite gravar em um arquivo alternativo
// (por exemplo em testes), e a versao sem caminho usa o ranking padrao do jogo.
string limparNomeRanking(const string& nome){
    string limpo;bool espaco=false;
    for(unsigned char c:nome){if(c==' '){espaco=!limpo.empty();continue;}
        if(c<32)continue;
        if(espaco){limpo+=' ';espaco=false;}limpo+=char(c);}
    return limpo;
}
string chaveNomeRanking(const string& nome){
    wstring chave=strParaW(limparNomeRanking(nome)),minuscula(chave.size(),L'\0');
    if(!chave.empty())LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,chave.data(),int(chave.size()),minuscula.data(),int(minuscula.size()),nullptr,nullptr,0);
    return wparaStr(minuscula);
}
string campoCsv(const string& campo){
    if(campo.find_first_of(";\"\r\n")==string::npos)return campo;
    string saida="\"";for(char c:campo){if(c=='\"')saida+='\"';saida+=c;}return saida+'\"';
}
bool lerCamposCsv(const string& linha,vector<string>& campos){
    campos.clear();string campo;bool aspas=false,fechado=false;
    for(size_t i=0;i<linha.size();++i){char c=linha[i];
        if(aspas){if(c=='\"'){if(i+1<linha.size() && linha[i+1]=='\"'){campo+='\"';++i;}else {aspas=false;fechado=true;}}else campo+=c;}
        else if(c==';'){campos.push_back(campo);campo.clear();fechado=false;}
        else if(c=='\"'){if(!campo.empty() || fechado)return false;aspas=true;}
        else if(c!='\r'){if(fechado)return false;campo+=c;}
    }
    if(aspas)return false;
    campos.push_back(campo);return campos.size()==5;
}
bool melhorRegistro(const RegistroPontuacao& a,const RegistroPontuacao& b){
    if(a.pontuacao!=b.pontuacao)return a.pontuacao>b.pontuacao;
    if(a.bombas!=b.bombas)return a.bombas<b.bombas;
    if(a.movimentos!=b.movimentos)return a.movimentos<b.movimentos;
    return chaveNomeRanking(a.jogador)<chaveNomeRanking(b.jogador);
}
vector<RegistroPontuacao> lerRanking(const string& caminhoArquivo);
bool gravarRanking(vector<RegistroPontuacao> registros,const string& caminhoArquivo){
    sort(registros.begin(),registros.end(),melhorRegistro);
    CopyFileA(caminhoArquivo.c_str(),(caminhoArquivo+".bak").c_str(),TRUE);
    string temporario=caminhoArquivo+".tmp";ofstream arquivo(temporario,ios::trunc);
    if(!arquivo)return false;
    for(const auto& r:registros)arquivo<<campoCsv(r.data)<<';'<<campoCsv(r.jogador)<<';'<<r.pontuacao<<';'<<r.bombas<<';'<<r.movimentos<<'\n';
    arquivo.flush();if(!arquivo)return false;arquivo.close();
    return MoveFileExA(temporario.c_str(),caminhoArquivo.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
bool salvarRegistroPartida(const RegistroPontuacao& registro, const string& caminhoArquivo) {
    RegistroPontuacao novo=registro;novo.jogador=limparNomeRanking(novo.jogador);
    if(novo.jogador.empty())return false;
    auto registros=lerRanking(caminhoArquivo);
    auto existente=find_if(registros.begin(),registros.end(),[&](const RegistroPontuacao& r){return chaveNomeRanking(r.jogador)==chaveNomeRanking(novo.jogador);});
    if(existente==registros.end())registros.push_back(novo);
    else if(melhorRegistro(novo,*existente))*existente=novo;
    return gravarRanking(registros,caminhoArquivo);
}

bool salvarRegistroPartida(const RegistroPontuacao& registro) {
    return salvarRegistroPartida(registro, caminhoRankingUtf8());
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
        vector<string> campos;if(!lerCamposCsv(linha,campos))continue;
        RegistroPontuacao registro;
        registro.data=campos[0];registro.jogador=campos[1];
        auto inteiroValido=[](const string& valor,int& destino){try{size_t fim;destino=stoi(valor,&fim);return fim==valor.size() && destino>=0;}catch(...){return false;}};
        if(!inteiroValido(campos[2],registro.pontuacao) || !inteiroValido(campos[3],registro.bombas) || !inteiroValido(campos[4],registro.movimentos))continue;
        registro.jogador=limparNomeRanking(registro.jogador);
        if(registro.jogador.empty() || registro.pontuacao<0 || registro.bombas<0 || registro.movimentos<0)continue;
        auto existente=find_if(registros.begin(),registros.end(),[&](const RegistroPontuacao& r){return chaveNomeRanking(r.jogador)==chaveNomeRanking(registro.jogador);});
        if(existente==registros.end())registros.push_back(registro);
        else if(melhorRegistro(registro,*existente))*existente=registro;
    }
    sort(registros.begin(), registros.end(),
         melhorRegistro);
    return registros;
}
bool editarNomeRanking(const string& antigo,const string& novo,const string& caminho){
    auto registros=lerRanking(caminho);string limpo=limparNomeRanking(novo);
    if(limpo.empty() || strParaW(limpo).size()>24)return false;
    auto alvo=find_if(registros.begin(),registros.end(),[&](const RegistroPontuacao& r){return chaveNomeRanking(r.jogador)==chaveNomeRanking(antigo);});
    if(alvo==registros.end())return false;
    for(const auto& r:registros)if(&r!=&*alvo && chaveNomeRanking(r.jogador)==chaveNomeRanking(limpo))return false;
    alvo->jogador=limpo;return gravarRanking(registros,caminho);
}
bool excluirRegistroRanking(const string& nome,const string& caminho){
    auto registros=lerRanking(caminho);size_t antes=registros.size();
    registros.erase(remove_if(registros.begin(),registros.end(),[&](const RegistroPontuacao& r){return chaveNomeRanking(r.jogador)==chaveNomeRanking(nome);}),registros.end());
    return registros.size()<antes && gravarRanking(registros,caminho);
}

// Na tela de nomes o campo 2 edita o nome de P2; nas demais, o de P1.
wstring& nomeEmEdicao() { return telaMenu==MENU_NOMES && campoNome==2 ? nomeJogador2 : nomeJogador; }
string& confirmacaoEmEdicao() { return telaMenu==MENU_NOMES && campoNome==2 ? nomeConfirmadoRanking2 : nomeConfirmadoRanking; }
void digitarNomeJogador(wchar_t caractere) {
    if(!editandoNome)return;
    wstring& nome=nomeEmEdicao();
    cursorNome=min(cursorNome,nome.size());
    if (caractere == L'\b') {
        if(selecionarNome){nome.clear();cursorNome=0;selecionarNome=false;}
        else if(cursorNome>0){nome.erase(--cursorNome,1);}
        return;
    }
    bool valido=caractere>=L' ' && caractere!=127;
    if(valido){
        if(selecionarNome){nome.clear();cursorNome=0;selecionarNome=false;}
        if(nome.size()<24){nome.insert(cursorNome,1,caractere);cursorNome++;avisoNome.clear();confirmacaoEmEdicao().clear();}
    }
}
bool confirmarNomeJogador(bool verificarExistente=false){
    wstring& nome=nomeEmEdicao();
    string limpo=limparNomeRanking(wparaStr(nome));
    if(limpo.empty()){avisoNome=L"Digite o nome antes de jogar.";editandoNome=true;cursorNome=0;return false;}
    nome=strParaW(limpo);cursorNome=nome.size();
    string chave=chaveNomeRanking(limpo);
    if(telaMenu==MENU_NOMES && campoNome==2 && chave==chaveNomeRanking(wparaStr(nomeJogador))){
        avisoNome=L"Os dois jogadores precisam de nomes diferentes.";editandoNome=true;selecionarNome=true;return false;
    }
    if(verificarExistente && confirmacaoEmEdicao()!=chave){
        for(const auto& r:lerRanking())if(chaveNomeRanking(r.jogador)==chave){
            wstring mensagem=L"O nome "+strParaW(r.jogador)+L" ja existe no ranking.\nPontuacao: "+to_wstring(r.pontuacao)+L" pontos.\n\nDeseja continuar com esse nome? Seu melhor resultado sera mantido e atualizado se voce supera-lo.";
            if(!janelaPrincipal || MessageBoxW(janelaPrincipal,mensagem.c_str(),L"Nome existente",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES){avisoNome=L"Escolha outro nome ou gerencie o ranking.";editandoNome=true;selecionarNome=true;return false;}
            break;
        }
        confirmacaoEmEdicao()=chave;
    }
    editandoNome=false;selecionarNome=false;avisoNome.clear();return true;
}
// Nomes confirmados para a partida manual (dois e diferentes quando jogam dois).
bool nomesProntos(){
    auto pronto=[](const wstring& nome,const string& confirmado){
        string limpo=limparNomeRanking(wparaStr(nome));
        return !limpo.empty() && chaveNomeRanking(limpo)==confirmado;
    };
    if(!pronto(nomeJogador,nomeConfirmadoRanking))return false;
    return !doisJogadores || (pronto(nomeJogador2,nomeConfirmadoRanking2) && nomeConfirmadoRanking2!=nomeConfirmadoRanking);
}
void editarCampoNome(int campo){
    campoNome=doisJogadores?campo:1;editandoNome=true;
    selecionarNome=!nomeEmEdicao().empty();cursorNome=nomeEmEdicao().size();
}
void abrirTelaNomes(TelaMenu depois){
    telaAposNomes=depois;telaMenu=MENU_NOMES;avisoNome.clear();editarCampoNome(1);
}
void abrirAtribuicaoControles();
// Enter na tela de nomes: confirma o campo atual e segue para o proximo.
void confirmarCampoNome(){
    if(!confirmarNomeJogador(true))return;
    if(campoNome==1 && doisJogadores){editarCampoNome(2);return;}
    campoNome=1;
    if(!nomesProntos()){editarCampoNome(1);return;}
    if(telaAposNomes==MENU_ATRIBUICAO_CONTROLES)abrirAtribuicaoControles();
    else telaMenu=telaAposNomes;
}
void iniciarRenomeacaoRanking(){
    auto registros=lerRanking();if(registros.empty())return;
    rankingSelecionado=clamp(rankingSelecionado,0,int(registros.size())-1);
    nomeOriginalRanking=registros[rankingSelecionado].jogador;nomeAntesEdicaoRanking=nomeJogador;
    nomeJogador=strParaW(nomeOriginalRanking);editandoNome=true;selecionarNome=true;cursorNome=nomeJogador.size();avisoRanking.clear();
}
void terminarRenomeacaoRanking(bool confirmar){
    if(confirmar && !editarNomeRanking(nomeOriginalRanking,wparaStr(nomeJogador),caminhoRankingUtf8())){
        avisoRanking=L"Nome vazio, repetido ou arquivo sem permissao. Escolha outro nome.";return;
    }
    if(!confirmar || chaveNomeRanking(wparaStr(nomeAntesEdicaoRanking))!=chaveNomeRanking(nomeOriginalRanking))nomeJogador=nomeAntesEdicaoRanking;
    else nomeJogador=strParaW(limparNomeRanking(wparaStr(nomeJogador)));
    editandoNome=false;selecionarNome=false;nomeOriginalRanking.clear();nomeConfirmadoRanking.clear();
    avisoRanking=confirmar?L"Nome atualizado. Pontuacao preservada.":L"Edicao cancelada.";
}
void pedirExclusaoRanking(){
    auto registros=lerRanking();if(registros.empty() || !janelaPrincipal)return;
    rankingSelecionado=clamp(rankingSelecionado,0,int(registros.size())-1);auto r=registros[rankingSelecionado];
    wstring texto=L"Excluir "+strParaW(r.jogador)+L" do ranking?\n\nA pontuacao de "+to_wstring(r.pontuacao)+L" pontos sera perdida. Voce podera jogar novamente com esse nome, comecando do zero.";
    if(MessageBoxW(janelaPrincipal,texto.c_str(),L"Excluir registro",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
    bool ok=excluirRegistroRanking(r.jogador,caminhoRankingUtf8());
    avisoRanking=ok?L"Registro excluido. Esse nome pode recomecar do zero.":L"Nao foi possivel excluir o registro.";
    if(ok)nomeConfirmadoRanking.clear();
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
// "dono" credita tambem a parte individual (0 = P1, 1 = P2); -1 so o total.
void ajustarPontos(int delta, int dono = -1) {
    pontos = max(0, pontos + delta);
    if (dono == 0 || dono == 1) pontosJogador[dono] = max(0, pontosJogador[dono] + delta);
}

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
// Jogador presente na arena agora (P2 so existe no manual com dois).
bool jogadorEmCampo(int numero) {
    return numero == 1 ? !jogadorCaido[0] : segundoJogadorAtivo() && !jogadorCaido[1];
}
// Com um jogador, ser atingido encerra a tentativa (e custa uma vida). Com
// dois, cada um perde uma das suas 3 vidas e renasce; quem zera fica fora
// ate a proxima fase. Os dois sem vidas = fim de jogo.
void derrubarJogador(int numero);
// Quais teclas de movimento pertencem a cada jogador. O joystick escolhido
// tira o teclado do seu dono; se ele desconectar, o teclado volta a valer.
enum { TECLAS_WASD = 1, TECLAS_SETAS = 2 };
int teclaDoJogador(bool p2) {
    bool p1Teclado = !controleNoJogador(1);
    bool p2Teclado = segundoJogadorAtivo() && !controleNoJogador(2);
    if (!p2) return !p1Teclado ? 0 : p2Teclado ? TECLAS_WASD : TECLAS_WASD|TECLAS_SETAS;
    return !p2Teclado ? 0 : p1Teclado ? TECLAS_SETAS : TECLAS_WASD|TECLAS_SETAS;
}
bool joystickDoJogador(int numero) {
    return controleConectado && (numero == 1 ? dispositivoP1 == JOYSTICK : doisJogadores && dispositivoP2 == JOYSTICK);
}
// Mesma divisao de teclaDoJogador, mas valida tambem no menu (antes da partida).
wstring teclasDoJogador(int numero) {
    if (joystickDoJogador(numero)) return L"JOYSTICK (A/B bomba)";
    bool outroNoTeclado = doisJogadores && !joystickDoJogador(3 - numero);
    if (!outroNoTeclado) return L"WASD/SETAS + ESPACO/ENTER";
    return numero == 1 ? L"WASD + ESPACO" : L"SETAS + ENTER";
}
wstring resumoJogadores() {
    wstring texto = doisJogadores ? L"2 JOGADORES  /  P1 " : L"1 JOGADOR  /  P1 ";
    texto += teclasDoJogador(1);
    if (doisJogadores) texto += L"  +  P2 " + teclasDoJogador(2);
    return texto;
}
// Itens seguem o dono das teclas de movimento: quem usa WASD aperta E/T/F,
// quem usa as setas aperta I/O/P; quem esta no joystick usa X/Y/L/R.
// Retorna o dono (0 = P1, 1 = P2) ou -1 se ninguem usa esse grupo de teclas.
int donoDasTeclas(int grupo) {
    if (teclaDoJogador(false) & grupo) return 0;
    if (teclaDoJogador(true) & grupo) return 1;
    return -1;
}
struct TeclasItens { const wchar_t *chute, *teleporte, *detonar; };
TeclasItens teclasItensDoJogador(int dono) {
    if (controleNoJogador(dono + 1)) return {L"X", L"Y", L"L/R"};
    if (donoDasTeclas(TECLAS_WASD) == dono) return {L"E", L"T", L"F"};
    return {L"I", L"O", L"P"};
}
// Com um jogador no joystick, as instrucoes mostram os botoes dele.
bool itensNoJoystick() { return modo == MANUAL && joystickDoJogador(1); }
bool somenteJoystick() { return itensNoJoystick() && !doisJogadores; }
bool algumJoystickNaPartida() { return modo == MANUAL && (joystickDoJogador(1) || joystickDoJogador(2)); }
wstring situacaoControle() {
    if (controleConectado) return L"Joystick conectado: " + nomeControle;
    if (controleSwitchSemLeitura) return L"Pro Controller visto, mas falta SDL2.dll ao lado do jogo.";
    return sdl.carregada ? L"Nenhum joystick conectado. Conecte e ele aparece aqui."
                         : L"Nenhum joystick (SDL2.dll ausente: so controles XInput/DirectInput).";
}

// Tela de atribuicao no estilo "escolher lados": cada dispositivo fica sob
// P1, ao centro (sem uso) ou sob P2. -1 = P1, 0 = sem uso, 1 = P2.
int ladoDispositivo(Dispositivo d) {
    if (dispositivoP1 == d) return -1;
    if (doisJogadores && dispositivoP2 == d) return 1;
    return 0;
}
// Sem joystick nao ha o que mover: o teclado atende P1 (e P2, dividido).
void moverDispositivo(Dispositivo d, int sentido) {
    if (!controleConectado) { dispositivoP1 = dispositivoP2 = TECLADO; return; }
    Dispositivo outro = d == TECLADO ? JOYSTICK : TECLADO;
    if (!doisJogadores) dispositivoP1 = sentido < 0 ? d : outro;
    else if (sentido < 0) { dispositivoP1 = d; dispositivoP2 = outro; }
    else { dispositivoP1 = outro; dispositivoP2 = d; }
}
// Opcao 0: P1 no teclado (e P2 no joystick). Opcao 1: P1 no joystick.
void escolherOpcaoControles(int opcao) { moverDispositivo(TECLADO, opcao == 0 ? -1 : 1); }
void validarAtribuicao() {
    if (!controleConectado) { dispositivoP1 = dispositivoP2 = TECLADO; return; }
    if (doisJogadores && dispositivoP1 == dispositivoP2) dispositivoP2 = dispositivoP1 == TECLADO ? JOYSTICK : TECLADO;
}
void abrirAtribuicaoControles() { validarAtribuicao(); telaMenu = MENU_ATRIBUICAO_CONTROLES; }

Gdiplus::Image* imagemInimigoAtual(){
    bool vermelhoEmUso=personagemP1==3 || (segundoJogadorAtivo() && personagemP2==3);
    return vermelhoEmUso?spriteInimigoVerde.get():spriteInimigoDirecional.get();
}

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

// Dono da primeira chama que fere o corpo (filtrada por "fere"); -1 = nenhuma.
template <typename Fere>
int donoDaChama(float l, float c, Fere fere) {
    for (const Bomba& b : bombas)
        if (b.explodindo && fere(b))
            for (Posicao p : b.celulasExplosao) if (corpoNaCelula(l,c,p)) return b.dono;
    return -1;
}
// Bomba de inimigo fere sempre; a propria, salvo com escudo; a do outro
// jogador so no modo inimigos (no modo amigos ela nao fere o parceiro).
bool bombaFereJogador(const Bomba& b, int numero) {
    if (b.dono == DONO_INIMIGO) return true;
    if (b.dono == numero-1) return !poderes[numero-1].imuneAPropriaBomba;
    return modoRivais;
}
bool bombaFereInimigo(const Bomba& b) { return b.dono != DONO_INIMIGO; }
int donoDaChamaNoJogador(float l, float c, int numero) {
    return donoDaChama(l, c, [numero](const Bomba& b) { return bombaFereJogador(b, numero); });
}

// Corpo de 3x2 casas, medido na posicao atual como no estudo.
bool celulaBoss(int l, int c) {
    return boss.vivo && !hornetNoAr() && c+1>boss.visualC+.001f && c<boss.visualC+larguraBoss()-.001f &&
           l+1>boss.visualL+.001f && l<boss.visualL+2-.001f;
}
bool bloqueadoBoss(Posicao p) {
    if(hornetNoAr())return false;
    if (celulaBoss(p.linha,p.coluna)) return true;
    if (boss.investindo) return false; // Investida reserva so o corpo atual: permite esquiva.
    return boss.vivo && boss.movendo &&
        p.coluna>=min(boss.posicao.coluna,boss.destino.coluna) &&
        p.coluna<max(boss.posicao.coluna,boss.destino.coluna)+larguraBoss() &&
        p.linha>=min(boss.posicao.linha,boss.destino.linha) &&
        p.linha<max(boss.posicao.linha,boss.destino.linha)+2;
}
bool contatoComInimigo(float l, float c) {
    for (const Inimigo& inimigo : inimigos)
        if (inimigo.vivo && personagensEmContato(l,c,inimigo.visualL,inimigo.visualC)) return true;
    return boss.vivo && !hornetNoAr() && c+.75f>boss.visualC && c+.25f<boss.visualC+larguraBoss() &&
           l+.75f>boss.visualL && l+.25f<boss.visualL+2;
}

void derrubarJogador(int numero) {
    if (numero < 1 || numero > 2 || !jogadorEmCampo(numero)) return;
    if (!segundoJogadorAtivo()) { jogadoresAtingidos |= numero; estado = DERROTA; return; }
    int i = numero-1;
    if (imunidadeJogadorMs[i] > 0) return; // acabou de renascer
    jogadoresAtingidos |= numero;
    jogadorCaido[i] = true;
    tempoQuedaMs[i] = 0;
    vidasJogador[i] = max(0, vidasJogador[i]-1);
    ++quedasComSom;
    somMorteMs = DURACAO_SOM_MORTE_MS;
    // Bombas-relogio de quem caiu viram bombas comuns, senao ficariam no mapa.
    for (Bomba& b : bombas)
        if (b.dono == i && b.relogio && !b.explodindo) { b.relogio = false; b.tempoMs = TEMPO_BOMBA_MS; }
    if (vidasJogador[0] == 0 && vidasJogador[1] == 0) { estado = FIM_DE_JOGO; vidas = 0; }
}

void verificarContatoJogadores() {
    if (jogadorEmCampo(1) && contatoComInimigo(jogadorVisualL,jogadorVisualC)) derrubarJogador(1);
    if (jogadorEmCampo(2) && contatoComInimigo(jogador2VisualL,jogador2VisualC)) derrubarJogador(2);
}

// Celula ocupada por alguma bomba ativa (qualquer uma, agora que podem
// existir varias ao mesmo tempo) - usado como obstaculo para inimigos.
bool bombaEm(Posicao p) {
    for (const Bomba& b : bombas) if (b.ativa && iguais(b.posicao, p)) return true;
    return false;
}

// Limite por jogador: a bomba de um nao ocupa a vaga do outro.
int limiteBombas(int dono) { return 1 + poderes[dono].bombasExtras; }
int bombasDoDono(int dono) {
    int total = 0;
    for (const Bomba& b : bombas) if (b.ativa && b.dono == dono) total++;
    return total;
}
int alcanceDaBomba(const Bomba& b) { return b.dono == DONO_INIMIGO ? ALCANCE_BOMBA_INIMIGO : poderes[b.dono].alcance; }

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
    for (int l=1;l<LINHAS-2 && !localEncontrado;++l) for (int c=1;c<COLUNAS-larguraBoss();++c) {
        auto sobrepoe=[&](float jl,float jc){return jc+.85f>c && jc+.15f<c+larguraBoss() && jl+.85f>l && jl+.15f<l+2;};
        bool segredo=marioAliado.escondido && marioAliado.bloco.coluna>=c && marioAliado.bloco.coluna<c+larguraBoss() && marioAliado.bloco.linha>=l && marioAliado.bloco.linha<l+2;
        if (!segredo && (!marioAliado.ativo || marioAliado.hp<=0 || !sobrepoe(marioAliado.l,marioAliado.c)) && !sobrepoe(jogadorVisualL,jogadorVisualC) &&
            (!segundoJogadorAtivo() || !sobrepoe(jogador2VisualL,jogador2VisualC))) {boss.posicao={l,c};localEncontrado=true;break;}
    }
    boss.destino = boss.posicao;
    boss.visualL = static_cast<float>(boss.posicao.linha);
    boss.visualC = static_cast<float>(boss.posicao.coluna);
    for(int l=boss.posicao.linha;l<boss.posicao.linha+2;++l)
        for(int c=boss.posicao.coluna;c<boss.posicao.coluna+larguraBoss();++c) mapa[l][c]=VAZIO;
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
    somMorteMs = 0;
    jogadoresAtingidos = 0;
    jogadorCaido[0] = jogadorCaido[1] = false;
    tempoQuedaMs[0] = tempoQuedaMs[1] = 0;
    vidasJogador[0] = vidasJogador[1] = VIDAS_POR_JOGADOR;
    imunidadeJogadorMs[0] = imunidadeJogadorMs[1] = 0;
    fill(begin(teclaPressionada),end(teclaPressionada),false);
    // O A que reiniciou a fase ainda esta apertado: nao vira bomba ao nascer.
    botaoBombaControleAnterior = entradaControle.bomba;
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
    if (faseAtual == 8 || faseAtual == TOTAL_FASES) {
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
    // Facil: um inimigo solta bomba; Medio: metade; Dificil: 75%.
    int percentual = dificuldade == MEDIO ? 50 : 75;
    int comBomba = dificuldade == FACIL ? min(1, static_cast<int>(inimigos.size()))
                 : (static_cast<int>(inimigos.size()) * percentual + 50) / 100;
    vector<int> ordemInimigos(inimigos.size());
    for (size_t i = 0; i < ordemInimigos.size(); i++) ordemInimigos[i] = static_cast<int>(i);
    shuffle(ordemInimigos.begin(), ordemInimigos.end(), gerador);
    uniform_int_distribution<int> primeiraBomba(RECARGA_BOMBA_INIMIGO_MIN_MS, RECARGA_BOMBA_INIMIGO_MAX_MS);
    for (int i = 0; i < comBomba; i++) {
        Inimigo& inimigo = inimigos[ordemInimigos[i]];
        inimigo.soltaBomba = true;
        inimigo.recargaBombaMs = primeiraBomba(gerador);
    }
    itensNoChao.clear();
    bombas.clear();
    // Bonus resetam ao trocar de fase (regra explicita do enunciado).
    poderes[0] = poderes[1] = PoderesJogador{};
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
    estatisticas.movimentosFase = estatisticas.bombasFase = 0;
    montarFase();
}

// Comeca uma partida do zero (a partir do menu, ou "R" apos Game Over): zera
// fase, pontos, estatisticas e vidas - so essa funcao faz isso.
void novaPartida(ModoJogo novoModo, Dificuldade novaDificuldade) {
    modo = novoModo;
    if(novoModo==AUTOMATICO)doisJogadores=false;
    dificuldade = novaDificuldade;
    telaMenu = MENU_PRINCIPAL;
    faseAtual = faseSelecionada;
    pontos = 0;
    pontosJogador[0] = pontosJogador[1] = 0;
    vidas = VIDAS_BASE;
    estatisticas = EstatisticasPartida{};
    registroSalvo = false;
    erroSalvarRanking = false;
    botaoBombaControleAnterior=entradaControle.bomba;
    tempoPartidaMs = 0;
    montarFase();
}

void coletarItensEm(Posicao pos, int dono = 0) {
    for (size_t i = 0; i < itensNoChao.size(); i++) {
        if (!iguais(itensNoChao[i].posicao, pos)) continue;
        PoderesJogador& p = poderes[dono];
        switch (itensNoChao[i].tipo) {
            case CHUTE: p.cartaChute = true; break;
            case TELEPORTE: p.cartaTeleporte = true; break;
            case RAIO: if (p.alcance < ALCANCE_EXPLOSAO_MAXIMO) p.alcance++; break;
            case BOMBA_EXTRA: p.bombasExtras++; break;
            // Com dois jogadores as vidas sao de cada um (vidasJogador).
            case VIDA_EXTRA: if (segundoJogadorAtivo()) vidasJogador[dono]++; else vidas++; break;
            case BOMBA_RELOGIO: p.bombaRelogio = true; break;
            case IMUNE_BOMBA: p.imuneAPropriaBomba = true; break;
            case ATRAVESSA_BLOCO: p.atravessaBloco = true; break;
        }
        itensNoChao.erase(itensNoChao.begin() + i);
        estatisticas.itensColetados++;
        ajustarPontos(PONTOS_POR_ITEM, dono);
        return;
    }
}

void coletarItens() { coletarItensEm(jogador); }

// Compartilhados entre moverJogador (P1) e moverJogador2 (P2, cooperativo):
// os dois times "um so" - mesma vida, pontos e progresso de fase.
bool podeAndarPara(Posicao destino, int dono = 0) {
    if (bloqueadoBoss(destino) || bombaEm(destino)) return false;
    return livre(destino.linha, destino.coluna) ||
           (poderes[dono].atravessaBloco && dentro(destino.linha, destino.coluna) &&
            mapa[destino.linha][destino.coluna] == PAREDE_FRAGIL);
}

bool podeInverterPasso(Posicao alvo, float l, float c, int dl, int dc, int dono) {
    // Apenas voltar no mesmo segmento; curvas esperam o centro da casa.
    if (!((dl == 0 && l == alvo.linha && dc * (alvo.coluna-c) < 0) ||
          (dc == 0 && c == alvo.coluna && dl * (alvo.linha-l) < 0))) return false;
    Posicao volta{alvo.linha+dl, alvo.coluna+dc};
    if (podeAndarPara(volta, dono)) return true;
    // Ainda nao terminou de sair da propria bomba: pode cancelar a saida.
    // Depois de chegar em outra casa, esta excecao deixa de se aplicar.
    if (!iguais(volta,origemPassoJogador[dono]) || bloqueadoBoss(volta) ||
        !(livre(volta.linha,volta.coluna) ||
          (poderes[dono].atravessaBloco && dentro(volta.linha,volta.coluna) &&
           mapa[volta.linha][volta.coluna] == PAREDE_FRAGIL))) return false;
    for (const Bomba& b : bombas)
        if (b.ativa && iguais(b.posicao,volta)) return b.dono == dono && !b.explodindo;
    return false;
}

void atingirSePerigoso(float l, float c, int numero = 1) {
    if (jogadorEmCampo(numero) && (contatoComInimigo(l,c) || donoDaChamaNoJogador(l,c,numero) >= 0)) derrubarJogador(numero);
}

void avancarOuVencerSePortal(Posicao pos, int dono = 0) {
    if (estado == JOGANDO && mapa[pos.linha][pos.coluna] == PORTAL) {
        ajustarPontos(max(0,PONTOS_POR_FASE-estatisticas.movimentosFase*CUSTO_POR_MOVIMENTO-estatisticas.bombasFase*CUSTO_POR_BOMBA), dono);
        if (faseAtual < TOTAL_FASES) avancarFase();
        else estado = VITORIA;
    }
}

void moverJogador(int dl, int dc) {
    if (estado != JOGANDO || jogadorCaido[0]) return;
    bool emPasso = !visualNoDestino(jogadorVisualL,jogadorVisualC,jogador);
    if (emPasso) {
        if (!podeInverterPasso(jogador,jogadorVisualL,jogadorVisualC,dl,dc,0)) return;
    } else {
        if (tempoMovimentoMs > 0) return;
        origemPassoJogador[0] = jogador;
    }
    direcaoJogador = {dl, dc};
    Posicao origem = jogador;
    Posicao destino{jogador.linha + dl, jogador.coluna + dc};
    if (emPasso || podeAndarPara(destino, 0)) jogador = destino;
    if (!iguais(origem, jogador)) {
        estatisticas.movimentos++;
        estatisticas.movimentosFase++;
        estatisticas.movimentosJogador[0]++;
        quadroAnimJogador = (quadroAnimJogador + 1) % QUADROS_POR_DIRECAO;
    }
    atingirSePerigoso(jogadorVisualL,jogadorVisualC,1);
    tempoMovimentoMs = iguais(origem,jogador) || emPasso ? 0 : INTERVALO_MOVIMENTO_MS;
}

// Jogador 2 (cooperativo): mesmo comportamento do jogador 1, so que grava em
// jogador2/direcaoJogador2/tempoMovimentoMs2. Vida, pontos, itens e fase sao
// os mesmos do time (atingirSePerigoso/coletarItensEm/avancarOuVencerSePortal
// ja sao genericos o bastante pra servir aos dois).
void moverJogador2(int dl, int dc) {
    if (estado != JOGANDO || jogadorCaido[1]) return;
    bool emPasso = !visualNoDestino(jogador2VisualL,jogador2VisualC,jogador2);
    if (emPasso) {
        if (!podeInverterPasso(jogador2,jogador2VisualL,jogador2VisualC,dl,dc,1)) return;
    } else {
        if (tempoMovimentoMs2 > 0) return;
        origemPassoJogador[1] = jogador2;
    }
    direcaoJogador2 = {dl, dc};
    Posicao origem = jogador2;
    Posicao destino{jogador2.linha + dl, jogador2.coluna + dc};
    if (emPasso || podeAndarPara(destino, 1)) jogador2 = destino;
    if (!iguais(origem, jogador2)) {
        estatisticas.movimentos++;
        estatisticas.movimentosFase++;
        estatisticas.movimentosJogador[1]++;
        quadroAnimJogador2 = (quadroAnimJogador2 + 1) % QUADROS_POR_DIRECAO;
    }
    atingirSePerigoso(jogador2VisualL,jogador2VisualC,2);
    tempoMovimentoMs2 = iguais(origem,jogador2) || emPasso ? 0 : INTERVALO_MOVIMENTO_MS;
}

void iniciarExplosaoBomba(Bomba& b);

void colocarBombaEm(Posicao pos, int dono = 0) {
    if (dono != 0 && dono != 1) return;
    if (estado != JOGANDO || bombasDoDono(dono) >= limiteBombas(dono) || bombaEm(pos)) return;
    if (jogadorCaido[dono]) return;
    Bomba nova;
    nova.posicao = pos;
    nova.ativa = true;
    nova.tempoMs = TEMPO_BOMBA_MS;
    nova.relogio = poderes[dono].bombaRelogio;
    nova.dono = dono;
    bombas.push_back(nova);
    estatisticas.bombasUsadas++;
    estatisticas.bombasFase++;
    estatisticas.bombasJogador[dono]++;
}

// O destino logico e reservado no inicio do passo. A bomba fica na casa
// de saida ate o corpo terminar de chegar, em qualquer direcao.
Posicao casaParaBomba(float visualL, float visualC, Posicao destino, int dono) {
    return visualNoDestino(visualL,visualC,destino) ? destino : origemPassoJogador[dono];
}

void colocarBomba() { colocarBombaEm(casaParaBomba(jogadorVisualL, jogadorVisualC, jogador, 0), 0); }
void colocarBomba2() { colocarBombaEm(casaParaBomba(jogador2VisualL, jogador2VisualC, jogador2, 1), 1); }

// Bomba-relogio (item): nao explode sozinha, so quando o jogador aperta a
// tecla de detonar - todas as bombas-relogio ativas DESSE jogador explodem
// juntas, como no Bomberman classico; as do outro jogador nao.
void detonarBombasRelogio(int dono = 0) {
    for (Bomba& b : bombas)
        if (b.ativa && !b.explodindo && b.relogio && b.dono == dono) iniciarExplosaoBomba(b);
}

void chutarBomba(int dono = 0) {
    if (estado != JOGANDO || jogadorCaido[dono] || !poderes[dono].cartaChute) return;
    if (dono == 1 && !segundoJogadorAtivo()) return;
    const Posicao& pos = dono == 0 ? jogador : jogador2;
    const Posicao& direcao = dono == 0 ? direcaoJogador : direcaoJogador2;
    Posicao esperado{pos.linha + direcao.linha, pos.coluna + direcao.coluna};
    for (Bomba& b : bombas) {
        if (!b.ativa || b.explodindo || !iguais(b.posicao, esperado)) continue;
        Posicao destinoFinal = b.posicao;
        for (int passo = 1; passo <= ALCANCE_CHUTE; passo++) {
            Posicao tentativa{b.posicao.linha + direcao.linha * passo,
                              b.posicao.coluna + direcao.coluna * passo};
            if (!livre(tentativa.linha, tentativa.coluna) || temInimigo(tentativa)) break;
            destinoFinal = tentativa;
        }
        b.posicao = destinoFinal;
        poderes[dono].cartaChute = false;
        return;
    }
}

void teletransportarJogador(int dono = 0) {
    if (estado != JOGANDO || jogadorCaido[dono] || !poderes[dono].cartaTeleporte) return;
    if (dono == 1 && !segundoJogadorAtivo()) return;
    const Posicao& outro = dono == 0 ? jogador2 : jogador;
    vector<Posicao> livres;
    for (int l = 1; l < LINHAS - 1; l++)
        for (int c = 1; c < COLUNAS - 1; c++)
            if (livre(l, c) && !temInimigo({l, c}) && !bombaEm({l, c}) &&
                !(jogadorEmCampo(2 - dono) && iguais({l, c}, outro)))
                livres.push_back({l, c});
    if (livres.empty()) return;
    uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
    Posicao& pos = dono == 0 ? jogador : jogador2;
    pos = livres[sorteio(gerador)];
    // Teleporte "pula" de verdade - a posicao visual tem que acompanhar na
    // hora, nao deslizar visivelmente atravessando o mapa inteiro.
    float& visualL = dono == 0 ? jogadorVisualL : jogador2VisualL;
    float& visualC = dono == 0 ? jogadorVisualC : jogador2VisualC;
    visualL = static_cast<float>(pos.linha);
    visualC = static_cast<float>(pos.coluna);
    poderes[dono].cartaTeleporte = false;
    atingirSePerigoso(visualL, visualC, dono + 1);
    coletarItensEm(pos, dono);
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
// bloco fragil), aqui o mesmo alcance do fogo (poderes[dono].alcance) tambem conta quantos blocos
// frageis em sequencia a chama consegue atravessar/destruir.
void libertarMario(Posicao p, int dono = 0) {
    if(!marioAliado.escondido || !iguais(p,marioAliado.bloco))return;
    marioAliado.escondido=false;marioAliado.ativo=true;marioAliado.hp=3;
    // No modo inimigos ele fica do lado de quem o libertou primeiro.
    marioAliado.dono=modoRivais && segundoJogadorAtivo() && (dono==0 || dono==1) ? dono : -1;
    marioAliado.posicao=p;marioAliado.l=float(p.linha);marioAliado.c=float(p.coluna);
    marioAliado.imunidade=2.f; // A bomba que liberta nao machuca o aliado.
}
void propagarExplosao(vector<Posicao>& celulas, int dl, int dc, Posicao posAtual, int alcanceRestante, int dono = 0) {
    if (alcanceRestante <= 0) return;
    Posicao proxima{posAtual.linha + dl, posAtual.coluna + dc};
    if (!dentro(proxima.linha, proxima.coluna) || mapa[proxima.linha][proxima.coluna] == PAREDE_SOLIDA) return;
    celulas.push_back(proxima);
    if (mapa[proxima.linha][proxima.coluna] == PAREDE_FRAGIL) {
        mapa[proxima.linha][proxima.coluna] = VAZIO;
        bool libertando=marioAliado.escondido && iguais(proxima,marioAliado.bloco);
        libertarMario(proxima, dono);
        if(marioAliado.ativo && !libertando && gerador()%3==0)macasNoChao.push_back(proxima);
        invalidarCacheMapa();
        // Caixa quebrada por bomba de inimigo nao rende pontos a ninguem.
        if (dono != DONO_INIMIGO) { estatisticas.caixasDestruidas++; ajustarPontos(PONTOS_POR_CAIXA, dono); }
    } else if (mapa[proxima.linha][proxima.coluna] == PAREDE_BONUS) {
        mapa[proxima.linha][proxima.coluna] = VAZIO;
        destruirBlocoBonus(proxima.linha, proxima.coluna);
        invalidarCacheMapa();
        if (dono != DONO_INIMIGO) { estatisticas.caixasDestruidas++; ajustarPontos(PONTOS_POR_CAIXA, dono); }
    }
    propagarExplosao(celulas, dl, dc, proxima, alcanceRestante - 1, dono);
}

void matarInimigo(Inimigo& inimigo, int& pontuacao, int dono = -1) {
    inimigo.vivo = false;
    pontuacao += PONTOS_POR_INIMIGO;
    if (dono == 0 || dono == 1) pontosJogador[dono] += PONTOS_POR_INIMIGO;
    estatisticas.inimigosAbatidos++;
}

void atingirPersonagens(vector<Inimigo>& listaInimigos,
                        EstadoJogo& estadoAtual, int& pontuacao) {
    // Os dados recebidos por referencia sao alterados diretamente.
    bool emJogo = estadoAtual == JOGANDO;
    int donoP1 = jogadorEmCampo(1) ? donoDaChamaNoJogador(jogadorVisualL,jogadorVisualC,1) : -1;
    int donoP2 = jogadorEmCampo(2) ? donoDaChamaNoJogador(jogador2VisualL,jogador2VisualC,2) : -1;
    // Modo inimigos: derrubar o rival vale como derrubar um inimigo.
    if (emJogo && donoP1 == 1) { pontuacao += PONTOS_POR_INIMIGO; pontosJogador[1] += PONTOS_POR_INIMIGO; }
    if (emJogo && donoP2 == 0) { pontuacao += PONTOS_POR_INIMIGO; pontosJogador[0] += PONTOS_POR_INIMIGO; }
    if (donoP1 >= 0) derrubarJogador(1);
    if (donoP2 >= 0) derrubarJogador(2);
    if (estado == DERROTA) estadoAtual = DERROTA;
    for (Inimigo& inimigo : listaInimigos) {
        if (!inimigo.vivo) continue;
        int dono = donoDaChama(inimigo.visualL,inimigo.visualC,bombaFereInimigo);
        if (dono >= 0) matarInimigo(inimigo, pontuacao, dono);
    }
}

// Separado de atingirPersonagens porque o boss tem vida (varios acertos) em
// vez de morrer no primeiro toque. O flag "bossAtingido" fica na propria
// bomba (nao mais global) porque agora varias bombas podem estar explodindo
// ao mesmo tempo; cada explosao conta no maximo 1 acerto, mesmo durando
// varios ticks de logica seguidos.
void concluirLutaBoss(int dono = -1){
    boss.vivo=false;
    boss.morteMs=bossAnimado()?1050:0;
    boss.relogioPoseMs=0;boss.acaoEspecial=0;boss.tempoEspecialMs=0;
    boss.movendo=false;boss.investindo=false;boss.avisoInvestidaMs=0;
    boss.alturaHornet=0;
    bossConcluido=boss.morteMs==0;
    ajustarPontos(PONTOS_POR_BOSS, dono);
    if(bossConcluido)colocarPortal();
}
void entrarEtapaHornet(EtapaHornet etapa,int duracao){
    boss.etapaHornet=etapa;boss.tempoEtapaHornetMs=duracao;boss.duracaoEtapaHornetMs=duracao;
}
void reagirDanoHornet(){
    if(indiceBoss()!=2 || boss.vida<=0)return;
    boss.contraPendenteHornet=boss.contraPendenteHornet || boss.etapaHornet==H_GUARDA;
    // Termina o deslocamento no destino antes da recuperacao, sem teleportar.
    if(boss.movendo || hornetNoAr())return;
    entrarEtapaHornet(H_DANO,480);
}
void atingirBoss() {
    if (!boss.vivo || boss.imunidadeMs>0) return;
    for (Bomba& b : bombas) {
        if (!b.explodindo || b.bossAtingido || b.dono == DONO_INIMIGO) continue;
        bool atingiu = false;
        for (const Posicao& p : b.celulasExplosao) if (celulaBoss(p.linha,p.coluna)) { atingiu = true; break; }
        if (!atingiu) continue;
        b.bossAtingido = true;
        boss.vida=max(0,boss.vida-4);
        boss.imunidadeMs=IMUNIDADE_BOSS_MS;
        reagirDanoHornet();
        if (boss.vida <= 0) {
            concluirLutaBoss(b.dono);
        }
        break;
    }
}

void iniciarExplosaoBomba(Bomba& b) {
    b.explodindo = true;
    b.tempoMs = TEMPO_EXPLOSAO_MS;
    b.celulasExplosao.clear();
    b.celulasExplosao.push_back(b.posicao);
    int alcance = alcanceDaBomba(b);
    propagarExplosao(b.celulasExplosao, -1, 0, b.posicao, alcance, b.dono);
    propagarExplosao(b.celulasExplosao, 1, 0, b.posicao, alcance, b.dono);
    propagarExplosao(b.celulasExplosao, 0, -1, b.posicao, alcance, b.dono);
    propagarExplosao(b.celulasExplosao, 0, 1, b.posicao, alcance, b.dono);
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
    if (!jogadorEmCampo(2)) return jogador;
    if (!jogadorEmCampo(1)) return jogador2;
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
            Posicao alvo = alvoDePerseguicaoPara(inimigo.posicao);
            int distancia = abs(inimigo.posicao.linha - alvo.linha) + abs(inimigo.posicao.coluna - alvo.coluna);
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

bool celulaNoAlcance(Posicao origemBomba, Posicao p, int alcance);
// Bombas de inimigos (Medio/Dificil): quem tem a habilidade solta uma bomba
// quando um jogador esta na linha da explosao. Ela fere so os jogadores (e o
// Mario), com alcance fixo, e cada inimigo espera a recarga antes da proxima.
bool jogadorNaMira(Posicao origem, Posicao alvo) {
    return !iguais(origem, alvo) && celulaNoAlcance(origem, alvo, ALCANCE_BOMBA_INIMIGO);
}
void soltarBombasInimigos(int tempoMs) {
    uniform_int_distribution<int> recarga(RECARGA_BOMBA_INIMIGO_MIN_MS, RECARGA_BOMBA_INIMIGO_MAX_MS);
    for (Inimigo& inimigo : inimigos) {
        if (!inimigo.vivo || !inimigo.soltaBomba) continue;
        inimigo.recargaBombaMs = max(0, inimigo.recargaBombaMs - tempoMs);
        if (inimigo.recargaBombaMs > 0 || estado != JOGANDO ||
            !visualNoDestino(inimigo.visualL, inimigo.visualC, inimigo.posicao) || bombaEm(inimigo.posicao)) continue;
        bool alvo = (jogadorEmCampo(1) && jogadorNaMira(inimigo.posicao, jogador)) ||
                    (jogadorEmCampo(2) && jogadorNaMira(inimigo.posicao, jogador2));
        if (!alvo) continue;
        Bomba nova;
        nova.posicao = inimigo.posicao;
        nova.ativa = true;
        nova.tempoMs = TEMPO_BOMBA_MS;
        nova.dono = DONO_INIMIGO;
        bombas.push_back(nova);
        inimigo.recargaBombaMs = recarga(gerador);
    }
}

// Limpa somente o interior percorrido; bordas e surpresa do Mario permanecem.
void abrirPassagemBoss(Posicao alvo) {
    int top=min(alvo.linha,boss.posicao.linha),bottom=max(alvo.linha,boss.posicao.linha)+2;
    int left=min(alvo.coluna,boss.posicao.coluna),right=max(alvo.coluna,boss.posicao.coluna)+larguraBoss();
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
    if(alvo.linha<1 || alvo.coluna<1 || alvo.linha+2>LINHAS-1 || alvo.coluna+larguraBoss()>COLUNAS-1) return false;
    int top=min(alvo.linha,boss.posicao.linha), bottom=max(alvo.linha,boss.posicao.linha)+2;
    int left=min(alvo.coluna,boss.posicao.coluna), right=max(alvo.coluna,boss.posicao.coluna)+larguraBoss();
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
// Antecipacao discreta da trajetoria: 15% do tempo estimado de voo.
// Usa somente deslocamento real em curso, nunca a ultima direcao parada.
pair<float,float> anteciparAlvoBoss(float l,float c,float origemL,float origemC,float velocidade){
    float dl=0,dc=0;
    if(abs(l-jogadorVisualL)<.001f && abs(c-jogadorVisualC)<.001f){dl=jogador.linha-l;dc=jogador.coluna-c;}
    else if(segundoJogadorAtivo() && abs(l-jogador2VisualL)<.001f && abs(c-jogador2VisualC)<.001f){dl=jogador2.linha-l;dc=jogador2.coluna-c;}
    else if(marioAliado.ativo && abs(l-marioAliado.l)<.001f && abs(c-marioAliado.c)<.001f){dl=marioAliado.posicao.linha-l;dc=marioAliado.posicao.coluna-c;}
    float n=hypot(dl,dc);
    float avanco=min(.75f,hypot(l+.5f-origemL,c+.5f-origemC)/velocidade*.15f*VELOCIDADE_VISUAL_CELS_SEG);
    if(n>.001f){l+=dl/n*avanco;c+=dc/n*avanco;}
    return {clamp(l,1.f,float(LINHAS-2)),clamp(c,1.f,float(COLUNAS-2))};
}
int direcaoPerseguicaoBoss(){
    float dl=jogadorVisualL+.5f-(boss.visualL+1),dc=jogadorVisualC+.5f-(boss.visualC+larguraBoss()/2.f);
    return abs(dc)>=abs(dl)?(dc>=0?1:3):(dl>=0?2:0);
}
bool prepararInvestidaBoss() {
    if(boss.movendo || boss.investindo)return false;
    float alvoL=jogadorVisualL+.5f,alvoC=jogadorVisualC+.5f;
    float origemL=boss.visualL+1,origemC=boss.visualC+larguraBoss()/2.f;
    auto considerar=[&](float l,float c){if(hypot(l+.5f-origemL,c+.5f-origemC)<hypot(alvoL-origemL,alvoC-origemC)){alvoL=l+.5f;alvoC=c+.5f;}};
    if(segundoJogadorAtivo())considerar(jogador2VisualL,jogador2VisualC);
    if(marioAliado.ativo && marioAliado.hp>0)considerar(marioAliado.l,marioAliado.c);
    int dl=0,dc=0;
    if(abs(alvoC-origemC)>=abs(alvoL-origemL))dc=alvoC>=origemC?1:-1;
    else dl=alvoL>=origemL?1:-1;
    Posicao destino=boss.posicao;
    for(int passo=1;passo<=4;++passo){Posicao p{boss.posicao.linha+dl*passo,boss.posicao.coluna+dc*passo};
        if(p.linha<1 || p.coluna<1 || p.linha+2>LINHAS-1 || p.coluna+larguraBoss()>COLUNAS-1)break;
        if(marioAliado.escondido && marioAliado.bloco.linha>=p.linha && marioAliado.bloco.linha<p.linha+2 && marioAliado.bloco.coluna>=p.coluna && marioAliado.bloco.coluna<p.coluna+larguraBoss())break;
        destino=p;
    }
    if(iguais(destino,boss.posicao))return false;
    boss.destino=destino;boss.investindo=true;boss.avisoInvestidaMs=1000;
    return true;
}
void criarPoderBoss() {
    boss.animAtaqueMs=700;
    ++boss.sequenciaAtaque;
    if(indiceBoss()==2){
        if(boss.etapaHornet!=H_IDLE || boss.movendo){--boss.sequenciaAtaque;return;}
        boss.acaoEspecial=(boss.sequenciaAtaque-1)%5+1;
        boss.olhandoDireita=jogadorVisualC>boss.visualC;
        boss.tempoProximoAtaqueMs=INTERVALO_ATAQUE_BOSS_MS;
        switch(boss.acaoEspecial){
            case 1:entrarEtapaHornet(H_PREP_AGULHA,750);break;
            case 2:case 4:
                if(prepararInvestidaBoss())entrarEtapaHornet(boss.acaoEspecial==2?H_PREP_DASH:H_PREP_AEREO,1000);
                else {boss.acaoEspecial=0;boss.tempoProximoAtaqueMs=1000;}
                break;
            case 3:entrarEtapaHornet(H_PREP_SALTO,350);break;
            case 5:entrarEtapaHornet(H_GUARDA,1200);break;
        }
        return;
    }
    auto lancar=[&](float l,float c,PoderBoss poder,float desvio=0.f){
        float origemL=boss.visualL+.8f,origemC=boss.visualC+larguraBoss()/2.f;
        float velocidade=poder==MACHADO?2.8f:poder==ENERGIA?3.6f:3.f;
        auto alvo=anteciparAlvoBoss(l,c,origemL,origemC,velocidade);
        float angulo=atan2(alvo.first+.5f-origemL,alvo.second+.5f-origemC)+desvio*.85f;
        projeteisBoss.push_back({origemL,origemC,sin(angulo)*velocidade,cos(angulo)*velocidade,0,4.5f*FATOR_FORCA_BOSS,poder,700});
        if(poder==MACHADO)projeteisBoss.back().arma=boss.sequenciaAtaque%2;
    };
    auto participantes=[&](auto atacar){
        atacar(jogadorVisualL,jogadorVisualC);
        if(segundoJogadorAtivo())atacar(jogador2VisualL,jogador2VisualC);
        if(marioAliado.ativo && marioAliado.hp>0)atacar(marioAliado.l,marioAliado.c);
    };
    if(bossAnimado()) {
        boss.acaoEspecial=(boss.sequenciaAtaque-1)%(indiceBoss()==2?3:4)+1;
        boss.relogioPoseMs=0;
        boss.olhandoDireita=jogadorVisualC>boss.visualC;
        if(boss.acaoEspecial==1){
            // Pose de preparacao precede o voo; a agulha regressa pelo mesmo fio.
            boss.tempoEspecialMs=1500;
            participantes([&](float l,float c){
                if(indiceBoss()==2)lancar(l,c,AGULHA);
                else for(float desvio:{-.20f,0.f,.20f})lancar(l,c,FOGO_BOWSER,desvio);
            });
        } else if(boss.acaoEspecial==2){
            boss.tempoEspecialMs=2200;
            if(boss.movendo)boss.investidaPendente=true;else prepararInvestidaBoss();
        } else if(boss.acaoEspecial==3){
            // Salto anunciado; a aterrissagem libera sprites em anel, com espacos.
            boss.tempoEspecialMs=1800;
            boss.tempoProximoMovimentoMs=2200;
        } else {
            boss.tempoEspecialMs=1500;
            boss.tempoProximoMovimentoMs=1800;
        }
        return;
    }
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
            // Campanha reforcada; o executavel Insano conserva o estudo.
            participantes([&](float l,float c){float origemL=boss.visualL+.8f,origemC=boss.visualC+1.5f;
                auto alvo=anteciparAlvoBoss(l,c,origemL,origemC,3.2f);
                float dl=alvo.first+.5f-origemL,dc=alvo.second+.5f-origemC,n=max(.1f,hypot(dl,dc));
                fogosBoss.push_back({origemL,origemC,dl/n*3.2f,dc/n*3.2f,4.f*FATOR_FORCA_BOSS});});break;
    }
}
void terminarProjetilBoss(ProjetilBoss& p) {
    impactosBoss.push_back({p.l,p.c,0,p.poder});p.vida=0;
    int l=int(p.l),c=int(p.c);
    if(p.poder==VENENO && dentro(l,c) && mapa[l][c]==VAZIO)
        areasBoss.push_back({{l,c},0,2200,VENENO});
}
void recolherAgulha(ProjetilBoss& p){
    if(!p.retornando){p.retornando=true;p.vida=3.f;}
}
void atualizarProjeteisBoss(int tempoMs) {
    float dt=tempoMs/1000.f;
    for(auto& i:impactosBoss)i.idade+=dt;
    for(auto& p:projeteisBoss){
        float restante=dt;
        if(p.avisoMs>0){int aviso=p.avisoMs;p.avisoMs=max(0,p.avisoMs-tempoMs);restante=max(0.f,(tempoMs-aviso)/1000.f);}
        // Subpassos impedem atravessar paredes/corpos quando um quadro atrasa.
        while(restante>0 && p.vida>0){float passo=min(restante,.025f);restante-=passo;
            float antesL=p.l,antesC=p.c;
            if(p.poder==AGULHA){
                if(!p.retornando && p.idade+passo>=1.1f*FATOR_FORCA_BOSS)recolherAgulha(p);
                if(p.retornando){float dl=p.origemL-p.l,dc=p.origemC-p.c,n=hypot(dl,dc);
                    if(n<.12f){p.vida=0;break;}p.vl=dl/max(.01f,n)*3.f;p.vc=dc/max(.01f,n)*3.f;}
            }
            p.l+=p.vl*passo;p.c+=p.vc*passo;p.idade+=passo;p.vida-=passo;
            int l=int(p.l),c=int(p.c);
            if(!dentro(l,c)){p.vida=0;break;}
            if((mapa[l][c]==PAREDE_SOLIDA || (marioAliado.escondido && iguais({l,c},marioAliado.bloco))) && !(p.poder==AGULHA && p.retornando)){p.l=antesL;p.c=antesC;
                if(p.poder==AGULHA && !p.retornando){recolherAgulha(p);continue;}
                if(p.poder!=AGULHA || !p.retornando){terminarProjetilBoss(p);break;}}
            if(mapa[l][c]==PAREDE_FRAGIL || mapa[l][c]==PAREDE_BONUS){
                if(mapa[l][c]==PAREDE_BONUS)destruirBlocoBonus(l,c);
                mapa[l][c]=VAZIO;invalidarCacheMapa();
                if(p.poder==AGULHA && !p.retornando){recolherAgulha(p);continue;}
                terminarProjetilBoss(p);break;
            }
            auto tocou=[&](float l,float c){return hypot(p.l-l-.5f,p.c-c-.5f)<.48f;};
            bool acertou=false;
            if(jogadorEmCampo(1) && tocou(jogadorVisualL,jogadorVisualC)){derrubarJogador(1);acertou=true;}
            if(jogadorEmCampo(2) && tocou(jogador2VisualL,jogador2VisualC)){derrubarJogador(2);acertou=true;}
            if(marioAliado.ativo && marioAliado.hp>0 && tocou(marioAliado.l,marioAliado.c)){ferirMario();acertou=true;}
            if(acertou || p.vida<=0){terminarProjetilBoss(p);break;}
        }
    }
    projeteisBoss.erase(remove_if(projeteisBoss.begin(),projeteisBoss.end(),[](const ProjetilBoss& p){return p.vida<=0;}),projeteisBoss.end());
    impactosBoss.erase(remove_if(impactosBoss.begin(),impactosBoss.end(),[](const ImpactoBoss& i){return i.idade>=.4f;}),impactosBoss.end());
}
void soltarAgulhaHornet(){
    float l=boss.visualL+1,c=boss.visualC+1;
    auto alvo=anteciparAlvoBoss(jogadorVisualL,jogadorVisualC,l,c,3.f);
    float a=atan2(alvo.first+.5f-l,alvo.second+.5f-c);
    projeteisBoss.push_back({l,c,sin(a)*3.f,cos(a)*3.f,0,4.f,AGULHA,150});
    projeteisBoss.back().origemL=l;projeteisBoss.back().origemC=c;
}
bool moverHornet(float dt,float velocidade){
    float dl=boss.destino.linha-boss.visualL,dc=boss.destino.coluna-boss.visualC,n=hypot(dl,dc),passo=min(n,velocidade*dt);
    if(dc!=0)boss.olhandoDireita=dc>0;
    if(n>0){boss.visualL+=dl*passo/n;boss.visualC+=dc*passo/n;}
    if(n-passo>.0001f)return false;
    boss.posicao=boss.destino;boss.visualL=float(boss.posicao.linha);boss.visualC=float(boss.posicao.coluna);
    boss.movendo=false;boss.investindo=false;return true;
}
void soltarFiosHornet(int quantidade){
    float l=boss.visualL+1,c=boss.visualC+1;
    for(int i=0;i<quantidade;++i){float a=i*6.2831853f/quantidade;
        projeteisBoss.push_back({l,c,sin(a)*2.1f,cos(a)*2.1f,0,1.6f*FATOR_FORCA_BOSS,FIO,250});}
}
void atualizarHornet(int tempoMs){
    // Subpassos preservam a ordem dos eventos mesmo com um quadro atrasado.
    for(int restante=tempoMs;restante>0;){
        int passo=min(16,restante);restante-=passo;float dt=passo/1000.f;
        atualizarProjeteisBoss(passo);
        if(estado!=JOGANDO)return;
        boss.relogioPoseMs+=passo;boss.imunidadeMs=max(0,boss.imunidadeMs-passo);
        boss.tempoProximoAtaqueMs-=passo;
        if(boss.etapaHornet==H_IDLE){
            boss.olhandoDireita=jogadorVisualC+.5f>boss.visualC+1;
            boss.alturaHornet=0;boss.acaoEspecial=0;
            if(boss.tempoProximoAtaqueMs<=0){criarPoderBoss();continue;}
            if((boss.tempoProximoMovimentoMs-=passo)<=0){
                int inicio=gerador()%4;if(gerador()%100<15)inicio=direcaoPerseguicaoBoss();
                const int dl[]={-1,0,1,0},dc[]={0,1,0,-1};
                bool moveu=false;
                for(int k=0;k<4;++k){int d=(inicio+k)%4;if(iniciarAvancoBoss(dl[d],dc[d],1)){moveu=true;break;}}
                if(moveu)entrarEtapaHornet(H_CORRIDA,0);else boss.tempoProximoMovimentoMs=750;
            }
            continue;
        }
        if(boss.etapaHornet==H_CORRIDA){
            if(moverHornet(dt,2.1f)){entrarEtapaHornet(H_IDLE,0);boss.tempoProximoMovimentoMs=2400;}
            continue;
        }
        if(boss.etapaHornet==H_DASH || boss.etapaHornet==H_AEREO){
            bool aereo=boss.etapaHornet==H_AEREO;
            if(moverHornet(dt,dificuldade==FACIL?3.7f:4.4f)){
                if(!aereo)boss.alturaHornet=0;
                entrarEtapaHornet(aereo?H_RECUP_AEREO:H_RECUP_DASH,480);
            }else if(aereo)boss.alturaHornet=48;
            continue;
        }
        boss.tempoEtapaHornetMs=max(0,boss.tempoEtapaHornetMs-passo);
        if(boss.etapaHornet==H_PREP_DASH || boss.etapaHornet==H_PREP_AEREO)boss.avisoInvestidaMs=boss.tempoEtapaHornetMs;
        float t=1.f-float(boss.tempoEtapaHornetMs)/max(1,boss.duracaoEtapaHornetMs);
        if(boss.etapaHornet==H_SALTO)boss.alturaHornet=sin(t*1.5707963f)*65;
        else if(boss.etapaHornet==H_PREP_GIRO || boss.etapaHornet==H_GIRO)boss.alturaHornet=65;
        else if(boss.etapaHornet==H_POUSO)boss.alturaHornet=(1-t)*65;
        else if(boss.etapaHornet==H_PREP_AEREO)boss.alturaHornet=t*48;
        else if(boss.etapaHornet==H_RECUP_AEREO)boss.alturaHornet=(1-t)*48;
        if(boss.etapaHornet==H_VOO_AGULHA){
            bool voando=any_of(projeteisBoss.begin(),projeteisBoss.end(),[](const ProjetilBoss& p){return p.poder==AGULHA && p.vida>0;});
            if(!voando)boss.tempoEtapaHornetMs=0;
        }
        if(boss.tempoEtapaHornetMs>0)continue;
        switch(boss.etapaHornet){
            case H_PREP_AGULHA:entrarEtapaHornet(H_LANCAR,360);soltarAgulhaHornet();break;
            case H_LANCAR:entrarEtapaHornet(H_VOO_AGULHA,3000);break;
            case H_VOO_AGULHA:entrarEtapaHornet(H_RECOLHER,480);break;
            case H_PREP_DASH:case H_PREP_AEREO:{bool aereo=boss.etapaHornet==H_PREP_AEREO;
                boss.avisoInvestidaMs=0;abrirPassagemBoss(boss.destino);boss.movendo=true;
                entrarEtapaHornet(aereo?H_AEREO:H_DASH,0);break;}
            case H_PREP_SALTO:entrarEtapaHornet(H_SALTO,550);break;
            case H_SALTO:entrarEtapaHornet(H_PREP_GIRO,420);break;
            case H_PREP_GIRO:entrarEtapaHornet(H_GIRO,660);soltarFiosHornet(dificuldade==FACIL?6:8);break;
            case H_GIRO:entrarEtapaHornet(H_POUSO,500);break;
            case H_GUARDA:
                if(hypot(jogadorVisualL-boss.visualL-.5f,jogadorVisualC-boss.visualC-.5f)<3.f){entrarEtapaHornet(H_CONTRA,630);soltarFiosHornet(4);}
                else entrarEtapaHornet(H_RECUP_CONTRA,480);
                break;
            case H_CONTRA:entrarEtapaHornet(H_RECUP_CONTRA,480);break;
            case H_DANO:entrarEtapaHornet(H_RECUP_DANO,420);break;
            case H_RECUP_DANO:
                if(boss.contraPendenteHornet){boss.contraPendenteHornet=false;entrarEtapaHornet(H_CONTRA,630);soltarFiosHornet(4);break;}
                if(any_of(projeteisBoss.begin(),projeteisBoss.end(),[](const ProjetilBoss& p){return p.poder==AGULHA && p.vida>0;})){
                    entrarEtapaHornet(H_VOO_AGULHA,3000);break;}
                // Recuperacao comum abaixo.
                [[fallthrough]];
            default:boss.alturaHornet=0;entrarEtapaHornet(H_IDLE,0);
                boss.tempoProximoMovimentoMs=1200;boss.tempoProximoAtaqueMs=max(1000,boss.tempoProximoAtaqueMs);break;
        }
    }
}
void atualizarAreasBoss(int tempoMs){
    for(auto& area:areasBoss) {
        if(area.avisoMs>0){int restante=area.avisoMs-tempoMs;area.avisoMs=max(0,restante);if(restante<0)area.ativoMs+=restante;}
        else area.ativoMs-=tempoMs;
        if(area.avisoMs<=0 && area.ativoMs>0){
            if(marioAliado.ativo && hypot(marioAliado.l-area.posicao.linha,marioAliado.c-area.posicao.coluna)<.72f)ferirMario();
            if(jogadorEmCampo(1) && hypot(jogadorVisualL-area.posicao.linha,jogadorVisualC-area.posicao.coluna)<.72f)derrubarJogador(1);
            if(jogadorEmCampo(2) && hypot(jogador2VisualL-area.posicao.linha,jogador2VisualC-area.posicao.coluna)<.72f)derrubarJogador(2);
        }
    }
    areasBoss.erase(remove_if(areasBoss.begin(),areasBoss.end(),[](const AreaBoss& a){return a.ativoMs<=0;}),areasBoss.end());
}

void atualizarBoss(int tempoMs) {
    if(!boss.vivo) {
        if(boss.morteMs>0){boss.morteMs=max(0,boss.morteMs-tempoMs);boss.relogioPoseMs+=tempoMs;
            if(!boss.morteMs){bossConcluido=true;colocarPortal();}}
        return;
    }
    if(indiceBoss()==2){atualizarHornet(tempoMs);atualizarAreasBoss(tempoMs);return;}
    float dt=tempoMs/1000.f;
    boss.relogioPoseMs+=tempoMs;
    if(bossAnimado() && boss.tempoEspecialMs>0){
        int antes=boss.tempoEspecialMs;boss.tempoEspecialMs=max(0,antes-tempoMs);
        if(boss.acaoEspecial==3 && antes>400 && boss.tempoEspecialMs<=400){
            float l=boss.visualL+1,c=boss.visualC+larguraBoss()/2.f;
            for(int i=0;i<8;++i){float a=i*3.1415926f/4;
                projeteisBoss.push_back({l,c,sin(a)*2.4f,cos(a)*2.4f,0,1.8f*FATOR_FORCA_BOSS,indiceBoss()==2?FIO:FOGO_BOWSER,250});}
        }
        if(boss.acaoEspecial==4 && antes>750 && boss.tempoEspecialMs<=750){
            float l=boss.visualL+1,c=boss.visualC+1.5f;
            auto alvo=anteciparAlvoBoss(jogadorVisualL,jogadorVisualC,l,c,2.8f);
            float a=atan2(alvo.first+.5f-l,alvo.second+.5f-c);
            for(float desvio:{-.25f,0.f,.25f})projeteisBoss.push_back({l,c,sin(a+desvio*.85f)*2.8f,cos(a+desvio*.85f)*2.8f,0,2.f*FATOR_FORCA_BOSS,GOLPE_BOWSER,0});
        }
        if(!boss.tempoEspecialMs)boss.acaoEspecial=0;
    }
    boss.imunidadeMs=max(0,boss.imunidadeMs-tempoMs);
    boss.animAtaqueMs=max(0,boss.animAtaqueMs-tempoMs);
    if(!boss.movendo && !boss.investindo)boss.olhandoDireita=jogadorVisualC+.5f>boss.visualC+larguraBoss()/2.f;
    if(boss.avisoInvestidaMs>0){
        boss.avisoInvestidaMs=max(0,boss.avisoInvestidaMs-tempoMs);
        if(boss.avisoInvestidaMs==0){abrirPassagemBoss(boss.destino);boss.movendo=true;}
    } else if(boss.movendo) {
        float dl=boss.destino.linha-boss.visualL, dc=boss.destino.coluna-boss.visualC;
        float velocidade=boss.investindo?4.8f:indiceBoss()==2?2.1f:1.5f;
        float distancia=hypot(dl,dc), passo=min(distancia,dt*velocidade);
        if(dc!=0)boss.olhandoDireita=dc>0;
        if(distancia>0){boss.visualL+=dl*passo/distancia;boss.visualC+=dc*passo/distancia;}
        if(distancia-passo<.0001f){boss.posicao=boss.destino;boss.visualL=float(boss.posicao.linha);boss.visualC=float(boss.posicao.coluna);boss.movendo=false;boss.investindo=false;boss.tempoProximoMovimentoMs=INTERVALO_MOVIMENTO_BOSS_MS;}
    } else if(!(bossAnimado() && boss.tempoEspecialMs>0) && (boss.tempoProximoMovimentoMs-=tempoMs)<=0) {
        int dl[4]={-1,0,1,0},dc[4]={0,1,0,-1};
        int preferido=boss.horizontal?(gerador()%2?1:3):(gerador()%2?0:2), outro=boss.horizontal?0:1;
        if(gerador()%100<15){preferido=direcaoPerseguicaoBoss();outro=preferido%2?0:1;}
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
    atualizarAreasBoss(tempoMs);
    for(auto& f:fogosBoss){
        f.l+=f.vl*dt;f.c+=f.vc*dt;f.vida-=dt;
        int l=int(f.l),c=int(f.c);
        if(marioAliado.ativo && marioAliado.hp>0 && hypot(f.l-marioAliado.l-.5f,f.c-marioAliado.c-.5f)<.5f){ferirMario();f.vida=0;}
        if(!dentro(l,c))f.vida=0;
        else if(mapa[l][c]==PAREDE_SOLIDA && l>2)f.vida=0;
        else if(marioAliado.escondido && iguais({l,c},marioAliado.bloco)){f.vida=0;}
        else if(mapa[l][c]==PAREDE_FRAGIL || mapa[l][c]==PAREDE_BONUS){mapa[l][c]=VAZIO;f.vida=0;invalidarCacheMapa();}
        if(jogadorEmCampo(1) && hypot(f.l-jogadorVisualL-.5f,f.c-jogadorVisualC-.5f)<.5f){derrubarJogador(1);f.vida=0;}
        if(jogadorEmCampo(2) && hypot(f.l-jogador2VisualL-.5f,f.c-jogador2VisualC-.5f)<.5f){derrubarJogador(2);f.vida=0;}
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
bool celulaNoAlcance(Posicao origemBomba, Posicao p, int alcance = -1) {
    if (alcance < 0) alcance = poderes[0].alcance; // so o bot (P1) simula bombas
    if (iguais(p, origemBomba)) return true;
    int dl = p.linha - origemBomba.linha;
    int dc = p.coluna - origemBomba.coluna;
    if (dl != 0 && dc != 0) return false;
    int distancia = abs(dl) + abs(dc);
    if (distancia > alcance) return false;
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
    for (const Bomba& b : bombas) if (b.ativa && celulaNoAlcance(b.posicao, p, alcanceDaBomba(b))) return true;
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
        for (int distancia = 1; distancia <= poderes[0].alcance; distancia++) {
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
        if(p.linha>=top && p.linha<max(boss.posicao.linha,boss.destino.linha)+2 && p.coluna>=left && p.coluna<max(boss.posicao.coluna,boss.destino.coluna)+larguraBoss())return true;}
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
    // No modo inimigos ele e de um so jogador: cair nao encerra a rodada.
    // Com dois jogadores (cada um com suas vidas) a queda dele nao encerra a fase.
    if(marioAliado.hp==0){if(segundoJogadorAtivo())marioAliado.ativo=false;else{estado=DERROTA;jogadoresAtingidos|=4;}}
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
    // Modo inimigos: o jogador rival de quem libertou o Mario tambem e alvo.
    if(m.dono>=0 && jogadorEmCampo(2-m.dono)){float rl=m.dono==0?jogador2VisualL:jogadorVisualL,rc=m.dono==0?jogador2VisualC:jogadorVisualC;
        Posicao rp=m.dono==0?jogador2:jogador;float d=hypot(rc-m.c,rl-m.l);
        if(d<maisPerto){maisPerto=d;tx=rp.coluna+.5f;ty=rp.linha+.5f;}}
    if(boss.vivo && hypot(m.c-boss.visualC-1,m.l-boss.visualL)<maisPerto){tx=boss.visualC+larguraBoss()/2.f;ty=boss.visualL+1;}
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
        if(m.movendo>=0){float dl=m.posicao.linha-m.l,dc=m.posicao.coluna-m.c,n=hypot(dl,dc),passo=min(n,4.2f*FATOR_MARIO*dt);
            if(n>0){m.l+=dl*passo/n;m.c+=dc*passo/n;}m.distancia+=passo;m.andando=passo>0;
            if(n-passo<.0001f){m.l=float(m.posicao.linha);m.c=float(m.posicao.coluna);m.movendo=-1;}}
        if(corpoNaExplosao(m.l,m.c))ferirMario();
        if(boss.investindo && contatoComInimigo(m.l,m.c))ferirMario();
        for(const auto& i:inimigos)if(i.vivo && hypot(m.l-i.visualL,m.c-i.visualC)<.55f)ferirMario();
        for(auto it=macasNoChao.begin();it!=macasNoChao.end();)if(iguais(*it,m.posicao)){it=macasNoChao.erase(it);m.recarga=0;}else ++it;
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
            mapa[p.linha][p.coluna]=VAZIO;invalidarCacheMapa();estatisticas.caixasDestruidas++;ajustarPontos(PONTOS_POR_CAIXA,marioAliado.dono);acertou=true;}
        for(auto& i:inimigos)if(i.vivo && hypot(a.tx-i.visualC-.5f,a.ty-i.visualL-.5f)<.95f){matarInimigo(i,pontos,marioAliado.dono);acertou=true;}
        if(marioAliado.dono>=0 && estado==JOGANDO && jogadorEmCampo(2-marioAliado.dono)){
            int rival=1-marioAliado.dono;float rl=rival==0?jogadorVisualL:jogador2VisualL,rc=rival==0?jogadorVisualC:jogador2VisualC;
            if(hypot(a.tx-rc-.5f,a.ty-rl-.5f)<.95f){derrubarJogador(rival+1);ajustarPontos(PONTOS_POR_INIMIGO,marioAliado.dono);acertou=true;}
        }
        for(auto& b:bombas)if(b.ativa && iguais(b.posicao,p)){b.tempoMs=0;acertou=true;}
        if(celulaBoss(p.linha,p.coluna)){
            boss.vida=max(0,boss.vida-3);boss.imunidadeMs=IMUNIDADE_BOSS_MS;reagirDanoHornet();acertou=true;
            if(boss.vida==0)concluirLutaBoss(marioAliado.dono);
        }
        (void)acertou; // Pontos somente por conquistas, nao por acertos isolados.
    }
    macasMario.erase(remove_if(macasMario.begin(),macasMario.end(),[](const MacaMario& a){return a.idade>=a.duracao;}),macasMario.end());
}

// Casa livre e longe de inimigos, bombas e ataques do chefe para renascer.
Posicao posicaoParaRenascer(int numero) {
    Posicao outro = numero == 1 ? jogador2 : jogador;
    for (int distanciaMinima = 4; distanciaMinima >= 0; distanciaMinima--) {
        vector<Posicao> livres;
        for (int l = 1; l < LINHAS - 1; l++)
            for (int c = 1; c < COLUNAS - 1; c++) {
                Posicao p{l, c};
                if (mapa[l][c] != VAZIO || bombaEm(p) || perigoDaBomba(p) || perigoDoBoss(p) || bloqueadoBoss(p)) continue;
                if (jogadorEmCampo(3-numero) && iguais(p, outro)) continue;
                bool perto = false;
                for (const Inimigo& i : inimigos)
                    if (i.vivo && abs(i.posicao.linha-l) + abs(i.posicao.coluna-c) < distanciaMinima) perto = true;
                if (!perto) livres.push_back(p);
            }
        if (!livres.empty()) {
            uniform_int_distribution<int> sorteio(0, static_cast<int>(livres.size()) - 1);
            return livres[sorteio(gerador)];
        }
    }
    return numero == 1 ? jogador : jogador2;
}
// Depois da animacao de queda, quem ainda tem vidas volta a arena.
void atualizarVidasJogadores(int tempoMs) {
    if (!segundoJogadorAtivo()) return;
    for (int i = 0; i < 2; i++) {
        imunidadeJogadorMs[i] = max(0, imunidadeJogadorMs[i] - tempoMs);
        if (!jogadorCaido[i] || vidasJogador[i] == 0 || tempoQuedaMs[i] < DURACAO_MORTE_MS) continue;
        Posicao p = posicaoParaRenascer(i+1);
        if (i == 0) { jogador = p; jogadorVisualL = float(p.linha); jogadorVisualC = float(p.coluna); direcaoJogador = {0,1}; tempoMovimentoMs = 0; }
        else { jogador2 = p; jogador2VisualL = float(p.linha); jogador2VisualC = float(p.coluna); direcaoJogador2 = {0,1}; tempoMovimentoMs2 = 0; }
        jogadorCaido[i] = false;
        imunidadeJogadorMs[i] = IMUNIDADE_RENASCER_MS;
    }
}

void atualizarBot() {
    // Decide a cada tick de logica (nao mais a cada INTERVALO_BOT_MS), mas o
    // movimento em si continua pausado pelo mesmo cooldown do jogador manual
    // (tempoMovimentoMs, dentro de moverJogador). Isso elimina a dessincronia
    // entre "quando o bot decide" e "quando o jogo deixa mover", que causava
    // o personagem parecendo travar/bater em paredes no modo automatico.
    if (modo != AUTOMATICO || estado != JOGANDO || tempoMovimentoMs > 0) return;
    // Bombas de inimigos so importam quando ameacam o bot; a dele, sempre.
    if (bombasDoDono(0) > 0 || perigoDaBomba(jogador) || perigoDoBoss(jogador)) {
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
    if (perigoDaBomba(destino)) return; // espera a bomba do inimigo explodir
    if (!iguais(destino, jogador)) moverJogador(destino.linha - jogador.linha, destino.coluna - jogador.coluna);
}

void processarMovimentoContinuo() {
    // Le o estado real das teclas a cada tick de logica, em vez de depender
    // do WM_KEYDOWN, que segue o delay/taxa de repeticao configurados no
    // Windows e causa a sensacao de input lag/travamento ao segurar uma tecla.
    if (modo != MANUAL) return;
    // Teclado dividido (P1 WASD, P2 setas) so quando os dois usam o teclado;
    // senao as duas leituras reagiriam a mesma tecla e os dois andariam juntos.
    // Quem esta sozinho no teclado usa WASD e setas.
    auto moverPorTeclas=[&](bool p2) {
        struct Entrada { int tecla,dl,dc; };
        vector<Entrada> entradas;
        const Entrada letras[]={{'W',-1,0},{'S',1,0},{'A',0,-1},{'D',0,1}};
        const Entrada setas[]={{VK_UP,-1,0},{VK_DOWN,1,0},{VK_LEFT,0,-1},{VK_RIGHT,0,1}};
        int dono=teclaDoJogador(p2);
        if(dono&TECLAS_WASD) for(auto e:letras) if(teclaPressionada[e.tecla]) entradas.push_back(e);
        if(dono&TECLAS_SETAS) for(auto e:setas) if(teclaPressionada[e.tecla]) entradas.push_back(e);
        stable_sort(entradas.begin(),entradas.end(),[](const Entrada& a,const Entrada& b){return ordemTecla[a.tecla]>ordemTecla[b.tecla];});
        for(auto e:entradas) {
            Posicao antes=p2 ? jogador2 : jogador;
            if(p2) moverJogador2(e.dl,e.dc); else moverJogador(e.dl,e.dc);
            if(!iguais(antes,p2 ? jogador2 : jogador) || estado!=JOGANDO) break;
        }
    };
    // O joystick usa o mesmo moverJogador/colocarBomba do teclado: mesmo
    // ritmo, colisao, animacao, bomba e itens.
    bool bombaNova=entradaControle.bomba && !botaoBombaControleAnterior;
    botaoBombaControleAnterior=entradaControle.bomba;
    moverPorTeclas(false);
    if(controleNoJogador(1)){
        if(entradaControle.dl || entradaControle.dc) moverJogador(entradaControle.dl,entradaControle.dc);
        if(bombaNova) colocarBomba();
    }
    if(segundoJogadorAtivo()){
        moverPorTeclas(true);
        if(controleNoJogador(2)){
            if(entradaControle.dl || entradaControle.dc) moverJogador2(entradaControle.dl,entradaControle.dc);
            if(bombaNova) colocarBomba2();
        }
    }
}

void atualizar(int tempoMs) {
    relogioAnimacaoMs += tempoMs;
    if (modo == MENU) return;
    for (int i = 0; i < 2; i++) if (jogadorCaido[i]) tempoQuedaMs[i] += tempoMs;
    somMorteMs = max(0, somMorteMs - tempoMs);
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
    atualizarVidasJogadores(tempoMs);
    processarMovimentoContinuo();
    if (modo == AUTOMATICO) atualizarBot();
    atualizarBombas(tempoMs);
    moverInimigos(tempoMs);
    soltarBombasInimigos(tempoMs);
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
    if(estado==JOGANDO && jogadorEmCampo(1) && visualNoDestino(jogadorVisualL,jogadorVisualC,jogador)) {
        coletarItens(); avancarOuVencerSePortal(jogador);
    }
    if(estado==JOGANDO && jogadorEmCampo(2) && visualNoDestino(jogador2VisualL,jogador2VisualC,jogador2)) {
        coletarItensEm(jogador2, 1); avancarOuVencerSePortal(jogador2, 1);
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
        auto salvar = [](const wstring& nome, const wstring& padrao, int pontuacao, int bombasUsadas, int movimentos) {
            RegistroPontuacao registro;
            registro.data = dataAtual();
            registro.jogador = wparaStr(nome.empty() ? padrao : nome);
            registro.pontuacao = pontuacao;
            registro.bombas = bombasUsadas;
            registro.movimentos = movimentos;
            return salvarRegistroPartida(registro);
        };
        if (segundoJogadorAtivo()) {
            // Dois nomes no ranking: no modo amigos os dois levam os pontos do
            // time; no modo inimigos, cada um leva os seus.
            bool ok1 = salvar(nomeJogador, L"Jogador 1", modoRivais ? pontosJogador[0] : pontos,
                              estatisticas.bombasJogador[0], estatisticas.movimentosJogador[0]);
            bool ok2 = salvar(nomeJogador2, L"Jogador 2", modoRivais ? pontosJogador[1] : pontos,
                              estatisticas.bombasJogador[1], estatisticas.movimentosJogador[1]);
            erroSalvarRanking = !ok1 || !ok2;
        } else {
            erroSalvarRanking = !salvar(nomeJogador, L"Jogador", pontos, estatisticas.bombasUsadas, estatisticas.movimentos);
        }
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
unique_ptr<Gdiplus::Bitmap> apararMargensTransparentes(unique_ptr<Gdiplus::Bitmap> quadro){
    int w=int(quadro->GetWidth()),h=int(quadro->GetHeight()),x0=w,y0=h,x1=-1,y1=-1;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){Gdiplus::Color cor;quadro->GetPixel(x,y,&cor);
        if(cor.GetA()){x0=min(x0,x);y0=min(y0,y);x1=max(x1,x);y1=max(y1,y);}}
    if(x1<0)return quadro;
    return recortarEmBitmapCompacto(quadro.get(),x0,y0,x1-x0+1,y1-y0+1);
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
    // Pedra clara com um leve tom de musgo: separa os pilares do chao verde.
    spritePedraSelva=recortarEmBitmapCompacto(spriteArenasEstudo.get(),5*46,46,46,46);
    for(int y=0;y<46;++y)for(int x=0;x<46;++x){
        Gdiplus::Color cor;spritePedraSelva->GetPixel(x,y,&cor);
        int luz=(cor.GetR()*30+cor.GetG()*59+cor.GetB()*11)/100;
        spritePedraSelva->SetPixel(x,y,Gdiplus::Color(cor.GetA(),min(235,65+luz*3/4),min(240,72+luz*3/4),min(225,60+luz*3/4)));
    }
    {Gdiplus::Graphics pedra(spritePedraSelva.get());
        Gdiplus::Pen luz(Gdiplus::Color(255,222,229,202),2),sombra(Gdiplus::Color(255,48,62,42),3);
        pedra.DrawRectangle(&sombra,1,1,43,43);
        pedra.DrawLine(&luz,3,3,42,3);pedra.DrawLine(&luz,3,3,3,42);}
    // Madeira em tons quentes por toda a casa, inclusive as margens verdes
    // do recorte original. Mantem os veios, mas distingue das paredes da selva.
    spriteCaixaSelva=recortarEmBitmapCompacto(spriteArenasEstudo.get(),5*46,92,46,46);
    for(int y=0;y<46;++y)for(int x=0;x<46;++x){
        Gdiplus::Color cor;spriteCaixaSelva->GetPixel(x,y,&cor);
        int luz=(cor.GetR()*30+cor.GetG()*59+cor.GetB()*11)/100;
        spriteCaixaSelva->SetPixel(x,y,Gdiplus::Color(cor.GetA(),min(255,55+luz),min(210,22+luz*3/4),min(115,10+luz/3)));
    }
    {Gdiplus::Graphics caixa(spriteCaixaSelva.get());
        Gdiplus::Pen luz(Gdiplus::Color(255,255,205,115),2),sombra(Gdiplus::Color(255,83,40,15),3);
        caixa.DrawRectangle(&sombra,1,1,43,43);
        caixa.DrawLine(&luz,3,3,42,3);caixa.DrawLine(&luz,3,3,3,42);}
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
    spriteInimigoVerde=recortarEmBitmapCompacto(spriteInimigoDirecional.get(),0,0,
        int(spriteInimigoDirecional->GetWidth()),int(spriteInimigoDirecional->GetHeight()));
    for(UINT y=0;y<spriteInimigoVerde->GetHeight();++y)for(UINT x=0;x<spriteInimigoVerde->GetWidth();++x){
        Gdiplus::Color cor;spriteInimigoVerde->GetPixel(x,y,&cor);
        if(cor.GetA() && cor.GetR()>cor.GetG()*1.25f && cor.GetR()>cor.GetB()*1.2f){
            int luz=(cor.GetR()*30+cor.GetG()*59+cor.GetB()*11)/100;
            spriteInimigoVerde->SetPixel(x,y,Gdiplus::Color(cor.GetA(),20+luz/4,min(255,60+luz),20+luz/3));
        }
    }
    atlasHornet=carregarSprite(L"hornet_animacoes.png");
    atlasBowser=carregarSprite(L"bowser_animacoes.png");
    spriteArenasNovas=carregarSprite(L"arenas_novas.png");
    spriteArenaCongo=carregarSprite(L"arena_congo.png");
    int agulha=ANIM_HORNET[27].inicio,fio=ANIM_HORNET[21].inicio;
    for(int i=0;i<3;++i)spritesAgulhaBoss[i]=apararMargensTransparentes(recortarEmBitmapCompacto(atlasHornet.get(),(agulha+i)%16*160,(agulha+i)/16*160,160,160));
    for(int i=0;i<5;++i)spritesFioBoss[i]=recortarEmBitmapCompacto(atlasHornet.get(),(fio+i)%16*160,(fio+i)/16*160,160,160);
    int linhaFio=ANIM_HORNET[28].inicio,efeito=ANIM_HORNET[17].inicio;
    for(int i=0;i<3;++i)fiosAgulhaBoss[i]=recortarEmBitmapCompacto(atlasHornet.get(),(linhaFio+i)%16*160,(linhaFio+i)/16*160,160,160);
    for(int i=0;i<5;++i)efeitosDashHornet[i]=recortarEmBitmapCompacto(atlasHornet.get(),(efeito+i)%16*160,(efeito+i)/16*160,160,160);
    for(int i=0;i<4;++i)spritesFogoBowser[i]=recortarEmBitmapCompacto(spriteFogoBoss.get(),5+i*40,4,31,55);
    spriteTemaFase1 = carregarSprite(L"SNES - Super Bomberman - Story Mode Stages - Stage 2-5.png");
    spriteTemaFase3 = carregarSprite(L"SNES - Super Bomberman - Story Mode Stages - Stage 6-3.png");
    prepararTemaFase2Compacto();
}

// MCI reproduz MP3 usando o winmm, ja vinculado ao jogo. Fechar/abrir/tocar
// uma faixa trava ~100 ms, entao o MCI roda numa thread propria: a thread do
// jogo so pede a faixa e segue (senao o personagem "pula" no inicio da fase).
MCIDEVICEID dispositivoMusica = 0; // so a thread de musica mexe
wstring faixaMusicaAtual;          // so a thread de musica mexe
CRITICAL_SECTION travaMusica;
wstring faixaMusicaPedida;         // protegida por travaMusica
bool encerrandoMusica = false;     // protegida por travaMusica
HANDLE eventoMusica = nullptr, threadMusica = nullptr;
wstring ultimaFaixaPedida;         // so a thread do jogo mexe

const wchar_t* const SOM_MORTE = L"dead.wav";

wstring musicaDaTela() {
    if (modo == MENU) return L"menu.mp3";
    // "|n" distingue uma queda da outra: cada morte recomeca o som.
    if (estado == DERROTA || estado == FIM_DE_JOGO || somMorteMs > 0)
        return wstring(SOM_MORTE) + L"|" + to_wstring(quedasComSom);
    if (faseAtual == TOTAL_FASES) return L"bowser_phase.mp3";
    return L"phase_" + to_wstring(faseAtual) + L".mp3";
}

void fecharFaixa() {
    if (dispositivoMusica) {
        mciSendCommandW(dispositivoMusica, MCI_CLOSE, MCI_WAIT, 0);
        dispositivoMusica = 0;
    }
    faixaMusicaAtual.clear();
}

// Roda na thread de musica (ou direto nos testes). Faixa vazia = silencio.
void tocarFaixa(const wstring& faixa) {
    if (faixa.empty()) { fecharFaixa(); return; }
    if (faixa == faixaMusicaAtual) return;
    fecharFaixa();
    faixaMusicaAtual = faixa;
    wstring arquivo = faixa.substr(0, faixa.find(L'|'));
    bool repetir = arquivo != SOM_MORTE; // o som de morte toca uma vez so
    wstring caminho = pastaDoExecutavel() + L"\\assets\\audio\\" + arquivo;
    MCI_OPEN_PARMSW abertura{};
    abertura.lpstrDeviceType = L"mpegvideo";
    abertura.lpstrElementName = caminho.c_str();
    MCIERROR erro = mciSendCommandW(0, MCI_OPEN,
        MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_WAIT,
        reinterpret_cast<DWORD_PTR>(&abertura));
    if (!erro) {
        dispositivoMusica = abertura.wDeviceID;
        MCI_PLAY_PARMS reproducao{};
        erro = mciSendCommandW(dispositivoMusica, MCI_PLAY, repetir ? MCI_DGV_PLAY_REPEAT : 0,
                              reinterpret_cast<DWORD_PTR>(&reproducao));
    }
    if (erro) {
        // Evita tentar reabrir um arquivo ausente/invalido a cada tick.
        wchar_t detalhe[256]{};
        mciGetErrorStringW(erro, detalhe, 256);
        wstring mensagem = L"Falha ao tocar " + caminho + L": " + detalhe + L"\n";
        OutputDebugStringW(mensagem.c_str());
        fecharFaixa();
        faixaMusicaAtual = faixa;
    }
}

DWORD WINAPI rodarThreadMusica(LPVOID) {
    for (;;) {
        WaitForSingleObject(eventoMusica, INFINITE);
        EnterCriticalSection(&travaMusica);
        wstring faixa = faixaMusicaPedida;
        bool sair = encerrandoMusica;
        LeaveCriticalSection(&travaMusica);
        if (sair) break;
        // Pedidos seguidos se juntam: so a ultima faixa pedida e aberta.
        tocarFaixa(faixa);
    }
    fecharFaixa();
    return 0;
}

void pedirFaixa(const wstring& faixa) {
    // O timer chama a cada quadro; so repassa quando a faixa muda.
    if (faixa == ultimaFaixaPedida && (threadMusica || faixa.empty())) return;
    ultimaFaixaPedida = faixa;
    if (!threadMusica) {
        InitializeCriticalSection(&travaMusica);
        eventoMusica = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        threadMusica = CreateThread(nullptr, 0, rodarThreadMusica, nullptr, 0, nullptr);
    }
    EnterCriticalSection(&travaMusica);
    faixaMusicaPedida = faixa;
    LeaveCriticalSection(&travaMusica);
    SetEvent(eventoMusica);
}

void iniciarMusica() { pedirFaixa(musicaDaTela()); }
void pararMusica() { pedirFaixa(L""); }

// Ao fechar o jogo: a thread fecha o dispositivo MCI e termina.
void encerrarMusica() {
    if (!threadMusica) return;
    EnterCriticalSection(&travaMusica);
    encerrandoMusica = true;
    LeaveCriticalSection(&travaMusica);
    SetEvent(eventoMusica);
    if (WaitForSingleObject(threadMusica, 3000) != WAIT_OBJECT_0) return;
    CloseHandle(threadMusica); CloseHandle(eventoMusica);
    DeleteCriticalSection(&travaMusica);
    threadMusica = eventoMusica = nullptr;
    encerrandoMusica = false;
    ultimaFaixaPedida.clear();
}

void liberarBuffer() {
    if (bufferDC) { DeleteDC(bufferDC); bufferDC = nullptr; }
    if (bufferBitmap) { DeleteObject(bufferBitmap); bufferBitmap = nullptr; }
    bufferLargura = 0;
    bufferAltura = 0;
}

void liberarAssets() {
    spriteCaixaSelva.reset();
    spritePedraSelva.reset();
    for(auto& cache:cacheQuadrosBoss)cache.clear();
    atlasHornet.reset();atlasBowser.reset();spriteArenasNovas.reset();
    spriteArenaCongo.reset();
    for(auto& s:spritesAgulhaBoss)s.reset();
    for(auto& s:spritesFioBoss)s.reset();
    for(auto& s:spritesFogoBowser)s.reset();
    for(auto& s:fiosAgulhaBoss)s.reset();
    for(auto& s:efeitosDashHornet)s.reset();
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
    spriteInimigoVerde.reset();
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
    // Segoe UI pode nao existir no Wine. Nao guardar uma fonte invalida no cache.
    unique_ptr<Gdiplus::Font> fonte;
    for (const wchar_t* nome : {L"Segoe UI", L"Liberation Sans", L"DejaVu Sans", L"Arial"}) {
        fonte = make_unique<Gdiplus::Font>(nome, tamanhoPx, Gdiplus::FontStyleRegular,
                                         Gdiplus::UnitPixel);
        if (fonte->GetLastStatus() == Gdiplus::Ok) break;
    }
    if (fonte->GetLastStatus() != Gdiplus::Ok) {
        fonte = make_unique<Gdiplus::Font>(Gdiplus::FontFamily::GenericSansSerif(),
                    tamanhoPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    }
    Gdiplus::Font* ponteiro = fonte.get();
    cacheFontes[tamanhoPx] = move(fonte);
    return ponteiro;
}

void desenharTexto(Gdiplus::Graphics& g, const wstring& texto, float x, float y,
                   float tamanho, Gdiplus::Color cor = Gdiplus::Color(255,220,230,240), bool centralizado = false) {
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
    if(fase==7){auto* f=spriteArenaCongo.get();
        return {f,Gdiplus::Rect(0,0,46,46),f,Gdiplus::Rect(0,46,46,46),f,Gdiplus::Rect(0,92,46,46)};}
    if(fase>=9){auto* f=spriteArenasNovas.get();int x=(fase-9)*46;
        return {f,Gdiplus::Rect(x,0,46,46),f,Gdiplus::Rect(x,46,46,46),f,Gdiplus::Rect(x,92,46,46)};}
    if (fase>3) {
        auto* f=spriteArenasEstudo.get();
        int x=INDICES_ATLAS_FASES[fase-1]*46;
        if(fase==6)return {f,Gdiplus::Rect(x,0,46,46),spritePedraSelva.get(),Gdiplus::Rect(0,0,46,46),spriteCaixaSelva.get(),Gdiplus::Rect(0,0,46,46)};
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
    if((tipo==VAZIO || tipo==PORTAL) && faseAtual>=9){
        bool variante=faseAtual==10?(l%4!=2 && c%4!=3):(l+c)%5==0;
        if(variante)origemTextura.Y=138;
    }
    bool temTextura = folhaTextura != nullptr;
    if((tipo==VAZIO || tipo==PORTAL) && faseAtual==7){
        int variante=(l*3+c*7)%5;
        if(variante<2)origemTextura.Y=(3+variante)*46;
    }

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
Gdiplus::Image* quadroPoderBoss(PoderBoss poder,int quadro,int arma=0){
    if(poder==AGULHA)return spritesAgulhaBoss[quadro%3].get();
    if(poder==FIO)return spritesFioBoss[quadro%5].get();
    if(poder==FOGO_BOWSER)return spritesFogoBowser[quadro%4].get();
    if(poder==GOLPE_BOWSER)return impactoBoss[quadro%4].get();
    if(poder==MACHADO)return arma==1?facoesBoss[quadro%5].get():spriteMachadoBoss.get();
    return poder==ENERGIA?energiaBoss[quadro%4].get():venenoBoss[quadro%4].get();
}
// Escolha de poses originais por acao, sem repetir um retrato estatico.
int linhaPoseBoss(){
    if(indiceBoss()==2){
        if(boss.morteMs>0)return 38;
        if(boss.imunidadeMs>450 && (boss.movendo || hornetNoAr()))return 36;
        switch(boss.etapaHornet){
            case H_CORRIDA:return 4;
            case H_PREP_AGULHA:return boss.tempoEtapaHornetMs>150?23:24;
            case H_LANCAR:case H_VOO_AGULHA:return 25;
            case H_RECOLHER:return 26;
            case H_PREP_DASH:return boss.tempoEtapaHornetMs>200?9:10;
            case H_DASH:case H_RECUP_DASH:return 11;
            case H_PREP_SALTO:return 5;
            case H_SALTO:return 6;
            case H_PREP_GIRO:return 19;
            case H_GIRO:return 20;
            case H_POUSO:return 7;
            case H_PREP_AEREO:return 15;
            case H_AEREO:case H_RECUP_AEREO:return 16;
            case H_GUARDA:return 31;
            case H_CONTRA:case H_RECUP_CONTRA:return 32;
            case H_DANO:case H_RECUP_DANO:return 37;
            default:return boss.relogioPoseMs<650?0:3;
        }
    }
    if(boss.morteMs>0)return 38+min(3,(1050-boss.morteMs)/270);
    if(boss.imunidadeMs>450)return 39;
    if(boss.investindo)return boss.avisoInvestidaMs>0?54:55;
    if(boss.acaoEspecial==4)return boss.tempoEspecialMs>1000?48:boss.tempoEspecialMs>750?49:boss.tempoEspecialMs>400?50:boss.tempoEspecialMs>150?51:47;
    if(boss.acaoEspecial==1)return boss.tempoEspecialMs>900?45:boss.tempoEspecialMs>350?14:47;
    if(boss.acaoEspecial==3)return boss.tempoEspecialMs>1450?45:boss.tempoEspecialMs>400?55:33;
    if(boss.movendo)return boss.destino.linha!=boss.posicao.linha?(boss.destino.linha>boss.posicao.linha?57:56):16;
    return 0;
}
int quadroAnimacaoBoss(){
    int linha=linhaPoseBoss();const auto& a=indiceBoss()==2?ANIM_HORNET[linha]:ANIM_BOWSER[linha];
    if(boss.morteMs>0){int t=1050-boss.morteMs;
        return a.inicio+min(a.quantidade-1,(indiceBoss()==2?t:t%270)/145);}
    if(indiceBoss()==2){
        int t=boss.duracaoEtapaHornetMs-boss.tempoEtapaHornetMs,d=max(1,boss.duracaoEtapaHornetMs);
        int primeiro=0,n=a.quantidade;
        switch(boss.etapaHornet){
            case H_IDLE:case H_CORRIDA:return a.inicio+(boss.relogioPoseMs/95)%n;
            case H_VOO_AGULHA:return a.inicio+n-1;
            case H_DASH:case H_AEREO:return a.inicio+(boss.relogioPoseMs/90)%2;
            case H_RECUP_DASH:case H_RECUP_AEREO:primeiro=2;n-=2;break;
            case H_RECUP_CONTRA:primeiro=n-2;n=2;break;
            case H_RECUP_DANO:return a.inicio+(t<d/2?1:0);
            case H_PREP_AGULHA:t=boss.tempoEtapaHornetMs>150?750-boss.tempoEtapaHornetMs:150-boss.tempoEtapaHornetMs;d=boss.tempoEtapaHornetMs>150?600:150;break;
            case H_PREP_DASH:t=boss.tempoEtapaHornetMs>200?1000-boss.tempoEtapaHornetMs:200-boss.tempoEtapaHornetMs;d=boss.tempoEtapaHornetMs>200?800:200;break;
            default:break;
        }
        return a.inicio+primeiro+min(n-1,max(0,t)*n/d);
    }
    int tempo=boss.morteMs>0?1050-boss.morteMs:boss.relogioPoseMs;
    bool unica=boss.morteMs>0;int duracao=80*a.quantidade;
    if(boss.avisoInvestidaMs>0){tempo=1000-boss.avisoInvestidaMs;duracao=1000;unica=true;}
    else if(boss.acaoEspecial==1){
        int corte=indiceBoss()==2?400:350;
        int inicio=boss.tempoEspecialMs>900?1500:boss.tempoEspecialMs>corte?900:corte;
        int fim=boss.tempoEspecialMs>900?900:boss.tempoEspecialMs>corte?corte:0;
        tempo=inicio-boss.tempoEspecialMs;duracao=inicio-fim;unica=true;
    }else if(boss.acaoEspecial==3){
        int inicio=boss.tempoEspecialMs>1450?1800:indiceBoss()==4?1450:boss.tempoEspecialMs>650?1450:boss.tempoEspecialMs>400?650:boss.tempoEspecialMs>150?400:150;
        int fim=boss.tempoEspecialMs>1450?1450:indiceBoss()==4?400:boss.tempoEspecialMs>650?650:boss.tempoEspecialMs>400?400:boss.tempoEspecialMs>150?150:0;
        tempo=inicio-boss.tempoEspecialMs;duracao=max(1,inicio-fim);unica=true;
    }else if(boss.acaoEspecial==4){
        int inicio=boss.tempoEspecialMs>1000?1500:boss.tempoEspecialMs>750?1000:boss.tempoEspecialMs>400?750:boss.tempoEspecialMs>150?400:150;
        int fim=boss.tempoEspecialMs>1000?1000:boss.tempoEspecialMs>750?750:boss.tempoEspecialMs>400?400:boss.tempoEspecialMs>150?150:0;
        tempo=inicio-boss.tempoEspecialMs;duracao=inicio-fim;unica=true;
    }
    if(indiceBoss()==4 && linha==33)return a.inicio+2+min(3,max(0,(400-boss.tempoEspecialMs)/95));
    return a.inicio+(unica?min(a.quantidade-1,max(0,tempo)*a.quantidade/max(1,duracao)):(tempo/95)%a.quantidade);
}
void desenharBossAnimado(Gdiplus::Graphics& g,float x,float y,float tamanho,bool retrato=false){
    int quadro=quadroAnimacaoBoss();auto salvo=g.Save();
    // Recorta uma vez por pose; evita reescalar a folha inteira a cada quadro.
    auto& cache=cacheQuadrosBoss[indiceBoss()==2?0:1];
    if(!cache.count(quadro))cache[quadro]=recortarEmBitmapCompacto(imagemBossAtual(),quadro%16*160,quadro/16*160,160,160);
    g.TranslateTransform(px(x+(boss.olhandoDireita?tamanho:0)),px(y));
    if(boss.olhandoDireita)g.ScaleTransform(-1,1);
    g.DrawImage(cache[quadro].get(),Gdiplus::RectF(0,0,px(tamanho),px(tamanho)),0.f,0.f,160.f,160.f,Gdiplus::UnitPixel);
    g.Restore(salvo);
    if(!retrato && indiceBoss()==2 && (boss.etapaHornet==H_PREP_GIRO || boss.etapaHornet==H_GUARDA))
        desenharTexto(g,boss.etapaHornet==H_GUARDA?L"CONTRA-ATAQUE!":L"FIOS!",int(x),int(y)-15,12,Gdiplus::Color(255,255,185,80));
    else if(!retrato && indiceBoss()==4 && boss.acaoEspecial==3 && boss.tempoEspecialMs>400)
        desenharTexto(g,L"SALTO!",int(x),int(y)-15,12,Gdiplus::Color(255,255,185,80));
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
    if(indiceBoss()==2){
        float x=(boss.visualC+1)*TAMANHO_CELULA,y=ALTURA_CABECALHO+(boss.visualL+1)*TAMANHO_CELULA-boss.alturaHornet;
        int q=(boss.relogioPoseMs/70)%5;
        if(boss.etapaHornet==H_DASH || boss.etapaHornet==H_AEREO)
            desenharSpritePoder(g,efeitosDashHornet[q].get(),x,y,126,boss.olhandoDireita?180.f:0.f);
        if(boss.etapaHornet==H_GIRO)
            desenharSpritePoder(g,spritesFioBoss[q].get(),x,y,124,boss.relogioPoseMs*.12f);
    }
    if(boss.investindo){
        float x=(boss.visualC+larguraBoss()/2.f)*TAMANHO_CELULA,y=ALTURA_CABECALHO+(boss.visualL+1)*TAMANHO_CELULA;
        float tx=(boss.destino.coluna+larguraBoss()/2.f)*TAMANHO_CELULA,ty=ALTURA_CABECALHO+(boss.destino.linha+1)*TAMANHO_CELULA;
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
        if(p.poder==AGULHA){
            float sx=p.origemC*TAMANHO_CELULA,sy=ALTURA_CABECALHO+p.origemL*TAMANHO_CELULA;
            float tamanho=hypot(x-sx,y-sy);
            if(tamanho>3){Gdiplus::Pen sombraFio(Gdiplus::Color(180,12,12,20),px(4.f));g.DrawLine(&sombraFio,px(sx),px(sy),px(x),px(y));
                Gdiplus::Pen fio(Gdiplus::Color(210,240,235,245),px(2.f));g.DrawLine(&fio,px(sx),px(sy),px(x),px(y));
                auto salvo=g.Save();g.TranslateTransform(px(sx),px(sy));g.RotateTransform(atan2(y-sy,x-sx)*180/3.1415926f);
                g.DrawImage(fiosAgulhaBoss[q%3].get(),Gdiplus::RectF(0,px(-12.f),px(tamanho),px(24.f)),0.f,60.f,160.f,40.f,Gdiplus::UnitPixel);g.Restore(salvo);}
            angulo+=180;
        }
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
        desenharSpritePoder(g,quadroPoderBoss(p.poder,q,p.arma),x,y,p.poder==AGULHA?56.f:p.poder==MACHADO?46.f:38.f,angulo);
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
    // Com dois jogadores, quem caiu toca a queda no proprio relogio e some.
    int tempoQueda=tempoFimMs;
    if (jogadorCaido[indice]) {
        if (tempoQuedaMs[indice] >= DURACAO_MORTE_MS) return;
        morto=true; tempoQueda=tempoQuedaMs[indice];
    }
    if (!morto && imunidadeJogadorMs[indice] > 0 && (imunidadeJogadorMs[indice]/120)%2==0) return; // pisca ao renascer
    if (morto && posesDerrota[skin]) {
        auto* folha=posesDerrota[skin].get();
        int quadro=min(static_cast<int>(folha->GetWidth())/16-1,tempoQueda/130);
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
        // Arco roxo = bomba de inimigo.
        Gdiplus::Pen arco(b.tempoMs < 800 && !b.relogio ? Gdiplus::Color(255,255,105,70)
                          : b.dono == DONO_INIMIGO ? Gdiplus::Color(255,200,110,255) : Gdiplus::Color(255,98,235,199), px(2));
        float progresso = b.relogio ? 1.f : static_cast<float>(b.tempoMs) / TEMPO_BOMBA_MS;
        g.DrawArc(&arco, x-px(23.f), y-px(23.f), px(46.f), px(46.f), -90.f, 360.f*progresso);
    }
    struct AtorDesenho { float linha; int tipo, indice; };
    vector<AtorDesenho> ordem={{jogadorVisualL,0,0}};
    if(segundoJogadorAtivo()) ordem.push_back({jogador2VisualL,0,1});
    for(int i=0;i<static_cast<int>(inimigos.size());++i)
        if(inimigos[i].vivo) ordem.push_back({inimigos[i].visualL,1,i});
    if(marioAliado.ativo && marioAliado.hp>0)ordem.push_back({marioAliado.l,3,0});
    if(boss.vivo || boss.morteMs>0) ordem.push_back({boss.visualL+1,2,0});
    stable_sort(ordem.begin(),ordem.end(),[](const AtorDesenho& a,const AtorDesenho& b){return a.linha<b.linha;});
    for(const AtorDesenho& ator:ordem) {
        if(ator.tipo==0) desenharJogadorAtual(g,ator.indice);
        else if(ator.tipo==3)desenharMarioAliado(g);
        else if(ator.tipo==1) {
            const Inimigo& i=inimigos[ator.indice];
            desenharQuadroNaCelula(g,imagemInimigoAtual(),i.visualL,i.visualC,
                linhaDaFolhaParaDirecao(i.direcao),i.quadroAnim,i.especial ? L"IN*" : L"IN");
        } else {
            int x=static_cast<int>((boss.visualC+larguraBoss()/2.f)*TAMANHO_CELULA-55);
            int y=static_cast<int>(ALTURA_CABECALHO+(boss.visualL+.6f)*TAMANHO_CELULA-70+sin(relogioAnimacaoMs*.003f)*5);
            if(boss.animAtaqueMs>0 && indiceBoss()!=3)y-=int(sin((700-boss.animAtaqueMs)/700.f*3.1415926f)*9);
            if(boss.avisoInvestidaMs>0)desenharTexto(g,L"INVESTIDA!",x-2,y-18,12,Gdiplus::Color(255,255,185,80));
            Gdiplus::SolidBrush sombra(Gdiplus::Color(80,0,0,0));
            g.FillEllipse(&sombra,px(x+15),px(bossAnimado()?int(ALTURA_CABECALHO+(boss.visualL+2)*TAMANHO_CELULA-10):y+98),px(80),px(22));
            if(bossAnimado()){
                float salto=indiceBoss()==2?boss.alturaHornet:boss.acaoEspecial==3 && boss.tempoEspecialMs>400?sin((1800-boss.tempoEspecialMs)/1400.f*3.1415926f)*65:0;
                float tamanho=indiceBoss()==2?134.f:158.f;
                float centro=(boss.visualC+larguraBoss()/2.f)*TAMANHO_CELULA;
                desenharBossAnimado(g,centro-tamanho/2,ALTURA_CABECALHO+(boss.visualL+2)*TAMANHO_CELULA-tamanho-salto,tamanho);
            }else desenharRecorte(g,imagemBossAtual(),0,0,imagemBossAtual()?imagemBossAtual()->GetWidth():1,imagemBossAtual()?imagemBossAtual()->GetHeight():1,x,y,110);
            if(boss.imunidadeMs>200){Gdiplus::Pen dano(Gdiplus::Color(230,255,155,65),px(3.f));g.DrawEllipse(&dano,px(x),px(y),px(110),px(110));}
        }
    }
    desenharPerigoBossOverlay(g);
    desenharMacasMario(g);
}

void desenharVidasJogadores(Gdiplus::Graphics& g);
void desenharHUD(Gdiplus::Graphics& g) {
    painel(g, 0, 0, LARGURA_JOGO, ALTURA_CABECALHO, Gdiplus::Color(255,11,16,28));
    painel(g, 0, 0, LARGURA_JOGO, 5, Gdiplus::Color(255,252,187,65));
    desenharTexto(g, L"BOMBER / UNIVALI", 22, 14, 30, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"FASE " + to_wstring(faseAtual) + L" / " + to_wstring(TOTAL_FASES) + L"   -   " + NOMES_FASES[faseAtual-1], 24, 58, 14,
                  Gdiplus::Color(255,110,157,186));
    desenharTexto(g, modo == AUTOMATICO ? L"COMPUTADOR JOGANDO" : doisJogadores ? (modoRivais ? L"2P INIMIGOS" : L"2P AMIGOS") : L"VOCE NO CONTROLE",
                  800, 22, 16, Gdiplus::Color(255,98,235,199));
    desenharTexto(g, (dificuldade == FACIL ? L"FACIL" : dificuldade == MEDIO ? L"MEDIO" : L"DIFICIL") +
                  wstring(L"   /   ") + to_wstring(tempoPartidaMs/1000) + L"s" +
                  (segundoJogadorAtivo() && jogadorCaido[0] && vidasJogador[0]==0 ? L"   /   P1 FORA" : L"") +
                  (segundoJogadorAtivo() && jogadorCaido[1] && vidasJogador[1]==0 ? L"   /   P2 FORA" : L""), 800, 53, 14,
                  Gdiplus::Color(255,144,164,190));
    desenharVidasJogadores(g);
}

// Vidas de cada jogador no topo (so com dois): rosto do personagem e 3 coracoes.
void desenharVidasJogadores(Gdiplus::Graphics& g) {
    if (!segundoJogadorAtivo()) return;
    for (int i = 0; i < 2; i++) {
        int x = 360 + i*215, y = 18;
        Gdiplus::Color cor = i==0 ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,252,187,65);
        painel(g, x, y, 200, 58, Gdiplus::Color(255,20,28,44));
        painel(g, x, y, 4, 58, cor);
        desenharRecorte(g, personagens[i==0 ? personagemP1 : personagemP2].get(), 16, 0, 16, 24, x+10, y+5, 32);
        wstring nome = i==0 ? nomeJogador : nomeJogador2;
        if (nome.empty()) nome = i==0 ? L"P1" : L"P2";
        if (nome.size() > 10) nome = nome.substr(0,10) + L"...";
        desenharTexto(g, (i==0 ? L"P1  " : L"P2  ") + nome, x+50, y+6, 11, cor);
        // Vida extra pode passar das 3 iniciais; cabem ate 5 coracoes no painel.
        for (int v = 0; v < min(5, max(VIDAS_POR_JOGADOR, vidasJogador[i])); v++) {
            bool cheia = v < vidasJogador[i];
            float cx = float(x+58+v*26), cy = float(y+40);
            Gdiplus::SolidBrush pincel(cheia ? Gdiplus::Color(255,255,85,95) : Gdiplus::Color(255,58,66,82));
            // Coracao: dois circulos e um triangulo.
            g.FillEllipse(&pincel, px(cx-8), px(cy-7), px(9.f), px(9.f));
            g.FillEllipse(&pincel, px(cx-1), px(cy-7), px(9.f), px(9.f));
            Gdiplus::PointF ponta[] = {Gdiplus::PointF(px(cx-8),px(cy-1.5f)),Gdiplus::PointF(px(cx+8),px(cy-1.5f)),Gdiplus::PointF(px(cx),px(cy+8))};
            g.FillPolygon(&pincel, ponta, 3);
        }
        if (vidasJogador[i] == 0 && jogadorCaido[i])
            desenharTexto(g, L"FORA", x+142, y+31, 12, Gdiplus::Color(255,255,120,100));
    }
}

void desenharPainelLateral(Gdiplus::Graphics& g) {
    painel(g, 794, ALTURA_CABECALHO, 272, LINHAS*TAMANHO_CELULA);
    if (segundoJogadorAtivo() && modoRivais) {
        // Todos contra todos: placar separado.
        desenharTexto(g, L"PONTUACAO   P1  x  P2", 812, 121, 13, Gdiplus::Color(255,144,164,190));
        desenharTexto(g, to_wstring(pontosJogador[0]), 810, 147, 30, Gdiplus::Color(255,98,235,199));
        desenharTexto(g, to_wstring(pontosJogador[1]), 935, 147, 30, Gdiplus::Color(255,252,187,65));
    } else {
        desenharTexto(g, segundoJogadorAtivo() ? L"PONTUACAO DO TIME" : L"PONTUACAO", 812, 121, 13, Gdiplus::Color(255,144,164,190));
        desenharTexto(g, to_wstring(pontos), 810, 147, 38, Gdiplus::Color(255,246,247,251));
    }
    desenharTexto(g, (segundoJogadorAtivo() ? L"VIDAS  P1 " + to_wstring(vidasJogador[0]) + L"  P2 " + to_wstring(vidasJogador[1])
                                            : L"VIDAS  " + to_wstring(vidas)) + L"    INIMIGOS  " + to_wstring(inimigosVivos()),
                  812, 207, 14, Gdiplus::Color(255,98,235,199));
    // Fogo e limite de bombas sao de cada jogador.
    auto fogoEBombas = [](int dono) {
        return L"FOGO  " + to_wstring(poderes[dono].alcance) + L"    BOMBAS  " +
               to_wstring(bombasDoDono(dono)) + L" / " + to_wstring(limiteBombas(dono));
    };
    if (segundoJogadorAtivo()) {
        desenharTexto(g, L"P1  " + fogoEBombas(0), 812, 230, 12, Gdiplus::Color(255,98,235,199));
        desenharTexto(g, L"P2  " + fogoEBombas(1), 812, 251, 12, Gdiplus::Color(255,252,187,65));
    } else desenharTexto(g, fogoEBombas(0), 812, 239, 14, Gdiplus::Color(255,252,187,65));
    painel(g, 812, 278, 236, 1, Gdiplus::Color(255,55,68,86));
    desenharTexto(g, boss.vivo ? NOMES_BOSSES[indiceBoss()] : L"OBJETIVO DA FASE", 812, 298, 14,
                  Gdiplus::Color(255,252,187,65));
    if (boss.vivo) {
        desenharTexto(g,PODERES_BOSSES[indiceBoss()],812,568,12,Gdiplus::Color(255,255,190,100));
        if(bossAnimado())desenharBossAnimado(g,842,330,172,true);
        else desenharRecorte(g, imagemBossAtual(), 0, 0, imagemBossAtual()?imagemBossAtual()->GetWidth():1, imagemBossAtual()?imagemBossAtual()->GetHeight():1, 842, 330, 172);
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
    auto itensDe = [](int dono) {
        const PoderesJogador& p = poderes[dono];
        TeclasItens t = teclasItensDoJogador(dono);
        wstring itens;
        if (p.cartaChute) itens += wstring(t.chute) + L": chute  ";
        if (p.cartaTeleporte) itens += wstring(t.teleporte) + L": teleporte  ";
        if (p.bombaRelogio) itens += wstring(t.detonar) + L": detonar  ";
        if (p.imuneAPropriaBomba) itens += L"Escudo  ";
        if (p.atravessaBloco) itens += L"Atravessar caixas";
        return itens;
    };
    if (segundoJogadorAtivo()) {
        desenharTexto(g, L"P1  " + itensDe(0), 812, 700, 11, Gdiplus::Color(255,98,235,199));
        desenharTexto(g, L"P2  " + itensDe(1), 812, 720, 11, Gdiplus::Color(255,252,187,65));
    } else desenharTexto(g, itensDe(0), 812, 710, 12, Gdiplus::Color(255,252,187,65));
}

wstring itensRodape(int dono) {
    TeclasItens t = teclasItensDoJogador(dono);
    return wstring(t.chute) + L" chute  " + t.teleporte + L" teleporte  " + t.detonar + L" detonar";
}

void desenharRodape(Gdiplus::Graphics& g) {
    int y = ALTURA_CABECALHO + LINHAS*TAMANHO_CELULA;
    painel(g, 0, y, LARGURA_JOGO, ALTURA_RODAPE, Gdiplus::Color(255,11,16,28));
    desenharTexto(g, modo == MANUAL ? L"P1  " + teclasDoJogador(1) + (doisJogadores ? L"      P2  " + teclasDoJogador(2) : L"")
                                    : wstring(L"COMPUTADOR JOGANDO      M  menu"), 24, y+12, 14, Gdiplus::Color(255,220,230,240));
    desenharTexto(g, somenteJoystick()
                  ? L"X  chute     Y  teleporte     L / R  detonar     SELECT  menu     fim da fase: A / START de novo"
                  : segundoJogadorAtivo()
                  ? L"itens  P1: " + itensRodape(0) + L"   P2: " + itensRodape(1) + L"     R reiniciar   M menu   ESC sair"
                  : itensNoJoystick()
                  ? L"P1: X chute  Y teleporte  L/R detonar  SELECT menu     teclado: R reiniciar  M menu  ESC sair"
                  : L"E  chute     T  teleporte     F  detonar     R  reiniciar     M  menu     ESC  sair",
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
    desenharTexto(g, venceu ? L"Campanha concluida!" : segundoJogadorAtivo() ? L"Os dois perderam as 3 vidas da fase." : L"Vidas restantes: " + to_wstring(vidas),
                  240, 480, 18, Gdiplus::Color(255,220,230,240), true);
    auto curto = [](const wstring& nome, const wchar_t* padrao) {
        wstring n = nome.empty() ? padrao : nome; return n.size() > 14 ? n.substr(0,14) + L"..." : n; };
    if (segundoJogadorAtivo() && modoRivais)
        desenharTexto(g, curto(nomeJogador,L"P1") + L"  " + to_wstring(pontosJogador[0]) + L"     x     " +
                      curto(nomeJogador2,L"P2") + L"  " + to_wstring(pontosJogador[1]), 240, 523, 20, Gdiplus::Color(255,252,187,65), true);
    else
        desenharTexto(g, (segundoJogadorAtivo() ? L"PONTOS DO TIME  " : L"PONTOS  ") + to_wstring(pontos), 240, 523, 22, Gdiplus::Color(255,252,187,65), true);
    if(erroSalvarRanking)desenharTexto(g,L"Nao foi possivel salvar o ranking. Confira a permissao da pasta.",240,565,13,Gdiplus::Color(255,255,120,100));
    wstring repetir = somenteJoystick() ? L"A / START" : algumJoystickNaPartida() ? L"R  ou  A / START" : L"R";
    wstring menu = somenteJoystick() ? L"B / SELECT" : algumJoystickNaPartida() ? L"M  ou  B / SELECT" : L"M";
    desenharTexto(g, repetir + (estado == DERROTA ? L"  tentar a fase novamente     " : L"  nova partida     ") + menu + L"  menu",
                  240, 602, 16, Gdiplus::Color(255,144,164,190), true);
}

void desenharNomeDigitavel(Gdiplus::Graphics& g,int x,int y,int largura,const wstring& nomeJogador,bool editandoNome,
                           const wchar_t* vazio=L"Clique aqui e digite seu nome"){
    auto salvo=g.Save();g.SetClip(Gdiplus::Rect(px(x),px(y),px(largura),px(29)));
    auto formato=Gdiplus::StringFormat::GenericTypographic()->Clone();
    formato->SetFormatFlags(formato->GetFormatFlags()|Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    Gdiplus::RectF medida;wstring prefixo=nomeJogador.substr(0,min(cursorNome,nomeJogador.size()));
    auto* fonte=obterFonte(px(20));g.MeasureString(prefixo.c_str(),int(prefixo.size()),fonte,Gdiplus::PointF(0,0),formato,&medida);
    float deslocamento=editandoNome?max(0.f,medida.Width-px(largura)+px(8)):0.f;
    float origemX=px(x)-deslocamento;
    Gdiplus::SolidBrush cor(nomeJogador.empty()?Gdiplus::Color(255,144,164,190):Gdiplus::Color(255,246,247,251));
    const wchar_t* texto=nomeJogador.empty()?vazio:nomeJogador.c_str();
    g.DrawString(texto,-1,fonte,Gdiplus::PointF(origemX,float(px(y))),formato,&cor);
    if(editandoNome && (relogioAnimacaoMs/500)%2==0){Gdiplus::Pen cursor(Gdiplus::Color(255,98,235,199),max(1,px(2)));
        float cx=origemX+medida.Width;g.DrawLine(&cursor,cx,float(px(y)),cx,float(px(y+24)));}
    delete formato;g.Restore(salvo);
}
void desenharMenu(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"ALGORITMOS E PROGRAMACAO II / UNIVALI", 44, 35, 13, Gdiplus::Color(255,110,157,186));
    desenharTexto(g, L"BOMBERMAN", 40, 70, 46, Gdiplus::Color(255,246,247,251));
    desenharTexto(g, L"Dez arenas. Escolha onde comecar sua campanha.", 44, 135, 17, Gdiplus::Color(255,98,235,199));
    // Os nomes sao digitados depois da escolha de 1 ou 2 jogadores.
    painel(g, 44, 192, 470, 75);
    desenharTexto(g, L"NOMES NO RANKING", 60, 203, 11, Gdiplus::Color(255,144,164,190));
    wstring nomes=nomeJogador.empty() ? L"Pedidos ao escolher 1 ou 2 jogadores"
                 : L"P1 "+nomeJogador+(doisJogadores && !nomeJogador2.empty() ? L"   /   P2 "+nomeJogador2 : L"");
    desenharTexto(g,nomes.size()>44 ? nomes.substr(0,44)+L"..." : nomes,60,224,18,
                  nomeJogador.empty() ? Gdiplus::Color(255,144,164,190) : Gdiplus::Color(255,246,247,251));
    desenharTexto(g,L"JOGAR > 1 ou 2 jogadores > nomes > controles > personagens",60,252,10,Gdiplus::Color(255,252,187,65));
    const wchar_t* titulos[] = {L"1   JOGAR", L"2   DEMONSTRACAO (IA JOGA)", L"3   COMO JOGAR", L"4   DIFICULDADES", L"5   COMO PONTUAR", L"6   RANKING"};
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
    desenharRecorte(g, imagemInimigoAtual(), q*16,0,16,24,844,390,84);
    desenharRecorte(g, spriteBombaAnimada.get(), ((relogioAnimacaoMs/125)%4)*16,0,16,16,770,350,64);
    desenharTexto(g, L"EXPLORE. DESCUBRA. SOBREVIVA.", 570, 551, 20, Gdiplus::Color(255,252,187,65));
    painel(g,550,604,486,55,Gdiplus::Color(255,59,28,42));
    desenharTexto(g,L"9   MODO INSANO",570,611,22,Gdiplus::Color(255,255,135,105));
    desenharTexto(g,L"O jogo completo original da batalha de sprites",570,638,12,Gdiplus::Color(255,220,230,240));
    painel(g,550,666,486,61);
    desenharTexto(g,L"8   SELETOR DE FASES",570,677,18,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"Dez arenas para explorar e comparar",570,704,12,Gdiplus::Color(255,144,164,190));
    painel(g,550,738,486,58);
    desenharTexto(g,L"7   ESCOLHER PERSONAGENS",570,746,18,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"P1: "+wstring(nomesPersonagens[personagemP1])+L"   /   P2: "+nomesPersonagens[personagemP2],570,775,12,Gdiplus::Color(255,144,164,190));
    painel(g,44,646,470,91,Gdiplus::Color(255,23,48,62));
    desenharTexto(g,L"JOGADORES E CONTROLES",61,655,18,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"1 JOGAR: escolha 1 ou 2 jogadores, teclado/joystick e personagens.",61,684,12,Gdiplus::Color(255,220,230,240));
    desenharTexto(g,situacaoControle(),61,708,11,Gdiplus::Color(255,252,187,65));
    desenharTexto(g, L"1: jogar (ou A no joystick)   /   2: demonstracao com IA", 44, 754, 14, Gdiplus::Color(255,144,164,190));
    desenharTexto(g, L"Q / ESC   sair", 44, 796, 14, Gdiplus::Color(255,220,230,240));
}

// Textos de configuracao reaproveitados no menu, nas telas de escolha e no rodape.
// O texto quebra no "\n" e cada linha e desenhada separada (sem corte).
void desenharLinhas(Gdiplus::Graphics& g, const wstring& texto, float x, float y, float tamanho, Gdiplus::Color cor) {
    size_t inicio = 0; int linha = 0;
    while (true) {
        size_t fim = texto.find(L'\n', inicio);
        desenharTexto(g, texto.substr(inicio, fim == wstring::npos ? wstring::npos : fim - inicio), x, y + linha * tamanho * 1.45f, tamanho, cor);
        if (fim == wstring::npos) break;
        inicio = fim + 1; linha++;
    }
}

void desenharQuantidadeJogadores(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g,L"QUANTOS VAO JOGAR?",44,48,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,L"Com dois, escolha se jogam como AMIGOS ou como INIMIGOS. Depois vem o nome de cada um.",44,106,17,Gdiplus::Color(255,110,157,186));
    const wchar_t* titulos[]={L"1   1 JOGADOR",L"2   AMIGOS",L"3   INIMIGOS"};
    const wchar_t* resumos[]={L"Teclado OU joystick,\nvoce escolhe.",L"2 jogadores no mesmo\ntime, 3 vidas cada,\npontos do time.",L"Todos contra todos:\n3 vidas cada e\npontos separados."};
    const wchar_t* regras[]={L"Ninguem mais entra\nna arena.",L"Sua bomba nao fere o\nparceiro, mas fere voce.",L"Qualquer bomba fere\nqualquer um. O Mario\najuda quem o libertar."};
    int atual=!doisJogadores ? 0 : modoRivais ? 2 : 1;
    for(int i=0;i<3;++i) {
        int x=44+i*337; bool marcado=atual==i;
        Gdiplus::Color destaque=i==2 ? Gdiplus::Color(255,255,120,100) : Gdiplus::Color(255,98,235,199);
        Gdiplus::Color cor=marcado ? destaque : Gdiplus::Color(255,55,68,86);
        painel(g,x,182,317,400);
        painel(g,x,182,317,5,cor);
        if(marcado) painel(g,x,577,317,5,cor);
        desenharTexto(g,titulos[i],x+20,204,24,marcado ? cor : Gdiplus::Color(255,220,230,240));
        int q=(relogioAnimacaoMs/180)%3;
        if(i==0) desenharRecorte(g,personagens[personagemP1].get(),q*16,0,16,24,x+118,252,80);
        else {
            desenharRecorte(g,personagens[personagemP1].get(),q*16,0,16,24,x+68,252,80);
            desenharRecorte(g,personagens[personagemP2].get(),q*16,0,16,24,x+168,252,80);
        }
        desenharLinhas(g,resumos[i],x+20,372,15,Gdiplus::Color(255,220,230,240));
        desenharLinhas(g,regras[i],x+20,452,13,Gdiplus::Color(255,144,164,190));
        desenharTexto(g,marcado ? L"SELECIONADO" : L"ESCOLHER",x+20,546,15,cor);
    }
    painel(g,44,612,992,52,Gdiplus::Color(255,23,48,62));
    desenharTexto(g,doisJogadores && !controleConectado ? L"Sem joystick: os dois dividem o teclado (WASD + ESPACO / SETAS + ENTER)." : situacaoControle(),
                  62,627,15,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"1 / 2 / 3  escolher     SETAS + ENTER  (ou direcional + A)     ESC  menu",44,790,15,Gdiplus::Color(255,144,164,190));
}

// Nome de cada jogador para o ranking, com o TOP 10 ao lado.
void desenharNomes(Gdiplus::Graphics& g, const vector<RegistroPontuacao>& registros) {
    fundoInterface(g);
    desenharTexto(g,L"NOMES PARA O RANKING",44,36,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,!doisJogadores ? L"1 JOGADOR: digite seu nome."
                   : modoRivais ? L"INIMIGOS: cada um digita o seu nome e leva os proprios pontos."
                                : L"AMIGOS: cada um digita o seu nome; os dois levam os pontos do time.",
                  44,90,17,Gdiplus::Color(255,110,157,186));
    for(int i=1;i<=(doisJogadores?2:1);++i) {
        int y=150+(i-1)*118; bool ativo=editandoNome && campoNome==i;
        const wstring& nome=i==1 ? nomeJogador : nomeJogador2;
        Gdiplus::Color cor=i==1 ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,252,187,65);
        painel(g,44,y,560,98,ativo ? Gdiplus::Color(255,30,52,66) : Gdiplus::Color(255,25,34,50));
        painel(g,44,y,5,98,ativo ? cor : Gdiplus::Color(255,55,68,86));
        desenharTexto(g,i==1 ? L"P1   NOME" : L"P2   NOME",64,y+12,13,cor);
        if(ativo && selecionarNome && !nome.empty()) painel(g,62,y+44,400,29,Gdiplus::Color(255,40,83,110));
        desenharNomeDigitavel(g,64,y+46,520,nome,ativo,i==1 ? L"Digite o nome de P1" : L"Digite o nome de P2");
    }
    int yTexto=doisJogadores ? 392 : 274;
    desenharTexto(g,avisoNome,44,yTexto,15,Gdiplus::Color(255,255,140,110));
    desenharLinhas(g,L"ENTER confirma e passa para o proximo"+wstring(doisJogadores ? L"\nSETAS cima / baixo trocam de campo" : L"")+
                  L"\nESC volta     Backspace / Delete apagam",44,yTexto+34,14,Gdiplus::Color(255,220,230,240));
    desenharLinhas(g,L"Joystick: o nome e digitado no teclado;\nA confirma e B volta.",44,yTexto+120,13,Gdiplus::Color(255,144,164,190));
    painel(g,44,656,560,64,Gdiplus::Color(255,30,62,66));
    painel(g,44,656,560,4,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,doisJogadores && campoNome==1 ? L"ENTER   CONFIRMAR P1" : L"ENTER   CONTINUAR",66,674,20,Gdiplus::Color(255,98,235,199));
    painel(g,640,150,396,570,Gdiplus::Color(255,18,27,43));
    desenharTexto(g,L"TOP 10",660,166,22,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"NOME",700,206,11,Gdiplus::Color(255,144,164,190));
    desenharTexto(g,L"PONTOS",940,206,11,Gdiplus::Color(255,144,164,190));
    if(registros.empty()) desenharTexto(g,L"Nenhuma partida salva ainda.",660,240,14,Gdiplus::Color(255,144,164,190));
    for(int i=0;i<min(10,int(registros.size()));++i) {
        int y=232+i*47; wstring nome=strParaW(registros[i].jogador);
        if(nome.size()>17) nome=nome.substr(0,17)+L"...";
        Gdiplus::Color cor=i==0 ? Gdiplus::Color(255,252,187,65) : Gdiplus::Color(255,220,230,240);
        if(i%2==0) painel(g,652,y-6,372,40,Gdiplus::Color(255,24,34,52));
        desenharTexto(g,to_wstring(i+1),662,y,16,cor);
        desenharTexto(g,nome,700,y,16,cor);
        desenharTexto(g,to_wstring(registros[i].pontuacao),940,y,16,cor);
    }
    desenharTexto(g,L"ENTER  confirmar     SETAS  trocar campo     ESC  voltar     ranking completo: opcao 6 do menu",44,790,14,Gdiplus::Color(255,144,164,190));
}
void desenharNomes(Gdiplus::Graphics& g) { desenharNomes(g, lerRanking()); }

void desenharAtribuicaoControles(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g,L"QUEM USA CADA CONTROLE?",44,36,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,doisJogadores ? L"2 JOGADORES: leve o teclado e o joystick para o lado de cada jogador."
                                  : L"1 JOGADOR: leve para P1 o controle que voce vai usar.",44,90,17,Gdiplus::Color(255,110,157,186));
    const wchar_t* titulos[]={L"P1",L"SEM USO",L"P2"};
    Gdiplus::Color coresLado[]={Gdiplus::Color(255,98,235,199),Gdiplus::Color(255,110,120,135),Gdiplus::Color(255,252,187,65)};
    for(int i=0;i<3;++i) {
        int x=44+i*346; bool inativa=i==2 && !doisJogadores;
        painel(g,x,140,300,350,inativa ? Gdiplus::Color(255,18,23,33) : Gdiplus::Color(255,25,34,50));
        painel(g,x,140,300,4,inativa ? Gdiplus::Color(255,45,52,64) : coresLado[i]);
        desenharTexto(g,titulos[i],x+20,154,24,inativa ? Gdiplus::Color(255,70,80,95) : coresLado[i]);
        if(inativa) desenharTexto(g,L"so com 2 jogadores",x+20,190,13,Gdiplus::Color(255,70,80,95));
    }
    auto cartao=[&](int lado,int y,const wstring& nome,const wstring& detalhe,bool disponivel) {
        int x=44+(lado+1)*346+18;
        Gdiplus::Color cor=!disponivel ? Gdiplus::Color(255,70,80,95) : lado==0 ? Gdiplus::Color(255,144,164,190) : coresLado[lado+1];
        painel(g,x,y,264,112,disponivel ? Gdiplus::Color(255,35,48,68) : Gdiplus::Color(255,24,30,42));
        painel(g,x,y,5,112,cor);
        desenharTexto(g,nome,x+20,y+14,21,cor);
        desenharTexto(g,detalhe,x+20,y+52,13,Gdiplus::Color(255,220,230,240));
    };
    bool joystick=controleConectado;
    if(doisJogadores && !joystick) {
        cartao(-1,232,L"TECLADO",L"WASD + ESPACO",true);
        cartao(1,232,L"TECLADO",L"SETAS + ENTER",true);
    } else {
        int lado=ladoDispositivo(TECLADO);
        cartao(lado,232,L"TECLADO",lado==0 ? L"nao sera usado" : teclasDoJogador(lado<0 ? 1 : 2),true);
    }
    if(joystick) {
        int lado=ladoDispositivo(JOYSTICK);
        wstring nome=nomeControle.size()>20 ? nomeControle.substr(0,20)+L"..." : nomeControle;
        cartao(lado,362,L"JOYSTICK",lado==0 ? L"nao sera usado\n"+nome : nome+L"\nA ou B bomba",true);
    } else cartao(0,362,L"JOYSTICK",L"nao detectado",false);
    for(int i=0;i<2;++i) {
        int x=44+i*506; bool habilitada=joystick || i==0;
        bool marcada=habilitada && (i==0) == (dispositivoP1==TECLADO || !joystick);
        Gdiplus::Color cor=!habilitada ? Gdiplus::Color(255,70,80,95) : marcada ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,220,230,240);
        painel(g,x,512,486,64,marcada ? Gdiplus::Color(255,30,62,66) : Gdiplus::Color(255,25,34,50));
        painel(g,x,512,4,64,cor);
        wstring texto=doisJogadores ? (i==0 ? (joystick ? L"1   P1 TECLADO  +  P2 JOYSTICK" : L"1   P1 E P2 NO TECLADO") : L"2   P1 JOYSTICK  +  P2 TECLADO")
                                    : (i==0 ? L"1   P1 NO TECLADO" : L"2   P1 NO JOYSTICK");
        desenharTexto(g,texto,x+22,532,18,cor);
    }
    painel(g,44,594,992,46,Gdiplus::Color(255,23,48,62));
    desenharTexto(g,situacaoControle(),62,607,15,Gdiplus::Color(255,252,187,65));
    painel(g,44,656,992,64,Gdiplus::Color(255,30,62,66));
    painel(g,44,656,992,4,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"ENTER / A   CONFIRMAR E ESCOLHER PERSONAGENS",62,676,20,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"SETAS ou A/D movem o TECLADO     direcional do joystick move o JOYSTICK     1 / 2 atalhos     ESC / B voltar",
                  44,790,13,Gdiplus::Color(255,144,164,190));
}

void desenharPersonagens(Gdiplus::Graphics& g) {
    fundoInterface(g);
    if(!doisJogadores) jogadorEmSelecao=1;
    desenharTexto(g,L"ESCOLHA SEU PERSONAGEM",44,48,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,resumoJogadores(),44,102,17,Gdiplus::Color(255,98,235,199));
    desenharTexto(g,L"Escolhendo P"+to_wstring(jogadorEmSelecao)+L" - "+teclasDoJogador(jogadorEmSelecao)+L" - 1-4, setas ou clique; ENTER confirma.",
                  44,137,17,jogadorEmSelecao==1 ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,252,187,65));
    desenharTexto(g,doisJogadores?L"Depois de confirmar P1, a escolha passa para P2. TAB alterna. Todos tem as mesmas habilidades."
                                :L"Cada personagem tem as mesmas habilidades.",44,174,14,Gdiplus::Color(255,144,164,190));
    int selecionado=jogadorEmSelecao==1 ? personagemP1 : personagemP2;
    for(int i=0;i<4;++i) {
        int x=44+i*254;
        painel(g,x,230,232,330);
        Gdiplus::Color cor=i==selecionado ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,55,68,86);
        painel(g,x,230,232,4,cor);
        desenharTexto(g,to_wstring(i+1)+L"   "+nomesPersonagens[i],x+18,259,20,Gdiplus::Color(255,220,230,240));
        desenharRecorte(g,personagens[i].get(),((relogioAnimacaoMs/180)%3)*16,0,16,24,x+40,310,152);
        if(personagemP1==i)desenharTexto(g,L"P1",x+18,462,16,Gdiplus::Color(255,98,235,199));
        if(doisJogadores && personagemP2==i)desenharTexto(g,L"P2",x+72,462,16,Gdiplus::Color(255,252,187,65));
        desenharTexto(g,i==selecionado ? L"SELECIONADO" : L"ESCOLHER",x+38,505,15,cor);
    }
    desenharTexto(g,doisJogadores?L"P1: "+wstring(nomesPersonagens[personagemP1])+L"     /     P2: "+nomesPersonagens[personagemP2]
                                :L"P1: "+wstring(nomesPersonagens[personagemP1]),44,626,20,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"1-4 / SETAS escolher   ENTER ou A confirmar   TAB trocar P1/P2   M controles   ESC menu",44,790,14,Gdiplus::Color(255,144,164,190));
}

void desenharSeletorFases(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g,L"ESCOLHA SUA ARENA",44,36,34,Gdiplus::Color(255,246,247,251));
    desenharTexto(g,L"Escolha a fase inicial. A campanha segue ate o Vale Langrisser, dominio do Bowser.",44,91,16,Gdiplus::Color(255,144,164,190));
    desenharTexto(g,modoPendente==MANUAL ? resumoJogadores() : wstring(L"DEMONSTRACAO  /  A IA JOGA"),44,121,14,Gdiplus::Color(255,98,235,199));
    for(int i=0;i<TOTAL_FASES;++i) {
        int x=44+(i%5)*202,y=150+(i/5)*250;
        painel(g,x,y,184,232);
        bool selecionada=faseSelecionada==i+1;
        painel(g,x,y,184,3,selecionada ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,55,68,86));
        TemaFase t=temaDaFase(i+1);
        for(int l=0;l<5;++l) for(int c=0;c<7;++c) {
            bool parede=l==0 || l==4 || c==0 || c==6 || (l==2 && c%2==0);
            bool caixa=!parede && (l+c)%3==0;
            auto* folha=parede ? t.paredeFolha : caixa ? t.caixaFolha : t.chaoFolha;
            Gdiplus::Rect r=parede ? t.parede : caixa ? t.caixa : t.chao;
            if(folha) g.DrawImage(folha,Gdiplus::Rect(px(x+11+c*23),px(y+17+l*23),px(23),px(23)),r.X,r.Y,r.Width,r.Height,Gdiplus::UnitPixel);
        }
        desenharTexto(g,to_wstring(i+1)+L"  "+NOMES_FASES[i],x+11,y+160,14,Gdiplus::Color(255,220,230,240));
        desenharTexto(g,selecionada ? L"SELECIONADA" : i<3 ? L"ARENA ORIGINAL" : i<8 ? L"ARENA DO ESTUDO" : L"NOVA ARENA",x+11,y+195,11,
                      selecionada ? Gdiplus::Color(255,98,235,199) : Gdiplus::Color(255,144,164,190));
    }
    painel(g,44,683,992,76);
    desenharTexto(g,L"INICIAR NA FASE "+to_wstring(faseSelecionada)+L"  /  "+NOMES_FASES[faseSelecionada-1],62,701,22,Gdiplus::Color(255,252,187,65));
    if(faseSelecionada%2==0)desenharTexto(g,NOMES_BOSSES[faseSelecionada/2-1],62,734,15,Gdiplus::Color(255,255,135,85));
    desenharTexto(g,L"1-9 / 0 / SETAS  selecionar     ENTER / A  iniciar     M / B  dificuldade     ESC  menu",44,790,15,Gdiplus::Color(255,144,164,190));
}

void desenharRanking(Gdiplus::Graphics& g,vector<RegistroPontuacao> registros) {
    fundoInterface(g);
    desenharTexto(g, L"RANKING", 20, 40, 32, Gdiplus::Color(255, 65, 210, 235), true);
    desenharTexto(g,L"Melhor resultado por nome. Empate: menos bombas, depois menos movimentos.",20,82,13,Gdiplus::Color(255,144,164,190));
    if (registros.empty()) {
        desenharTexto(g, L"Nenhuma partida manual salva ainda.", 20, 110, 18,
                      Gdiplus::Color(255, 205, 210, 215));
    } else {
        rankingSelecionado=clamp(rankingSelecionado,0,int(registros.size())-1);
        int inicio=rankingSelecionado/10*10;
        desenharTexto(g,L"NOME",65,117,13);desenharTexto(g,L"PONTOS",460,117,13);
        desenharTexto(g,L"BOMBAS",600,117,13);desenharTexto(g,L"MOVIMENTOS",715,117,13);desenharTexto(g,L"DATA",870,117,13);
        for(int i=inicio;i<min(inicio+10,int(registros.size()));++i){
            const auto& r=registros[i];int y=150+(i-inicio)*40;
            if(i==rankingSelecionado)painel(g,20,y-3,1030,36,Gdiplus::Color(255,35,64,81));
            desenharTexto(g,to_wstring(i+1),25,y,15);
            desenharTexto(g,strParaW(r.jogador),65,y,16);
            desenharTexto(g,to_wstring(r.pontuacao),460,y,16);
            desenharTexto(g,to_wstring(r.bombas),600,y,15);
            desenharTexto(g,to_wstring(r.movimentos),715,y,15);
            desenharTexto(g,strParaW(r.data),870,y,15);
        }
        desenharTexto(g,L"Pagina "+to_wstring(inicio/10+1)+L" de "+to_wstring((registros.size()+9)/10)+L"  /  SETAS selecionam, PgUp/PgDn mudam pagina",20,565,14);
        painel(g,20,610,280,44);desenharTexto(g,L"E  RENOMEAR",35,621,18);
        painel(g,320,610,280,44);desenharTexto(g,L"DEL  EXCLUIR",335,621,18,Gdiplus::Color(255,255,120,100));
    }
    if(editandoNome && !nomeOriginalRanking.empty()){
        painel(g,20,675,740,65);if(selecionarNome)painel(g,32,699,600,28,Gdiplus::Color(255,40,83,110));
        desenharTexto(g,L"NOVO NOME  /  ENTER salva, ESC cancela",35,681,12);
        desenharNomeDigitavel(g,35,703,700,nomeJogador,editandoNome);
    }
    desenharTexto(g,avisoRanking,20,755,14,Gdiplus::Color(255,252,187,65));
    desenharTexto(g, L"[M ou ESC] Voltar ao menu", 20, ALTURA_JOGO - 40.0f, 15,
                  Gdiplus::Color(255, 160, 165, 170), true);
}

void desenharRanking(Gdiplus::Graphics& g){desenharRanking(g,lerRanking());}

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
        if(!informativa && dificuldade==i){painel(g,x,557,316,5,cores[i]);desenharTexto(g,L"SELECIONADO",x+18,522,14,cores[i]);}
        desenharTexto(g,nomes[i],x+18,210,26,cores[i]);
        desenharTexto(g,ritmos[i],x+18,259,13,Gdiplus::Color(255,144,164,190));
        desenharTexto(g,i==0 ? L"Um inimigo solta bombas" : i==1 ? L"Metade dos inimigos solta bombas" : L"75% dos inimigos soltam bombas",
                      x+18,280,13,cores[i]);
        desenharRecorte(g,imagemInimigoAtual(),((relogioAnimacaoMs/180)%3)*16,0,16,24,x+109,299,96);
        desenharTexto(g,to_wstring(i==0 ? 3 : i==1 ? 5 : 7)+L" inimigos por fase",x+18,414,17,Gdiplus::Color(255,220,230,240));
        desenharTexto(g,i==0 ? L"Perseguicao ate 5 celulas" : L"Perseguicao a qualquer distancia",x+18,457,14,Gdiplus::Color(255,144,164,190));
        desenharTexto(g,to_wstring(i==0 ? 25 : i==1 ? 50 : 75)+L"% de chance de perseguir",x+18,490,14,cores[i]);
    }
    desenharTexto(g,L"10 FASES   /   PORTAIS   /   ITENS ESCONDIDOS",44,610,22,Gdiplus::Color(255,252,187,65));
    desenharTexto(g,L"Chefes a cada duas fases; Bowser encerra a campanha na fase 10.",44,658,17,Gdiplus::Color(255,220,230,240));
    desenharTexto(g,informativa ? L"M / ESC   voltar ao menu" : L"1 / 2 / 3  ou  SETAS + ENTER (direcional + A)   escolher       M / B   voltar",44,790,16,Gdiplus::Color(255,144,164,190));
}

void desenharComoJogar(Gdiplus::Graphics& g) {
    fundoInterface(g);
    desenharTexto(g, L"COMO JOGAR", 20, 30, 28, Gdiplus::Color(255, 65, 210, 235), true);
    struct Linha { const wchar_t* texto; int y; bool titulo; };
    const Linha linhas[] = {
        {L"Objetivo: limpe os inimigos das 10 fases da campanha, pise no", 80, false},
        {L"portal que aparece; fases 2, 4, 6, 8 e 10 tem chefes.", 98, false},
        {L"", 118, false},
        {L"Controles", 130, true},
        {L"WASD ou setas: mover   Espaco: colocar bomba", 156, false},
        {L"E: chutar bomba (precisa do item)   T: teleportar (precisa do item)", 174, false},
        {L"F: detonar bombas-relogio (precisa do item)", 192, false},
        {L"R: reiniciar a fase atual   M: voltar ao menu   Q/ESC: sair", 210, false},
        {L"Joystick: mover, A/B bomba, X chute, Y teleporte, L/R detonar, Select menu", 228, false},
        {L"Bombas e explosao", 248, true},
        {L"A bomba explode sozinha apos alguns segundos. A chama se espalha", 268, false},
        {L"em cruz e atravessa quantos blocos frageis o nivel do fogo permitir,", 286, false},
        {L"destruindo cada um deles no caminho (ate parar numa parede solida).", 304, false},
        {L"", 324, false},
        {L"Vidas", 336, true},
        {L"Voce comeca cada fase com um numero base de vidas. Morrer custa 1", 362, false},
        {L"vida e reinicia a fase atual; zerar as vidas termina a partida.", 380, false},
        {L"Dois jogadores", 410, true},
        {L"Cada um tem 3 vidas por fase; sem vidas, volta na proxima fase.", 436, false},
        {L"AMIGOS: a bomba de um nao fere o outro (a sua propria fere voce).", 454, false},
        {L"INIMIGOS: todos contra todos; o Mario ajuda quem o libertar primeiro.", 472, false},
        {L"Itens valem so p/ quem pega. P2 nas setas: I chute, O teleporte, P detonar.", 490, false},
        {L"Inimigos", 520, true},
        {L"Facil: um inimigo solta bombas; Medio: metade; Dificil: 75%.", 546, false},
        {L"A bomba de inimigo tem arco roxo e fere so os jogadores.", 564, false},
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
    const Gdiplus::Color titulo(255,98,235,199), texto(255,220,230,240), destaque(255,252,187,65);
    desenharTexto(g,L"COMO FUNCIONA A PONTUACAO",44,46,30,titulo,true);
    desenharTexto(g,L"Os pontos entram quando voce realiza uma acao. Caminhar sozinho nao soma nem desconta.",44,93,14,texto);
    painel(g,44,145,480,255);
    desenharTexto(g,L"RECOMPENSAS IMEDIATAS",63,164,20,destaque);
    desenharTexto(g,L"+ "+to_wstring(PONTOS_POR_INIMIGO)+L"   inimigo derrotado",63,206,17,texto);
    desenharTexto(g,L"+ "+to_wstring(PONTOS_POR_CAIXA)+L"     bloco quebravel destruido",63,245,17,texto);
    desenharTexto(g,L"+ "+to_wstring(PONTOS_POR_ITEM)+L"     item coletado",63,284,17,texto);
    desenharTexto(g,L"+ "+to_wstring(PONTOS_POR_BOSS)+L"   chefe derrotado",63,323,17,texto);
    painel(g,548,145,488,255);
    desenharTexto(g,L"BONUS AO ENTRAR NO PORTAL",567,164,20,destaque);
    desenharTexto(g,L"Base: "+to_wstring(PONTOS_POR_FASE)+L" pontos por fase",567,207,17,texto);
    desenharTexto(g,L"- "+to_wstring(CUSTO_POR_MOVIMENTO)+L" por movimento nesta fase",567,246,17,texto);
    desenharTexto(g,L"- "+to_wstring(CUSTO_POR_BOMBA)+L" por bomba plantada nesta fase",567,285,17,texto);
    desenharTexto(g,L"Minimo do bonus: 0 ponto",567,324,17,titulo);
    painel(g,44,423,992,256,Gdiplus::Color(255,18,27,43));
    desenharTexto(g,L"EXEMPLO",63,443,19,destaque);
    desenharTexto(g,L"40 movimentos e 4 bombas: "+to_wstring(PONTOS_POR_FASE)+L" - 40 - 20 = "+to_wstring(max(0,PONTOS_POR_FASE-40*CUSTO_POR_MOVIMENTO-4*CUSTO_POR_BOMBA))+L" pontos de bonus.",63,482,17,texto);
    desenharTexto(g,L"Andar e plantar bombas so afetam esse bonus quando a fase termina.",63,525,15,texto);
    desenharTexto(g,L"Mario pode render pontos por inimigos e blocos; acertos isolados nao pontuam.",63,559,15,texto);
    desenharTexto(g,L"2P AMIGOS: o time divide os pontos. 2P INIMIGOS: cada um soma os seus (+"+to_wstring(PONTOS_POR_INIMIGO)+L" por derrubar o rival).",63,593,15,texto);
    desenharTexto(g,L"A pontuacao total nunca fica abaixo de zero.",63,627,15,titulo);
    desenharTexto(g,L"[M ou ESC] Voltar ao menu",44,ALTURA_JOGO-49.0f,16,texto,true);
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
            case MENU_QUANTIDADE_JOGADORES: desenharQuantidadeJogadores(g); break;
            case MENU_ATRIBUICAO_CONTROLES: desenharAtribuicaoControles(g); break;
            case MENU_NOMES: desenharNomes(g); break;
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
    if(modo==MENU && (telaMenu==MENU_NOMES || telaMenu==MENU_RANKING) && editandoNome){
        wstring& nome=nomeEmEdicao();
        cursorNome=min(cursorNome,nome.size());
        bool telaNomes=telaMenu==MENU_NOMES;
        if(tecla==VK_RETURN || tecla==VK_TAB){if(telaNomes)confirmarCampoNome();else terminarRenomeacaoRanking(true);}
        else if(tecla==VK_ESCAPE){
            if(!telaNomes)terminarRenomeacaoRanking(false);
            else if(campoNome==2)editarCampoNome(1);
            else {editandoNome=false;selecionarNome=false;avisoNome.clear();telaMenu=MENU_QUANTIDADE_JOGADORES;}
        }
        else if(telaNomes && (tecla==VK_UP || tecla==VK_DOWN)){if(doisJogadores)editarCampoNome(3-campoNome);}
        else if(tecla==VK_HOME){cursorNome=0;selecionarNome=false;}
        else if(tecla==VK_END){cursorNome=nome.size();selecionarNome=false;}
        else if(tecla==VK_LEFT){if(cursorNome)cursorNome--;selecionarNome=false;}
        else if(tecla==VK_RIGHT){cursorNome=min(cursorNome+1,nome.size());selecionarNome=false;}
        else if(tecla==VK_DELETE){if(selecionarNome){nome.clear();cursorNome=0;selecionarNome=false;}else if(cursorNome<nome.size())nome.erase(cursorNome,1);}
        else if(tecla=='A' && (GetKeyState(VK_CONTROL)&0x8000))selecionarNome=true;
        return;
    }
    if (modo == MENU) {
        if(telaMenu==MENU_RANKING){
            auto r=lerRanking();int total=int(r.size());
            if(tecla==VK_UP)rankingSelecionado=max(0,rankingSelecionado-1);
            else if(tecla==VK_DOWN)rankingSelecionado=min(max(0,total-1),rankingSelecionado+1);
            else if(tecla==VK_PRIOR)rankingSelecionado=max(0,rankingSelecionado-10);
            else if(tecla==VK_NEXT)rankingSelecionado=min(max(0,total-1),rankingSelecionado+10);
            else if(tecla=='E')iniciarRenomeacaoRanking();
            else if(tecla==VK_DELETE)pedirExclusaoRanking();
            else if(tecla=='M' || tecla==VK_ESCAPE)telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu==MENU_FASES) {
            int escolha=tecla>='1' && tecla<='9' ? static_cast<int>(tecla-'0')
                       : tecla>=VK_NUMPAD1 && tecla<=VK_NUMPAD9 ? static_cast<int>(tecla-VK_NUMPAD1+1) : (tecla=='0' || tecla==VK_NUMPAD0)?10:0;
            if(escolha) faseSelecionada=escolha;
            else if(tecla==VK_LEFT) faseSelecionada=(faseSelecionada+TOTAL_FASES-2)%TOTAL_FASES+1;
            else if(tecla==VK_RIGHT) faseSelecionada=faseSelecionada%TOTAL_FASES+1;
            else if(tecla==VK_UP || tecla==VK_DOWN) faseSelecionada=(faseSelecionada+4)%TOTAL_FASES+1;
            else if(tecla==VK_RETURN){if(modoPendente!=MANUAL || nomesProntos())novaPartida(modoPendente,dificuldade);else abrirTelaNomes(MENU_FASES);}
            else if(tecla=='M') telaMenu=MENU_DIFICULDADE;
            else if(tecla==VK_ESCAPE) telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu == MENU_QUANTIDADE_JOGADORES) {
            // Tres opcoes: 1 jogador, 2 AMIGOS e 2 INIMIGOS (todos contra todos).
            int escolha=tecla>='1' && tecla<='3' ? int(tecla-'1') : tecla>=VK_NUMPAD1 && tecla<=VK_NUMPAD3 ? int(tecla-VK_NUMPAD1) : -1;
            int atual=!doisJogadores ? 0 : modoRivais ? 2 : 1;
            if(tecla==VK_LEFT || tecla==VK_UP) atual=(atual+2)%3;
            else if(tecla==VK_RIGHT || tecla==VK_DOWN) atual=(atual+1)%3;
            if(escolha>=0) atual=escolha;
            doisJogadores=atual>0; modoRivais=atual==2;
            if(escolha>=0 || tecla==VK_RETURN) abrirTelaNomes(MENU_ATRIBUICAO_CONTROLES);
            else if(tecla=='M' || tecla==VK_ESCAPE) telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu == MENU_NOMES) {
            // Fora da digitacao (ex.: clique fora do campo): volta a editar.
            if(tecla=='M' || tecla==VK_ESCAPE) telaMenu=MENU_QUANTIDADE_JOGADORES;
            else editarCampoNome(campoNome);
            return;
        }
        if (telaMenu == MENU_ATRIBUICAO_CONTROLES) {
            // Setas/A/D movem o cartao TECLADO; o direcional do joystick move o
            // cartao JOYSTICK (ver processarControleNoMenu).
            if(tecla==VK_LEFT || tecla=='A') moverDispositivo(TECLADO,-1);
            else if(tecla==VK_RIGHT || tecla=='D') moverDispositivo(TECLADO,1);
            else if(tecla=='1' || tecla==VK_NUMPAD1) escolherOpcaoControles(0);
            else if(tecla=='2' || tecla==VK_NUMPAD2) escolherOpcaoControles(1);
            else if(tecla==VK_RETURN){validarAtribuicao();jogadorEmSelecao=1;telaMenu=MENU_PERSONAGENS;}
            else if(tecla=='M' || tecla==VK_ESCAPE) abrirTelaNomes(MENU_ATRIBUICAO_CONTROLES);
            return;
        }
        if (telaMenu == MENU_PERSONAGENS) {
            if(!doisJogadores) jogadorEmSelecao=1;
            int& atual=jogadorEmSelecao==1 ? personagemP1 : personagemP2;
            int escolha = tecla >= '1' && tecla <= '4' ? static_cast<int>(tecla-'1')
                        : tecla >= VK_NUMPAD1 && tecla <= VK_NUMPAD4 ? static_cast<int>(tecla-VK_NUMPAD1) : -1;
            if (escolha>=0) { atual=escolha; if(jogadorEmSelecao==1 && doisJogadores) jogadorEmSelecao=2; }
            else if(tecla==VK_LEFT) atual=(atual+3)%4;
            else if(tecla==VK_RIGHT) atual=(atual+1)%4;
            else if(tecla==VK_TAB && doisJogadores) jogadorEmSelecao=3-jogadorEmSelecao;
            else if(tecla==VK_RETURN) {
                if(jogadorEmSelecao==1 && doisJogadores) jogadorEmSelecao=2;
                else telaMenu=MENU_DIFICULDADE;
            }
            else if(tecla=='M') telaMenu=MENU_ATRIBUICAO_CONTROLES;
            else if(tecla==VK_ESCAPE) telaMenu=MENU_PRINCIPAL;
            return;
        }
        if (telaMenu == MENU_DIFICULDADE) {
            // Aceita tanto os numeros da fileira superior quanto o teclado
            // numerico (numpad), que usa codigos de tecla virtuais diferentes.
            if (tecla == '1' || tecla == VK_NUMPAD1) { dificuldade=FACIL; telaMenu=MENU_FASES; }
            else if (tecla == '2' || tecla == VK_NUMPAD2) { dificuldade=MEDIO; telaMenu=MENU_FASES; }
            else if (tecla == '3' || tecla == VK_NUMPAD3) { dificuldade=DIFICIL; telaMenu=MENU_FASES; }
            else if (tecla == VK_LEFT) dificuldade=static_cast<Dificuldade>((dificuldade+2)%3);
            else if (tecla == VK_RIGHT) dificuldade=static_cast<Dificuldade>((dificuldade+1)%3);
            else if (tecla == VK_RETURN) telaMenu=MENU_FASES;
            else if (tecla == 'M') telaMenu = modoPendente==MANUAL ? MENU_PERSONAGENS : MENU_PRINCIPAL;
            else if (tecla == VK_ESCAPE) telaMenu = MENU_PRINCIPAL;
            return;
        }
        if (telaMenu != MENU_PRINCIPAL) {
            // Telas so-informativas (ranking, como jogar, dificuldades,
            // pontuacao): qualquer uma dessas duas teclas volta ao menu.
            if (tecla == 'M' || tecla == VK_ESCAPE) telaMenu = MENU_PRINCIPAL;
            return;
        }
        // O nome de cada jogador e pedido depois da escolha de 1 ou 2 jogadores.
        if (tecla == '1' || tecla == VK_NUMPAD1) { modoPendente = MANUAL; telaMenu = MENU_QUANTIDADE_JOGADORES; }
        else if (tecla == '2' || tecla == VK_NUMPAD2) { modoPendente = AUTOMATICO; telaMenu = MENU_DIFICULDADE; }
        else if (tecla == '3') telaMenu = MENU_COMO_JOGAR;
        else if (tecla == '4') telaMenu = MENU_DIFICULDADES_INFO;
        else if (tecla == '5') telaMenu = MENU_PONTUACAO;
        else if (tecla == '6') telaMenu = MENU_RANKING;
        else if (tecla == '7' || tecla == VK_NUMPAD7) { modoPendente=MANUAL; telaMenu=MENU_PERSONAGENS; jogadorEmSelecao=1; }
        else if (tecla == '8' || tecla == VK_NUMPAD8) { modoPendente=MANUAL; telaMenu=MENU_FASES; }
        else if (tecla == '9' || tecla == VK_NUMPAD9) iniciarModoInsano();
        else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
        return;
    }
    if (tecla == 'M') modo = MENU;
    // Espaco acompanha WASD e Enter acompanha as setas (ver teclaDoJogador).
    else if (tecla == ' ') {
        if (teclaDoJogador(false) & TECLAS_WASD) colocarBomba();
        else if (teclaDoJogador(true) & TECLAS_WASD) colocarBomba2();
    }
    // Cada jogador usa os itens que ele mesmo pegou (ver donoDasTeclas).
    else if (tecla == 'E' || tecla == 'T' || tecla == 'F' || tecla == 'I' || tecla == 'O' || tecla == 'P') {
        bool wasd = tecla == 'E' || tecla == 'T' || tecla == 'F';
        int dono = donoDasTeclas(wasd ? TECLAS_WASD : TECLAS_SETAS);
        if (dono < 0) return;
        if (tecla == 'E' || tecla == 'I') chutarBomba(dono);
        else if (tecla == 'T' || tecla == 'O') teletransportarJogador(dono);
        else detonarBombasRelogio(dono);
    }
    else if (tecla == VK_RETURN) {
        if (teclaDoJogador(true) & TECLAS_SETAS) colocarBomba2();
        else if (teclaDoJogador(false) & TECLAS_SETAS) colocarBomba();
    }
    else if (tecla == 'R') {
        if ((estado==DERROTA || estado==FIM_DE_JOGO) && tempoFimMs<DURACAO_MORTE_MS) return;
        if (estado == FIM_DE_JOGO || estado == VITORIA) novaPartida(modo, dificuldade);
        else reiniciarFase();
    }
    else if (tecla == 'Q' || tecla == VK_ESCAPE) DestroyWindow(janelaPrincipal);
}

// Botoes do joystick fora do movimento: navegacao nos menus (direcional =
// setas, A/Start = Enter, B/Select = voltar) e, na partida, itens de quem
// esta com o joystick,
// Select para o menu e Start para tentar de novo. So age no toque, nao ao segurar.
void processarControleNoMenu() {
    EntradaControle e = entradaControle, ant = controleMenuAnterior;
    controleMenuAnterior = e;
    if (!controleConectado) return;
    auto novo = [](bool agora, bool antes) { return agora && !antes; };
    bool direcaoNova = (e.dl || e.dc) && (e.dl != ant.dl || e.dc != ant.dc);
    if (modo != MENU) {
        if (estado != JOGANDO) {
            // A/B tambem plantam bomba: so valem depois que o painel final
            // aparece, para quem ainda estava apertando nao pular a tela.
            bool painelPronto = tempoFimMs >= (estado == VITORIA ? 600 : DURACAO_MORTE_MS + 400);
            if (!painelPronto) return;
            if (novo(e.voltar, ant.voltar) || novo(e.select, ant.select)) lerTeclado('M', 0);
            else if (novo(e.confirmar, ant.confirmar) || novo(e.start, ant.start)) lerTeclado('R', 0);
            return;
        }
        if (novo(e.select, ant.select)) { lerTeclado('M', 0); return; }
        int dono = controleNoJogador(1) ? 0 : controleNoJogador(2) ? 1 : -1;
        if (dono >= 0) {
            if (novo(e.chute, ant.chute)) chutarBomba(dono);
            if (novo(e.teleporte, ant.teleporte)) teletransportarJogador(dono);
            if (novo(e.detonar, ant.detonar)) detonarBombasRelogio(dono);
        }
        return;
    }
    if (telaMenu == MENU_NOMES) {
        // O nome vem do teclado; o joystick confirma, volta e troca o campo.
        if (direcaoNova && e.dl) lerTeclado(e.dl < 0 ? VK_UP : VK_DOWN, 0);
        else if (novo(e.voltar, ant.voltar) || novo(e.select, ant.select)) lerTeclado(VK_ESCAPE, 0);
        else if ((novo(e.confirmar, ant.confirmar) || novo(e.start, ant.start)) && !e.select) lerTeclado(VK_RETURN, 0);
        return;
    }
    if (editandoNome) return;
    if (telaMenu == MENU_ATRIBUICAO_CONTROLES && direcaoNova && e.dc) { moverDispositivo(JOYSTICK, e.dc); return; }
    // Voltar antes de confirmar: Start e Select juntos contam como voltar.
    if (direcaoNova) lerTeclado(e.dl < 0 ? VK_UP : e.dl > 0 ? VK_DOWN : e.dc < 0 ? VK_LEFT : VK_RIGHT, 0);
    else if (novo(e.voltar, ant.voltar) || novo(e.select, ant.select)) {
        if (telaMenu != MENU_PRINCIPAL) lerTeclado('M', 0);
    }
    else if ((novo(e.confirmar, ant.confirmar) || novo(e.start, ant.start)) && !e.select)
        lerTeclado(telaMenu == MENU_PRINCIPAL ? '1' : VK_RETURN, 0);
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

// Um quadro do jogo: controle, logica em passos fixos de 8 ms, musica e
// pedido de desenho. Chamado pelo laco do WinMain a cada PASSO_LOGICA_MS
// (e pelo WM_TIMER so quando o Windows segura esse laco).
chrono::steady_clock::time_point ultimoPassoDoJogo = chrono::steady_clock::now();
void passoDoJogo(HWND janela) {
    static auto ultimoInstante = chrono::steady_clock::now();
    ultimoPassoDoJogo = chrono::steady_clock::now();
    if(processoInsano) {
        if(WaitForSingleObject(processoInsano,0)==WAIT_OBJECT_0) {
            CloseHandle(processoInsano); processoInsano=nullptr;
            ShowWindow(janela,SW_SHOW);
            iniciarMusica();
            ultimoInstante=chrono::steady_clock::now();
        } else return;
    }
    auto agora = chrono::steady_clock::now();
    atualizarEstadoControle(janela);
    processarControleNoMenu();
    int decorrido = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(agora - ultimoInstante).count());
    ultimoInstante = agora;
    // Passos fixos pequenos: uma pausa de desenho nao muda a velocidade.
    static int acumuladoMs = 0;
    acumuladoMs += min(decorrido, 100);
    while (acumuladoMs >= 8) {
        atualizar(8);
        acumuladoMs -= 8;
    }
    if (!processoInsano) iniciarMusica();
    InvalidateRect(janela, nullptr, FALSE);
}

LRESULT CALLBACK processarMensagem(HWND janela, UINT mensagem, WPARAM wParam, LPARAM lParam) {
    switch (mensagem) {
    case WM_CREATE:
        janelaPrincipal = janela;
        atualizarEscalaDpi(janela);
        carregarAssets();
        iniciarMusica();
        atualizarEstadoControle(janela);
        SetTimer(janela, 1, PASSO_LOGICA_MS, nullptr);
        return 0;
    case WM_TIMER:
        // Reserva: arrastar/redimensionar a janela prende o laco do WinMain
        // num laco do proprio Windows; ai o jogo segue pelo timer.
        if (chrono::steady_clock::now() - ultimoPassoDoJogo > chrono::milliseconds(50)) passoDoJogo(janela);
        return 0;
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
            if(x>=44 && x<514 && y>=192 && y<267){lerTeclado('1',0);InvalidateRect(janela,nullptr,FALSE);return 0;}
            if(x>=550 && x<1036 && y>=604 && y<659) { lerTeclado('9',0); return 0; }
            if(x>=550 && x<1036 && y>=666 && y<727) { lerTeclado('8',0); return 0; }
            if(x>=550 && x<1036 && y>=738 && y<808) lerTeclado('7',0);
            else if (x >= 44 && x < 514 && y >= 287 && y < 629) {
                int linha = (y-287)/57;
                if ((y-287)%57 < 47) lerTeclado('1'+linha, 0);
            } else if (x >= 44 && x < 514 && y >= 646 && y < 737) lerTeclado('1',0);
            else if (x >= 44 && x < 210 && y >= 785 && y < 830) lerTeclado(VK_ESCAPE,0);
        } else if(modo==MENU && telaMenu==MENU_RANKING){
            if(!editandoNome){
                if(y>=150 && y<550 && x>=20 && x<1050){auto r=lerRanking();int alvo=rankingSelecionado/10*10+(y-150)/40;if(alvo<int(r.size()))rankingSelecionado=alvo;}
                else if(y>=610 && y<654){if(x>=20 && x<300)iniciarRenomeacaoRanking();else if(x>=320 && x<600)pedirExclusaoRanking();}
                else if(y>=790)lerTeclado('M',0);
            }
        } else if (modo == MENU && telaMenu == MENU_FASES) {
            if(x>=44 && x<1038 && y>=150 && y<636) {
                int coluna=(x-44)/202,linha=(y-150)/250;
                if(coluna<5 && linha<2 && (x-44)%202<184 && (y-150)%250<232) faseSelecionada=linha*5+coluna+1;
            } else if(x>=44 && x<1036 && y>=683 && y<759) lerTeclado(VK_RETURN,0);
            else if(y>=780) lerTeclado('M',0);
        } else if (modo == MENU && telaMenu == MENU_PERSONAGENS) {
            if(x>=44 && x<1038 && y>=230 && y<560) {
                int coluna=(x-44)/254;
                if(coluna<4 && (x-44)%254<232) lerTeclado('1'+coluna,0);
            } else if(y>=100 && y<180) lerTeclado(VK_TAB,0);
            else if(y>=780) lerTeclado(VK_RETURN,0);
        } else if (modo == MENU && telaMenu == MENU_QUANTIDADE_JOGADORES) {
            if(y>=182 && y<582 && x>=44 && x<1055 && (x-44)%337<317) lerTeclado('1'+(x-44)/337,0);
            else if(y>=780) lerTeclado(VK_ESCAPE,0);
        } else if (modo == MENU && telaMenu == MENU_NOMES) {
            if(x>=44 && x<604 && y>=150 && y<248) editarCampoNome(1);
            else if(doisJogadores && x>=44 && x<604 && y>=268 && y<366) editarCampoNome(2);
            else if(x>=44 && x<604 && y>=656 && y<720){if(!editandoNome)editarCampoNome(campoNome);lerTeclado(VK_RETURN,0);}
            else if(y>=780){editandoNome=false;selecionarNome=false;avisoNome.clear();telaMenu=MENU_QUANTIDADE_JOGADORES;}
        } else if (modo == MENU && telaMenu == MENU_ATRIBUICAO_CONTROLES) {
            if(y>=512 && y<576 && x>=44 && x<1036 && (x-44)%506<486) lerTeclado('1'+(x-44)/506,0);
            else if(y>=656 && y<720 && x>=44 && x<1036) lerTeclado(VK_RETURN,0);
            else if(y>=140 && y<490 && x>=44 && x<1036 && controleConectado) {
                // Clique numa coluna leva o teclado para aquele lado.
                int coluna=(x-44)/346;
                if(coluna==0) moverDispositivo(TECLADO,-1);
                else if(coluna==2 || (coluna==1 && !doisJogadores)) moverDispositivo(TECLADO,1);
            }
            else if(y>=780) lerTeclado('M',0);
        } else if (modo == MENU && telaMenu == MENU_DIFICULDADE && y >= 182 && y < 562) {
            int coluna = (x-44)/338;
            if (x>=44 && coluna<3 && (x-44)%338<316) lerTeclado('1'+coluna,0);
        }
        InvalidateRect(janela,nullptr,FALSE);
        return 0;
    }
    case WM_CHAR:
        if (modo == MENU && (telaMenu == MENU_NOMES || telaMenu==MENU_RANKING))
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
    case WM_DEVICECHANGE:
        // Joystick conectado/desconectado: procura de novo no proximo tick.
        buscarControleDireto = true;
        return TRUE;
    case WM_KILLFOCUS:
        fill(begin(teclaPressionada), end(teclaPressionada), false);
        return 0;
    case WM_DESTROY:
        if(processoInsano) { CloseHandle(processoInsano); processoInsano=nullptr; }
        executando = false;
        KillTimer(janela, 1);
        encerrarMusica();
        encerrarControle();
        liberarAssets();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wParam, lParam);
}

void tornarAplicacaoDpiAware() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

void rodarLacoDoJogo(HWND janela);

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

    rodarLacoDoJogo(janela);
    Gdiplus::GdiplusShutdown(tokenGdiPlus);
    return 0;
}

// Laco do jogo. O WM_TIMER do Windows so dispara em multiplos de ~15,6 ms:
// pedindo 16 ms, os quadros saiam alternando 15 e 31 ms e o boneco andava
// "engasgado". Aqui cada quadro sai a cada PASSO_LOGICA_MS, esperando com
// o relogio do Windows em 1 ms e acordando na hora se chegar tecla.
void rodarLacoDoJogo(HWND janela) {
    timeBeginPeriod(1);
    MSG mensagem;
    auto proximoQuadro = chrono::steady_clock::now();
    while (executando) {
        bool sair = false;
        while (PeekMessageW(&mensagem, nullptr, 0, 0, PM_REMOVE)) {
            if (mensagem.message == WM_QUIT) { sair = true; break; }
            TranslateMessage(&mensagem);
            DispatchMessageW(&mensagem);
        }
        if (sair || !executando) break;
        auto agora = chrono::steady_clock::now();
        if (agora < proximoQuadro) {
            long long falta = chrono::duration_cast<chrono::milliseconds>(proximoQuadro - agora).count();
            MsgWaitForMultipleObjects(0, nullptr, FALSE, static_cast<DWORD>(max(1LL, falta)), QS_ALLINPUT);
            continue;
        }
        proximoQuadro += chrono::milliseconds(PASSO_LOGICA_MS);
        if (proximoQuadro < agora) proximoQuadro = agora + chrono::milliseconds(PASSO_LOGICA_MS);
        passoDoJogo(janela);
        UpdateWindow(janela); // desenha ja, sem esperar a fila
    }
    timeEndPeriod(1);
}
