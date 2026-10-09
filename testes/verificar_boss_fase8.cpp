// Regressao do boss da campanha comparado aos parametros do estudo.
#define main main_colisoes
#include "verificar_colisoes.cpp"
#undef main

void prepararBoss() {
    prepararCenario({10,12},10,12,{8,8},8,8);
    inimigos.clear(); fogosBoss.clear(); areasBoss.clear(); bossConcluido=false; faseAtual=8;
    boss.vivo=true;boss.vida=VIDA_BOSS;
    boss.posicao={2,5};boss.destino=boss.posicao;boss.visualL=2;boss.visualC=5;
    boss.tempoProximoMovimentoMs=4000;boss.tempoProximoAtaqueMs=5000;
}
int main() {
    prepararBoss();
    assert(celulaBoss(2,5)&&celulaBoss(3,7)&&!celulaBoss(4,7));
    assert(!contatoComInimigo(5,5)&&!contatoComInimigo(3,8));
    mapa[2][8]=PAREDE_SOLIDA;
    assert(iniciarAvancoBoss(0,1,2));
    assert(mapa[2][8]==VAZIO && bloqueadoBoss({2,9}));
    assert(!podeAndarPara({2,9}));
    atualizarBoss(500);
    assert(abs(boss.visualC-5.75f)<.001f && boss.visualL==2);
    atualizarBoss(1000);
    assert(!boss.movendo && boss.posicao.coluna==7 && boss.tempoProximoMovimentoMs==4000);
    prepararBoss();
    jogador={2,8};jogadorVisualL=2;jogadorVisualC=8;
    assert(!iniciarAvancoBoss(0,1,2));
    prepararBoss();boss.tempoProximoAtaqueMs=0;
    atualizarBoss(8);assert(fogosBoss.size()==1);
    auto f=fogosBoss.front();assert(abs(hypot(f.vl,f.vc)-3.2f)<.001f);
    assert(boss.tempoProximoAtaqueMs==4500);
    float distanciaAntes=hypot(f.l-10.5f,f.c-12.5f);
    atualizarBoss(100);f=fogosBoss.front();
    assert(hypot(f.l-10.5f,f.c-12.5f)<distanciaAntes);
    fogosBoss={{10.5f,12.5f,0,0,1}};atualizarBoss(8);
    assert(estado==DERROTA && jogadoresAtingidos==1);
    prepararBoss();fogosBoss={{6.5f,6.5f,0,0,1}};mapa[6][6]=PAREDE_FRAGIL;
    atualizarBoss(8);assert(mapa[6][6]==VAZIO && fogosBoss.empty());
    prepararBoss();
    Bomba b;b.ativa=true;b.explodindo=true;b.celulasExplosao={{3,7}};
    bombas.push_back(b);atingirBoss();assert(boss.vida==96);
    bombas.push_back(b);atingirBoss();assert(boss.vida==96);
    atualizarBoss(700);atingirBoss();assert(boss.vida==92);
    boss.vida=4;boss.imunidadeMs=0;bombas.push_back(b);atingirBoss();
    assert(!boss.vivo&&bossConcluido&&portalAtivo);
    // Chefes nas quatro fases pares, portal somente depois da luta.
    for(int fase=1;fase<=8;++fase){
        prepararBoss();faseAtual=fase;boss=Boss{};bossIniciado=false;
        verificarVitoria();
        assert(boss.vivo==(fase%2==0));assert(portalAtivo==(fase%2!=0));
        if(boss.vivo)assert(boss.vida==vidaMaximaBoss());
    }
    for(int fase:{2,4,6}){
        prepararBoss();faseAtual=fase;criarPoderBoss();
        assert(areasBoss.empty() && fogosBoss.empty() && !projeteisBoss.empty());
        assert(projeteisBoss.front().poder==indiceBoss());
        assert(projeteisBoss.size()==size_t(fase==6?3:1));
        for(auto& p:projeteisBoss)assert(p.avisoMs==700);
    }
    prepararBoss();faseAtual=2;criarPoderBoss();assert(projeteisBoss.front().arma==1);
    projeteisBoss.clear();criarPoderBoss();assert(projeteisBoss.front().arma==0);
    // Aviso de lancamento nao atinge e nao se move.
    prepararBoss();faseAtual=2;criarPoderBoss();auto machado=projeteisBoss.front();
    atualizarBoss(500);assert(estado==JOGANDO && projeteisBoss.front().l==machado.l);
    atualizarBoss(300);assert(projeteisBoss.front().l>machado.l && projeteisBoss.front().idade>0);
    // Impacto respeita parede e preserva o bloco secreto do Mario.
    prepararBoss();faseAtual=2;mapa[5][6]=PAREDE_SOLIDA;
    projeteisBoss={{5.5f,5.5f,0,3,0,4,MACHADO,0}};
    atualizarProjeteisBoss(400);assert(projeteisBoss.empty() && estado==JOGANDO && mapa[5][6]==PAREDE_SOLIDA);
    prepararBoss();marioAliado.escondido=true;marioAliado.bloco={5,6};mapa[5][6]=PAREDE_FRAGIL;
    projeteisBoss={{5.5f,5.5f,0,3,0,4,VENENO,0}};
    atualizarProjeteisBoss(400);assert(projeteisBoss.empty() && mapa[5][6]==PAREDE_FRAGIL && marioAliado.escondido);
    prepararBoss();mapa[5][6]=PAREDE_BONUS;blocosBonus={{{5,6},VIDA_EXTRA}};
    projeteisBoss={{5.5f,5.5f,0,3,0,4,ENERGIA,0}};
    atualizarProjeteisBoss(400);assert(mapa[5][6]==VAZIO && itensNoChao.size()==1);
    // Contato no corpo atual, nao na casa de destino.
    prepararBoss();jogador={5,8};jogadorVisualL=5;jogadorVisualC=5;
    projeteisBoss={{5.5f,8.5f,0,0,0,1,MACHADO,0}};
    atualizarProjeteisBoss(8);assert(estado==JOGANDO);
    projeteisBoss={{5.5f,5.5f,0,0,0,1,MACHADO,0}};
    atualizarProjeteisBoss(8);assert(estado==DERROTA && jogadoresAtingidos==1);
    // Veneno deixa poca apenas ao chegar; o dano acompanha o circulo desenhado.
    prepararBoss();faseAtual=6;
    projeteisBoss={{5.5f,5.5f,0,0,0,.001f,VENENO,0}};
    atualizarProjeteisBoss(8);assert(areasBoss.size()==1);
    jogadorVisualL=5;jogadorVisualC=5;atualizarBoss(8);assert(estado==DERROTA);
    prepararBoss();areasBoss={{{5,5},0,2200,VENENO}};
    jogadorVisualL=5.7f;jogadorVisualC=5.7f;atualizarBoss(8);assert(estado==JOGANDO);
    // Investida avisa, permite escapar da rota e avanca so no eixo escolhido.
    prepararBoss();faseAtual=4;jogador={2,12};jogadorVisualL=2;jogadorVisualC=12;
    assert(prepararInvestidaBoss());assert(boss.avisoInvestidaMs==1000 && boss.destino.linha==2);
    assert(!bloqueadoBoss({2,10}));float inicio=boss.visualC;
    atualizarBoss(500);assert(boss.visualC==inicio && estado==JOGANDO);
    atualizarBoss(500);assert(boss.movendo && boss.visualC==inicio);
    atualizarBoss(100);assert(abs(boss.visualC-inicio-.48f)<.001f && boss.visualL==2);
    // Investida para antes do Mario escondido e pode atingir apenas o corpo atual.
    prepararBoss();faseAtual=4;jogador={2,12};jogadorVisualL=2;jogadorVisualC=12;
    marioAliado.escondido=true;marioAliado.bloco={2,9};assert(prepararInvestidaBoss());
    assert(boss.destino.coluna==6);
    prepararBoss();faseAtual=4;boss.movendo=true;boss.destino={2,7};criarPoderBoss();
    assert(boss.investidaPendente);atualizarBoss(1500);assert(boss.investindo && !boss.investidaPendente);
    prepararBoss();faseAtual=4;jogador={2,10};jogadorVisualL=2;jogadorVisualC=10;
    assert(prepararInvestidaBoss());atualizarBoss(1000);atualizarBoss(100);
    verificarContatoJogadores();assert(estado==JOGANDO); // destino reservado nao e contato
    atualizarBoss(500);verificarContatoJogadores();assert(estado==DERROTA);
    prepararBoss();doisJogadores=true;jogador2={5,5};jogador2VisualL=5;jogador2VisualC=5;
    projeteisBoss={{5.5f,5.5f,0,0,0,1,ENERGIA,0}};
    atualizarProjeteisBoss(8);assert(estado==DERROTA && jogadoresAtingidos==2);
    prepararBoss();marioAliado.ativo=true;marioAliado.hp=3;marioAliado.l=5;marioAliado.c=5;
    projeteisBoss={{5.5f,5.5f,0,0,0,1,MACHADO,0}};
    atualizarProjeteisBoss(8);assert(marioAliado.hp==2 && estado==JOGANDO);
    assert(!impactosBoss.empty());
    std::cout<<"Bosses 2/4/6/8 e boss fase 8: movimento, corpo, passagem, fogo e dano OK\n";
}
