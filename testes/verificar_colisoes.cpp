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
    imuneAPropriaBomba = false; vidaPerdida = false; vidas = 3;
    registroSalvo = false; portalAtivo = false; jogadoresAtingidos=0;
}

int main() {
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
    estado=JOGANDO; imuneAPropriaBomba=true;
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
    atualizar(8); assert(estado == DERROTA && jogadoresAtingidos==2);
    prepararCenario({1,1},1,1,{5,5},5,5);
    modo=AUTOMATICO; doisJogadores=true; tempoMovimentoMs=100000;
    atualizar(8); assert(estado == JOGANDO);
    std::cout << "OK: separacao horizontal/vertical/diagonal, cruzamento, chamas e P2.\n";
    return 0;
}
