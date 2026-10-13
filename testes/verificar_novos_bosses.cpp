// Regressao das novas lutas, sem janela ou entradas no computador.
#define main main_colisoes
#include "verificar_colisoes.cpp"
#undef main

void prepararNovoBoss(int fase){
    prepararCenario({10,12},10,12,{8,8},8,8);
    faseAtual=fase;inimigos.clear();bossIniciado=false;bossConcluido=false;
    marioAliado=MarioAliado{};iniciarBoss();
    boss.posicao={3,3};boss.destino=boss.posicao;boss.visualL=3;boss.visualC=3;
    boss.tempoProximoMovimentoMs=100000;boss.tempoProximoAtaqueMs=100000;
}
int main(){
    assert(TOTAL_FASES==10);
    prepararNovoBoss(6);assert(larguraBoss()==2);
    assert(celulaBoss(3,4) && !celulaBoss(3,5));
    boss.destino={3,7};boss.movendo=true;
    assert(!contatoComInimigo(3,7) && !contatoComInimigo(5,3));
    prepararNovoBoss(6);criarPoderBoss();
    assert(boss.acaoEspecial==1 && projeteisBoss.empty() && linhaPoseBoss()==23);
    atualizarBoss(608);assert(linhaPoseBoss()==24 && projeteisBoss.empty());
    atualizarBoss(144);assert(boss.etapaHornet==H_LANCAR && linhaPoseBoss()==25 && projeteisBoss.size()==1);
    auto origem=projeteisBoss.front();assert(origem.avisoMs==150 && origem.origemL==4 && origem.origemC==4);
    atualizarBoss(200);assert(boss.etapaHornet==H_LANCAR);
    criarPoderBoss();assert(boss.acaoEspecial==1 && boss.sequenciaAtaque==1); // Nao interrompe a pose.
    atualizarBoss(1265);assert(projeteisBoss.size()==1 && projeteisBoss[0].retornando);
    for(int i=0;i<240 && boss.etapaHornet!=H_IDLE;++i)atualizarBoss(16);
    assert(projeteisBoss.empty() && boss.etapaHornet==H_IDLE);
    // Parede recolhe a agulha pelo fio; o segredo do Mario nao e destruido.
    prepararNovoBoss(6);marioAliado.escondido=true;marioAliado.bloco={6,7};mapa[6][7]=PAREDE_FRAGIL;
    projeteisBoss={{6.5f,6.5f,0,3,0,4,AGULHA,0}};projeteisBoss[0].origemL=6.5f;projeteisBoss[0].origemC=6.5f;
    atualizarProjeteisBoss(800);assert(projeteisBoss.empty() && marioAliado.escondido && mapa[6][7]==PAREDE_FRAGIL);
    prepararNovoBoss(6);boss.sequenciaAtaque=1;criarPoderBoss();
    assert(boss.etapaHornet==H_PREP_DASH && boss.avisoInvestidaMs==1000 && linhaPoseBoss()==9);
    float antes=boss.visualC;atualizarBoss(500);assert(boss.visualC==antes && boss.avisoInvestidaMs==500);
    atualizarBoss(500);assert(boss.etapaHornet==H_DASH);
    atualizarBoss(100);assert(boss.movendo && linhaPoseBoss()==11);
    while(boss.movendo)atualizarBoss(16);
    assert(boss.etapaHornet==H_RECUP_DASH);int fim=ANIM_HORNET[11].inicio+ANIM_HORNET[11].quantidade;
    assert(quadroAnimacaoBoss()>=ANIM_HORNET[11].inicio+2 && quadroAnimacaoBoss()<fim);
    atualizarBoss(480);assert(boss.etapaHornet==H_IDLE);
    prepararNovoBoss(6);boss.sequenciaAtaque=2;criarPoderBoss();
    assert(boss.etapaHornet==H_PREP_SALTO && linhaPoseBoss()==5);
    atualizarBoss(352);assert(boss.etapaHornet==H_SALTO && linhaPoseBoss()==6);
    atualizarBoss(560);assert(boss.etapaHornet==H_PREP_GIRO && hornetNoAr());
    assert(!contatoComInimigo(3,3) && !celulaBoss(3,3));
    atualizarBoss(432);assert(boss.etapaHornet==H_GIRO && projeteisBoss.size()==6 && linhaPoseBoss()==20);
    atualizarBoss(672);assert(boss.etapaHornet==H_POUSO && linhaPoseBoss()==7);
    atualizarBoss(512);assert(boss.etapaHornet==H_IDLE && boss.alturaHornet==0);
    prepararNovoBoss(6);boss.sequenciaAtaque=3;criarPoderBoss();
    assert(boss.etapaHornet==H_PREP_AEREO && linhaPoseBoss()==15);
    atualizarBoss(800);assert(hornetNoAr() && !contatoComInimigo(3,3));
    atualizarBoss(208);assert(boss.etapaHornet==H_AEREO && linhaPoseBoss()==16);
    while(boss.movendo)atualizarBoss(16);
    assert(boss.etapaHornet==H_RECUP_AEREO && boss.alturaHornet==48);
    atualizarBoss(480);assert(boss.alturaHornet==0 && boss.etapaHornet==H_IDLE);
    // Dano durante a guarda causa reacao, recuperacao e contra-ataque anunciado.
    prepararNovoBoss(6);boss.sequenciaAtaque=4;criarPoderBoss();assert(linhaPoseBoss()==31);
    Bomba contra;contra.ativa=true;contra.explodindo=true;contra.celulasExplosao={{3,3}};bombas.push_back(contra);
    atingirBoss();assert(boss.vida==88 && boss.etapaHornet==H_DANO && boss.contraPendenteHornet);
    atualizarBoss(480);assert(boss.etapaHornet==H_RECUP_DANO);
    atualizarBoss(432);assert(boss.etapaHornet==H_CONTRA && linhaPoseBoss()==32 && projeteisBoss.size()==4);
    atualizarBoss(640);assert(boss.etapaHornet==H_RECUP_CONTRA);
    atualizarBoss(480);assert(boss.etapaHornet==H_IDLE);
    prepararNovoBoss(10);assert(larguraBoss()==3 && vidaMaximaBoss()==129);
    jogadorVisualC=1;jogador.coluna=1;atualizarBoss(16);assert(!boss.olhandoDireita);
    jogadorVisualC=12;jogador.coluna=12;atualizarBoss(16);assert(boss.olhandoDireita);
    criarPoderBoss();assert(projeteisBoss.size()==3);
    for(auto& p:projeteisBoss){assert(p.poder==FOGO_BOWSER && p.avisoMs==700);assert(abs(p.vida-5.175f)<.001f);}
    prepararNovoBoss(10);jogadorVisualL=6;jogadorVisualC=8;jogador={6,9};
    auto previsto=anteciparAlvoBoss(6,8,4,4.5f,3);
    assert(previsto.first==6 && previsto.second>8 && previsto.second<=8.75f);
    jogador={6,8};previsto=anteciparAlvoBoss(6,8,4,4.5f,3);
    assert(previsto.first==6 && previsto.second==8);
    prepararNovoBoss(10);boss.sequenciaAtaque=1;criarPoderBoss();assert(linhaPoseBoss()==54);
    atualizarBoss(1000);assert(linhaPoseBoss()==55);
    prepararNovoBoss(10);boss.sequenciaAtaque=2;criarPoderBoss();atualizarBoss(1401);
    assert(projeteisBoss.size()==8 && projeteisBoss[0].poder==FOGO_BOWSER);
    prepararNovoBoss(10);boss.sequenciaAtaque=3;criarPoderBoss();assert(linhaPoseBoss()==48);
    atualizarBoss(500);assert(linhaPoseBoss()==49 && projeteisBoss.empty());
    atualizarBoss(251);assert(linhaPoseBoss()==50 && projeteisBoss.size()==3);
    for(auto& p:projeteisBoss)assert(p.poder==GOLPE_BOWSER);
    for(int fase:{6,10}){
        prepararNovoBoss(fase);boss.vida=4;
        Bomba b;b.ativa=true;b.explodindo=true;b.celulasExplosao={{3,3}};bombas.push_back(b);
        atingirBoss();assert(!boss.vivo && boss.morteMs==1050 && !portalAtivo && !bossConcluido);
        verificarVitoria();assert(!portalAtivo);
        atualizarBoss(1000);assert(!portalAtivo && !contatoComInimigo(3,3));
        atualizarBoss(50);assert(portalAtivo && bossConcluido);
    }
    std::cout<<"Hornet e Bowser: poses, agulha, fios, fogo, casco, contato e morte OK\n";
}
