// Verifica que, com dois jogadores, cada item vale so para quem o pegou
// (fogo, bomba extra, bomba-relogio, escudo, atravessar, chute, teleporte e
// vida), sem abrir janela.
#include "../bomberman.cpp"
#include <cassert>
#include <iostream>

void prepararArena(){
    for(auto& linha:mapa)for(auto& celula:linha)celula=VAZIO;
    modo=MANUAL;estado=JOGANDO;doisJogadores=true;modoRivais=false;
    controleConectado=false;dispositivoP1=dispositivoP2=TECLADO;
    jogador={5,5};jogadorVisualL=5;jogadorVisualC=5;direcaoJogador={0,1};
    jogador2={7,7};jogador2VisualL=7;jogador2VisualC=7;direcaoJogador2={0,1};
    tempoMovimentoMs=tempoMovimentoMs2=0;
    bombas.clear();inimigos.clear();itensNoChao.clear();
    fill(begin(teclaPressionada),end(teclaPressionada),false);
    jogadorCaido[0]=jogadorCaido[1]=false;jogadoresAtingidos=0;
    vidasJogador[0]=vidasJogador[1]=VIDAS_POR_JOGADOR;imunidadeJogadorMs[0]=imunidadeJogadorMs[1]=0;
    poderes[0]=poderes[1]=PoderesJogador{};
}
void pegar(TipoItem tipo,int dono){
    Posicao p=dono==0?jogador:jogador2;
    itensNoChao.push_back({p,tipo});
    coletarItensEm(p,dono);
}

int main(){
    // Cada carta fica so com quem a pegou.
    for(int dono=0;dono<2;dono++){
        int outro=1-dono;
        prepararArena();
        pegar(RAIO,dono);pegar(BOMBA_EXTRA,dono);pegar(BOMBA_RELOGIO,dono);pegar(IMUNE_BOMBA,dono);
        pegar(ATRAVESSA_BLOCO,dono);pegar(CHUTE,dono);pegar(TELEPORTE,dono);pegar(VIDA_EXTRA,dono);
        const PoderesJogador& p=poderes[dono];
        const PoderesJogador& o=poderes[outro];
        assert(p.alcance==ALCANCE_EXPLOSAO_INICIAL+1 && o.alcance==ALCANCE_EXPLOSAO_INICIAL);
        assert(limiteBombas(dono)==2 && limiteBombas(outro)==1);
        assert(p.bombaRelogio && !o.bombaRelogio);
        assert(p.imuneAPropriaBomba && !o.imuneAPropriaBomba);
        assert(p.atravessaBloco && !o.atravessaBloco);
        assert(p.cartaChute && !o.cartaChute && p.cartaTeleporte && !o.cartaTeleporte);
        assert(vidasJogador[dono]==VIDAS_POR_JOGADOR+1 && vidasJogador[outro]==VIDAS_POR_JOGADOR);
    }

    // Bomba-relogio: so a bomba de quem tem o item espera a detonacao, e a
    // tecla de um jogador nao detona as bombas do outro.
    prepararArena();pegar(BOMBA_RELOGIO,1);
    colocarBomba();colocarBomba2();
    assert(bombas.size()==2 && bombas[0].dono==0 && !bombas[0].relogio && bombas[1].dono==1 && bombas[1].relogio);
    lerTeclado('F',0); // P1 (WASD) nao tem o item
    assert(!bombas[1].explodindo);
    lerTeclado('P',0); // P2 (setas) detona a dele
    assert(bombas[1].explodindo);
    // Dois com bomba-relogio: cada um detona so as proprias.
    prepararArena();pegar(BOMBA_RELOGIO,0);pegar(BOMBA_RELOGIO,1);
    colocarBomba();colocarBomba2();
    lerTeclado('F',0);
    assert(bombas[0].explodindo && !bombas[1].explodindo);
    // No modo inimigos a bomba-relogio de P2 tambem vale (ele tem tecla propria).
    prepararArena();modoRivais=true;pegar(BOMBA_RELOGIO,1);colocarBomba2();
    assert(bombas.size()==1 && bombas[0].relogio);

    // Alcance da explosao e do dono da bomba.
    prepararArena();pegar(RAIO,0);pegar(RAIO,0);
    colocarBomba();colocarBomba2();
    assert(alcanceDaBomba(bombas[0])==ALCANCE_EXPLOSAO_INICIAL+2 && alcanceDaBomba(bombas[1])==ALCANCE_EXPLOSAO_INICIAL);

    // Bomba extra: P1 com o item coloca 2, P2 continua limitado a 1.
    prepararArena();pegar(BOMBA_EXTRA,0);
    colocarBomba();jogador={5,6};jogadorVisualC=6;colocarBomba();
    colocarBomba2();jogador2={7,8};jogador2VisualC=8;colocarBomba2();
    assert(bombasDoDono(0)==2 && bombasDoDono(1)==1);

    // Escudo protege so o dono do item contra a propria bomba.
    auto chamaEm=[](Posicao p,int dono){Bomba b;b.ativa=b.explodindo=true;b.tempoMs=500;b.posicao=p;b.celulasExplosao={p};b.dono=dono;return b;};
    prepararArena();pegar(IMUNE_BOMBA,1);
    bombas.push_back(chamaEm(jogador2,1));atingirPersonagens(inimigos,estado,pontos);
    assert(!jogadorCaido[1]);
    bombas.push_back(chamaEm(jogador,0));atingirPersonagens(inimigos,estado,pontos);
    assert(jogadorCaido[0]);

    // Atravessar caixas: so quem pegou anda sobre bloco fragil.
    prepararArena();pegar(ATRAVESSA_BLOCO,1);
    mapa[5][6]=PAREDE_FRAGIL;mapa[7][8]=PAREDE_FRAGIL;
    moverJogador(0,1);moverJogador2(0,1);
    assert(jogador.coluna==5 && jogador2.coluna==8);

    // Chute: a tecla de P2 (I) chuta com a carta dele, na direcao dele.
    prepararArena();pegar(CHUTE,1);
    Bomba b;b.ativa=true;b.tempoMs=TEMPO_BOMBA_MS;b.posicao={7,8};b.dono=1;bombas.push_back(b);
    b.posicao={5,6};b.dono=0;bombas.push_back(b);
    lerTeclado('E',0); // P1 sem carta: nada acontece
    assert(iguais(bombas[1].posicao,{5,6}));
    lerTeclado('I',0);
    assert(bombas[0].posicao.coluna>8 && !poderes[1].cartaChute);

    // Teleporte: so P2 muda de lugar, e a carta dele e consumida.
    prepararArena();pegar(TELEPORTE,1);
    lerTeclado('T',0);
    assert(iguais(jogador,{5,5}));
    lerTeclado('O',0);
    assert(!poderes[1].cartaTeleporte && iguais(jogador,{5,5}) && !iguais(jogador2,jogador));
    assert(jogador2VisualL==jogador2.linha && jogador2VisualC==jogador2.coluna);

    // Joystick no P2: os botoes de item agem sobre P2 e o teclado (E/T/F) vale para P1.
    prepararArena();controleConectado=true;dispositivoP2=JOYSTICK;pegar(BOMBA_RELOGIO,1);pegar(BOMBA_RELOGIO,0);
    colocarBomba();colocarBomba2();
    controleMenuAnterior=EntradaControle{};entradaControle=EntradaControle{};entradaControle.detonar=true;
    processarControleNoMenu();
    assert(!bombas[0].explodindo && bombas[1].explodindo);
    lerTeclado('F',0);
    assert(bombas[0].explodindo);
    controleConectado=false;dispositivoP2=TECLADO;entradaControle=controleMenuAnterior=EntradaControle{};

    // Um jogador so: E/T/F e I/O/P agem sobre P1; trocar de fase zera tudo.
    prepararArena();doisJogadores=false;pegar(BOMBA_RELOGIO,0);pegar(VIDA_EXTRA,0);
    assert(poderes[0].bombaRelogio && vidas>0);
    faseAtual=1;avancarFase();
    assert(!poderes[0].bombaRelogio && poderes[0].alcance==ALCANCE_EXPLOSAO_INICIAL && !poderes[1].bombaRelogio);

    std::cout<<"OK: itens separados por jogador\n";
}
