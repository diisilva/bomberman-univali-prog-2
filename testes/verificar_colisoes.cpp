// Regressao de morte antes do contato. Sem janela, assets ou ranking.
#include "../bomberman.cpp"
#include <cassert>
#include <iostream>

void prepararCenario(Posicao pos, float l, float c, Posicao alvoInimigo, float il, float ic) {
    for (auto& linha : mapa) for (auto& celula : linha) celula = VAZIO;
    modo = MANUAL; estado = JOGANDO; doisJogadores = false;
    fill(begin(teclaPressionada), end(teclaPressionada), false);
    jogador = pos; jogadorVisualL = l; jogadorVisualC = c;
    tempoMovimentoMs = 100000;
    inimigos = {{alvoInimigo, true, 100000}};
    inimigos[0].visualL = il; inimigos[0].visualC = ic;
    bombas.clear(); itensNoChao.clear(); perigoBoss.clear(); boss = Boss{};
    marioAliado=MarioAliado{};macasMario.clear();macasNoChao.clear();areasBoss.clear();fogosBoss.clear();blocosBonus.clear();projeteisBoss.clear();impactosBoss.clear();
    poderes[0] = poderes[1] = PoderesJogador{}; vidaPerdida = false; vidas = 3;
    registroSalvo = false; portalAtivo = false; jogadoresAtingidos=0;
    jogadorCaido[0] = jogadorCaido[1] = false;
    vidasJogador[0] = vidasJogador[1] = VIDAS_POR_JOGADOR; imunidadeJogadorMs[0] = imunidadeJogadorMs[1] = 0;
}

int main() {
    // A bomba fica na casa de saida durante todo o passo, inclusive se
    // movimento e bomba forem recebidos no mesmo tick (como no joystick).
    const Posicao direcoes[] = {{0,1},{0,-1},{1,0},{-1,0}};
    for (bool p2 : {false, true}) for (Posicao d : direcoes) {
        for (float progresso : {0.f, .25f, .75f, .999f, 1.f}) {
            prepararCenario({5,5},5,5,{8,8},8,8);
            poderes[0]=poderes[1]=PoderesJogador{}; estatisticas={};
            doisJogadores=p2; jogador2={5,5}; jogador2VisualL=5; jogador2VisualC=5;
            tempoMovimentoMs=tempoMovimentoMs2=0;
            if(p2) moverJogador2(d.linha,d.coluna);
            else moverJogador(d.linha,d.coluna);
            Posicao destino{5+d.linha,5+d.coluna};
            assert(iguais(p2 ? jogador2 : jogador,destino));
            float& l=p2 ? jogador2VisualL : jogadorVisualL;
            float& c=p2 ? jogador2VisualC : jogadorVisualC;
            l=5+d.linha*progresso; c=5+d.coluna*progresso;
            if(p2) colocarBomba2(); else colocarBomba();
            Posicao esperada=progresso<1 ? Posicao{5,5} : destino;
            assert(bombas.size()==1 && iguais(bombas[0].posicao,esperada));
            assert(bombas[0].dono==(p2 ? 1 : 0));
            if(p2) colocarBomba2(); else colocarBomba();
            assert(bombas.size()==1 && estatisticas.bombasUsadas==1);
            if(progresso==0) {
                // A bomba nao bloqueia o passo em curso; ao chegar, o
                // jogador pode continuar, mas nao voltar para cima dela.
                avancarVisual(l,c,destino,1000);
                assert(visualNoDestino(l,c,destino));
                tempoMovimentoMs=tempoMovimentoMs2=0;
                if(p2) moverJogador2(-d.linha,-d.coluna);
                else moverJogador(-d.linha,-d.coluna);
                assert(iguais(p2 ? jogador2 : jogador,destino));
                if(p2) moverJogador2(d.linha,d.coluna);
                else moverJogador(d.linha,d.coluna);
                assert(iguais(p2 ? jogador2 : jogador,Posicao{5+2*d.linha,5+2*d.coluna}));
            }
        }
    }
    // Inverter antes do fim do passo nao deve completar o avanco e deixar
    // a bomba bloqueando a volta. Cobrir bomba antes/depois da inversao.
    for (bool p2 : {false,true}) for (Posicao d : direcoes)
        for (float progresso : {0.f,.1f,.5f,.9f}) for (bool bombaAntes : {false,true}) {
            prepararCenario({5,5},5,5,{8,8},8,8);
            poderes[0]=poderes[1]=PoderesJogador{}; estatisticas={};
            doisJogadores=p2; jogador2={5,5}; jogador2VisualL=5; jogador2VisualC=5;
            tempoMovimentoMs=tempoMovimentoMs2=0;
            auto mover=[&](int sentido) {
                if(p2) moverJogador2(d.linha*sentido,d.coluna*sentido);
                else moverJogador(d.linha*sentido,d.coluna*sentido);
            };
            auto plantar=[&]() { if(p2) colocarBomba2(); else colocarBomba(); };
            mover(1);
            float& l=p2 ? jogador2VisualL : jogadorVisualL;
            float& c=p2 ? jogador2VisualC : jogadorVisualC;
            l=5+d.linha*progresso; c=5+d.coluna*progresso;
            if(bombaAntes) {
                plantar();
                bombas[0].dono=p2 ? 0 : 1;
                mover(-1); // Nao vale atravessar a bomba do outro jogador.
                assert(iguais(p2 ? jogador2 : jogador,Posicao{5+d.linha,5+d.coluna}));
                bombas[0].dono=p2 ? 1 : 0;
            }
            // Curva perpendicular continua esperando o centro da casa.
            if(p2) moverJogador2(-d.coluna,d.linha);
            else moverJogador(-d.coluna,d.linha);
            assert(iguais(p2 ? jogador2 : jogador,Posicao{5+d.linha,5+d.coluna}));
            mover(-1);
            assert(iguais(p2 ? jogador2 : jogador,Posicao{5,5}));
            assert(abs(l-(5+d.linha*progresso))<.0001f);
            assert(abs(c-(5+d.coluna*progresso))<.0001f); // Sem teleporte.
            if(!bombaAntes) plantar();
            assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{5,5}));
            if(progresso>0) {
                mover(1); mover(-1); // Pode mudar de ideia mais de uma vez.
                assert(iguais(p2 ? jogador2 : jogador,Posicao{5,5}));
            }
            avancarVisual(l,c,p2 ? jogador2 : jogador,1000);
            tempoMovimentoMs=tempoMovimentoMs2=0;
            mover(-1); // Sair da bomba na direcao escolhida.
            Posicao saida{5-d.linha,5-d.coluna};
            assert(iguais(p2 ? jogador2 : jogador,saida));
            avancarVisual(l,c,saida,1000);
            tempoMovimentoMs=tempoMovimentoMs2=0;
            mover(1); // Depois de sair, a bomba volta a bloquear.
            assert(iguais(p2 ? jogador2 : jogador,saida));
        }
    // Correcao publicada no remoto: pode sair da bomba, mas nao entrar de novo.
    prepararCenario({5,5},5,5,{8,8},8,8);
    Bomba bloqueio;bloqueio.ativa=true;bloqueio.posicao={5,5};bombas.push_back(bloqueio);
    assert(podeAndarPara({5,6}));assert(!podeAndarPara({5,5}));
    // Mesmo destino logico, mas separados por duas casas na imagem.
    for (int caso = 0; caso < 3; ++caso) {
        prepararCenario({5,5}, caso==0 ? 4.f : 5.f, caso==1 ? 4.f : 5.f,
                        {5,5}, caso==2 ? 6.f : caso==0 ? 6.f : 5.f, caso==0 ? 5.f : 6.f);
        atualizar(8);
        assert(estado == JOGANDO && vidas == 3);
        for(int i=0;i<40 && estado==JOGANDO;++i) atualizar(8);
        assert(estado == DERROTA && vidas == 2);
        atualizar(8);
        assert(vidas == 2); // Uma morte desconta apenas uma vida.
    }
    // Cruzamento em sentidos opostos: destinos diferentes, contato real no meio.
    prepararCenario({5,6},5,5,{5,5},5,6);
    for(int i=0;i<40 && estado==JOGANDO;++i) atualizar(8);
    assert(estado == DERROTA);

    // Inimigo escolhe a casa do jogador; nao pode matar antes de deslizar ate ele.
    prepararCenario({5,5},5,5,{5,6},5,6);
    moverIndividualPerseguicao(inimigos[0],0);
    assert(estado == JOGANDO);

    // Casas vizinhas e diagonais livres nao sao contato entre os corpos.
    const Posicao afastados[]={{5,6},{6,5},{6,6},{5,7},{7,5}};
    for (Posicao p : afastados) {
        prepararCenario({5,5},5,5,p,static_cast<float>(p.linha),static_cast<float>(p.coluna));
        atualizar(8); assert(estado == JOGANDO);
    }
    // O boss obedece ao mesmo contato, sem dano por reservar o destino.
    prepararCenario({5,5},5,5,{8,8},8,8);
    boss.vivo=true; boss.vida=5; boss.posicao={5,5}; boss.visualL=5; boss.visualC=6;
    boss.tempoProximoMovimentoMs=100000; boss.tempoProximoAtaqueMs=100000;
    atualizar(8); assert(estado == JOGANDO);
    boss.visualC=5.2f;
    atualizar(8); assert(estado == DERROTA && jogadoresAtingidos==1);

    // Destino em chamas nao pode atingir alguem que ainda esta fora da chama.
    prepararCenario({5,5},5,4,{8,8},8,8);
    Bomba bomba; bomba.ativa=true; bomba.explodindo=true; bomba.tempoMs=650;
    bomba.celulasExplosao={{5,5}}; bombas.push_back(bomba);
    atingirPersonagens(inimigos,estado,pontos);
    assert(estado == JOGANDO);
    jogadorVisualC=5;
    atingirPersonagens(inimigos,estado,pontos);
    assert(estado == DERROTA);
    estado=JOGANDO; poderes[0].imuneAPropriaBomba=true;
    atingirPersonagens(inimigos,estado,pontos);
    assert(estado == JOGANDO);
    // Inimigo tambem nao morre antecipadamente por ter escolhido casa em chamas.
    prepararCenario({1,1},1,1,{5,5},5,4);
    bombas.push_back(bomba);
    atingirPersonagens(inimigos,estado,pontos);
    assert(inimigos[0].vivo);
    inimigos[0].visualC=5;
    atingirPersonagens(inimigos,estado,pontos);
    assert(!inimigos[0].vivo);

    // P2 participa apenas no manual cooperativo, nunca como jogador fantasma no auto.
    prepararCenario({1,1},1,1,{5,5},5,5);
    doisJogadores=true; jogador2={5,5}; jogador2VisualL=5; jogador2VisualC=5;
    // Com dois, cada um perde uma das suas vidas e o outro continua jogando.
    atualizar(8); assert(estado == JOGANDO && jogadorCaido[1] && vidasJogador[1] == 2 && vidas == 3);
    jogadorVisualL=5; jogadorVisualC=5; jogador={5,5};
    atualizar(8); assert(estado == JOGANDO && jogadorCaido[0] && vidasJogador[0] == 2 && vidas == 3);
    prepararCenario({1,1},1,1,{5,5},5,5);
    modo=AUTOMATICO; doisJogadores=true; tempoMovimentoMs=100000;
    atualizar(8); assert(estado == JOGANDO);
    std::cout << "OK: bombas na casa de saida (4 direcoes, P1/P2), separacao, cruzamento, chamas e P2.\n";
    return 0;
}
