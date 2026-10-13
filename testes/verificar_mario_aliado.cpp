// Mario aliado: parametros e ataques iguais aos do Actor original do Insano.
#define main main_colisoes
#include "verificar_colisoes.cpp"
#undef main
void prepararMario(){
    prepararCenario({10,12},10,12,{8,8},8,8);inimigos.clear();faseAtual=2;
    marioAliado.ativo=true;marioAliado.hp=3;marioAliado.posicao={5,5};
    marioAliado.l=5;marioAliado.c=5;
}
int main(){
    for(int fase=1;fase<=TOTAL_FASES;++fase){faseAtual=fase;modo=MANUAL;dificuldade=FACIL;montarFase();
        assert(marioAliado.escondido==(fase%2==0));assert(!marioAliado.ativo);
        if(marioAliado.escondido)assert(mapa[marioAliado.bloco.linha][marioAliado.bloco.coluna]==PAREDE_FRAGIL);
    }
    prepararMario();marioAliado=MarioAliado{};marioAliado.escondido=true;marioAliado.bloco={5,5};mapa[5][5]=PAREDE_FRAGIL;
    vector<Posicao> chamas;propagarExplosao(chamas,0,1,{5,4},1);
    assert(marioAliado.ativo&&!marioAliado.escondido&&marioAliado.hp==3&&marioAliado.imunidade==2);
    ferirMario();assert(marioAliado.hp==3);
    prepararMario();marioAliado.posicao={5,6};marioAliado.movendo=3;marioAliado.recarga=100;marioAliado.pensar=100;
    atualizarMario(100);assert(abs(marioAliado.c-5.3213f)<.001f && marioAliado.l==5);
    prepararMario();inimigos={{{5,8},true,100000}};inimigos[0].visualL=5;inimigos[0].visualC=8;
    lancarMacaMario();assert(macasMario.size()==1 && marioAliado.recarga==1.05f && marioAliado.pose==.28f);
    auto maca=macasMario[0];assert(maca.duracao>=.65f && maca.duracao<=1.45f && maca.tx==8.5f);
    marioAliado.recarga=100;marioAliado.pensar=100;
    atualizarMario(650);assert(!inimigos[0].vivo && macasMario.empty() && estado==JOGANDO);
    prepararMario();boss.vivo=true;boss.vida=100;boss.visualL=2;boss.visualC=5;boss.posicao={2,5};
    lancarMacaMario();marioAliado.recarga=100;marioAliado.pensar=100;atualizarMario(1451);
    assert(boss.vida==97);
    prepararMario();Bomba bomba;bomba.ativa=true;bomba.posicao={5,8};bomba.tempoMs=1000;bombas.push_back(bomba);
    macasMario={{5.5f,5.5f,8.5f,5.5f,0,.65f}};marioAliado.recarga=100;marioAliado.pensar=100;
    atualizarMario(651);assert(bombas[0].tempoMs==0);
    prepararMario();macasNoChao={{5,5}};marioAliado.recarga=100;marioAliado.pensar=100;
    atualizarMario(8);assert(macasNoChao.empty() && !macasMario.empty());
    prepararMario();ferirMario();assert(marioAliado.hp==2 && marioAliado.imunidade==1.6f);
    ferirMario();assert(marioAliado.hp==2);
    marioAliado.imunidade=0;ferirMario();assert(marioAliado.hp==1);
    marioAliado.imunidade=0;ferirMario();assert(estado==DERROTA && jogadoresAtingidos==4);
    prepararMario();boss.vivo=true;boss.posicao={5,1};boss.destino=boss.posicao;boss.visualL=5;boss.visualC=1;
    assert(!iniciarAvancoBoss(0,1,2)); // nao atravessa o aliado
    marioAliado.ativo=false;marioAliado.escondido=true;marioAliado.bloco={5,5};
    assert(!iniciarAvancoBoss(0,1,2)); // conserva o bloco da surpresa
    std::cout<<"Mario aliado: surpresa, movimento 3.21 (10% mais lento), macas, dano e tres vidas OK\n";
}
