// Verifica traducao de controle, fluxo de jogadores/controles e cooperativo,
// sem abrir janela nem exigir hardware.
#include "../bomberman.cpp"
#include <cassert>
#include <iostream>

void prepararArena(){
    for(auto& linha:mapa)for(auto& celula:linha)celula=VAZIO;
    modo=MANUAL;estado=JOGANDO;
    jogador={5,5};jogadorVisualL=5;jogadorVisualC=5;
    jogador2={7,7};jogador2VisualL=7;jogador2VisualC=7;
    tempoMovimentoMs=tempoMovimentoMs2=0;
    bombas.clear();inimigos.clear();itensNoChao.clear();
    fill(begin(teclaPressionada),end(teclaPressionada),false);
    entradaControle=EntradaControle{};botaoBombaControleAnterior=false;
    jogadorCaido[0]=jogadorCaido[1]=false;jogadoresAtingidos=0;
    vidasJogador[0]=vidasJogador[1]=VIDAS_POR_JOGADOR;imunidadeJogadorMs[0]=imunidadeJogadorMs[1]=0;
}
EntradaControle direcao(int dl,int dc,bool bomba=false){EntradaControle e;e.dl=dl;e.dc=dc;e.bomba=e.confirmar=bomba;return e;}

int main(){
    // Traducoes: XInput, DirectInput e o layout usado pela SDL2.
    XINPUT_GAMEPAD xbox{};
    xbox.wButtons=XINPUT_GAMEPAD_DPAD_UP|XINPUT_GAMEPAD_A;
    auto x=traduzirXInput(xbox);
    assert(x.dl==-1 && x.dc==0 && x.bomba && x.confirmar);
    xbox.wButtons=XINPUT_GAMEPAD_B;xbox.sThumbLX=22000;xbox.sThumbLY=2000;
    x=traduzirXInput(xbox);
    assert(x.dc==1 && x.dl==0 && x.bomba && x.voltar && !x.confirmar);
    xbox.wButtons=XINPUT_GAMEPAD_X;xbox.sThumbLX=5000;xbox.sThumbLY=0;
    x=traduzirXInput(xbox);
    assert(x.dc==0 && x.dl==0 && !x.bomba && x.chute);

    DIJOYSTATE2 generico{};
    for(auto& pov:generico.rgdwPOV)pov=0xffffffff;
    generico.rgdwPOV[0]=9000;generico.rgbButtons[0]=0x80;
    auto d=traduzirDirectInput(generico);
    assert(d.dc==1 && d.bomba);
    generico.rgdwPOV[0]=0xffffffff;generico.rgbButtons[0]=0;
    faixasControle[0]={0,65535,true};faixasControle[1]={0,65535,true};
    faixasControle[2]={0,0,false};faixasControle[3]={0,0,false};
    generico.lX=32767;generico.lY=32767;
    normalizarEstadoControle(generico);
    d=traduzirDirectInput(generico);
    assert(d.dc==0 && d.dl==0 && !d.bomba);
    generico.lX=65535;generico.lY=32767;
    normalizarEstadoControle(generico);
    d=traduzirDirectInput(generico);
    assert(d.dc==1 && d.dl==0);
    // Ruido do analogico do 8BitDo pela SDL (368/-346) fica abaixo da zona morta.
    auto s=montarEntrada(false,false,false,false,368,-346,12000,false,false,false,false,false,false,false);
    assert(!s.dl && !s.dc && !s.bomba);
    s=montarEntrada(false,false,true,false,0,32000,12000,true,false,false,false,true,false,false);
    assert(s.dc==-1 && s.dl==0 && s.bomba && s.detonar); // Direcional tem prioridade.

    // Switch Pro no DirectInput continua rejeitado (leituras fantasmas).
    DIDEVICEINSTANCEW switchPro{};
    switchPro.guidProduct.Data1=(0x2009u<<16)|0x057eu;
    controleSwitchSemLeitura=false;
    assert(descobrirControleGenerico(&switchPro,nullptr)==DIENUM_CONTINUE);
    assert(controleSwitchSemLeitura);

    // Plugar o controle nao cria jogador nem move ninguem.
    prepararArena();doisJogadores=false;dispositivoP1=TECLADO;controleConectado=true;
    entradaControle=direcao(0,1,true);
    processarMovimentoContinuo();
    assert(jogador.coluna==5 && bombas.empty() && !segundoJogadorAtivo());

    // 1 jogador no teclado: WASD e setas, Espaco e Enter.
    prepararArena();teclaPressionada[VK_RIGHT]=true;
    processarMovimentoContinuo();
    assert(jogador.coluna==6);
    jogadorVisualC=6;tempoMovimentoMs=0;
    lerTeclado(VK_RETURN,0);assert(bombas.size()==1 && iguais(bombas[0].posicao,jogador));

    // 1 jogador no joystick: o controle move P1 e o teclado nao age.
    prepararArena();dispositivoP1=JOYSTICK;
    teclaPressionada['W']=true;entradaControle=direcao(1,0);
    processarMovimentoContinuo();
    assert(jogador.linha==6 && jogador.coluna==5);
    jogadorVisualL=6;tempoMovimentoMs=0;entradaControle=direcao(0,0,true);
    processarMovimentoContinuo();
    assert(bombas.size()==1 && iguais(bombas[0].posicao,jogador));
    processarMovimentoContinuo();
    assert(bombas.size()==1); // Segurar o botao nao repete a bomba.
    lerTeclado(' ',0);assert(bombas.size()==1);
    // Joystick desconectado: o teclado volta a valer para P1.
    controleConectado=false;tempoMovimentoMs=0;
    processarMovimentoContinuo();
    assert(jogador.linha==5);
    controleConectado=true;

    // 2 jogadores, P1 teclado + P2 joystick: P1 usa WASD e setas.
    prepararArena();doisJogadores=true;dispositivoP1=TECLADO;dispositivoP2=JOYSTICK;
    teclaPressionada[VK_RIGHT]=true;entradaControle=direcao(-1,0);
    processarMovimentoContinuo();
    assert(jogador.coluna==6 && jogador2.linha==6 && jogador2.coluna==7);
    assert(estatisticas.movimentosFase>=2);
    entradaControle=direcao(0,0,true);
    processarMovimentoContinuo();
    assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{7,7})); // Casa de saida de P2.
    // Limite por jogador: a bomba de P2 nao impede a de P1.
    lerTeclado(' ',0);assert(bombas.size()==2 && iguais(bombas[1].posicao,Posicao{5,5}) && bombas[1].dono==0 && bombas[0].dono==1);
    lerTeclado(' ',0);assert(bombas.size()==2); // P1 ja usou a sua

    // 2 jogadores, P1 joystick + P2 teclado: o teclado inteiro e de P2.
    prepararArena();doisJogadores=true;dispositivoP1=JOYSTICK;dispositivoP2=TECLADO;
    teclaPressionada['D']=true;entradaControle=direcao(0,-1);
    processarMovimentoContinuo();
    assert(jogador.coluna==4 && jogador2.coluna==8);
    lerTeclado(' ',0);assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{7,7}));
    entradaControle=direcao(0,0,true);processarMovimentoContinuo();
    assert(bombas.size()==2 && iguais(bombas[1].posicao,Posicao{5,5}));

    // 2 jogadores no teclado: P1 WASD/Espaco, P2 setas/Enter.
    prepararArena();controleConectado=false;dispositivoP1=dispositivoP2=TECLADO;
    teclaPressionada['D']=true;teclaPressionada[VK_DOWN]=true;
    processarMovimentoContinuo();
    assert(jogador.coluna==6 && jogador.linha==5 && jogador2.linha==8 && jogador2.coluna==7);
    lerTeclado(VK_RETURN,0);assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{7,7}));
    lerTeclado(' ',0);assert(bombas.size()==2 && iguais(bombas[1].posicao,Posicao{5,5})); // cada um com a sua
    lerTeclado(VK_RETURN,0);assert(bombas.size()==2);

    // Joystick: inverter e plantar no mesmo tick durante a caminhada.
    prepararArena();doisJogadores=false;controleConectado=true;dispositivoP1=JOYSTICK;
    entradaControle=direcao(0,1);processarMovimentoContinuo();
    avancarVisual(jogadorVisualL,jogadorVisualC,jogador,100);
    entradaControle=direcao(0,-1,true);processarMovimentoContinuo();
    assert(iguais(jogador,Posicao{5,5}) && jogadorVisualC>5);
    assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{5,5}));
    avancarVisual(jogadorVisualL,jogadorVisualC,jogador,1000);
    processarMovimentoContinuo();assert(iguais(jogador,Posicao{5,4}) && bombas.size()==1);

    // Teclado: plantar antes do tick que recebe a inversao.
    prepararArena();controleConectado=false;dispositivoP1=TECLADO;
    teclaPressionada['D']=true;processarMovimentoContinuo();
    avancarVisual(jogadorVisualL,jogadorVisualC,jogador,100);
    teclaPressionada['D']=false;teclaPressionada['A']=true;
    lerTeclado(' ',0);processarMovimentoContinuo();
    assert(iguais(jogador,Posicao{5,5}) && jogadorVisualC>5);
    assert(bombas.size()==1 && iguais(bombas[0].posicao,Posicao{5,5}));
    avancarVisual(jogadorVisualL,jogadorVisualC,jogador,1000);
    processarMovimentoContinuo();assert(iguais(jogador,Posicao{5,4}));

    // Demonstracao: o joystick nao interfere e so existe um personagem.
    prepararArena();controleConectado=true;dispositivoP1=JOYSTICK;
    novaPartida(AUTOMATICO,FACIL);
    assert(!doisJogadores && !controleNoJogador(1));

    // Fluxo do menu: JOGAR > quantidade > nomes > controles > personagens > dificuldade > fase.
    modo=MENU;telaMenu=MENU_PRINCIPAL;editandoNome=false;
    nomeJogador=L"Teste controle";nomeConfirmadoRanking=chaveNomeRanking(wparaStr(nomeJogador));
    nomeJogador2=L"Teste P2";nomeConfirmadoRanking2=chaveNomeRanking(wparaStr(nomeJogador2));
    controleConectado=true;dispositivoP1=TECLADO;
    lerTeclado('1',0);assert(telaMenu==MENU_QUANTIDADE_JOGADORES);
    lerTeclado('1',0);assert(telaMenu==MENU_NOMES && !doisJogadores && editandoNome && campoNome==1);
    lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_ATRIBUICAO_CONTROLES && !doisJogadores && !editandoNome);
    lerTeclado(VK_RIGHT,0);assert(dispositivoP1==JOYSTICK);   // teclado vai para "sem uso"
    lerTeclado(VK_LEFT,0);assert(dispositivoP1==TECLADO);
    moverDispositivo(JOYSTICK,-1);assert(dispositivoP1==JOYSTICK); // direcional do joystick
    lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_PERSONAGENS && jogadorEmSelecao==1);
    lerTeclado(VK_RIGHT,0);int escolhido=personagemP1;
    lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_DIFICULDADE && personagemP1==escolhido);
    lerTeclado(VK_RIGHT,0);lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_FASES);
    lerTeclado('3',0);lerTeclado(VK_RETURN,0);
    assert(modo==MANUAL && faseAtual==3 && !segundoJogadorAtivo() && controleNoJogador(1));

    // 2 jogadores, trocando os lados.
    modo=MENU;telaMenu=MENU_PRINCIPAL;dispositivoP1=TECLADO;
    lerTeclado('1',0);lerTeclado('2',0);
    assert(telaMenu==MENU_NOMES && doisJogadores && !modoRivais && campoNome==1);
    lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_NOMES && campoNome==2 && editandoNome);
    lerTeclado(VK_ESCAPE,0);assert(telaMenu==MENU_NOMES && campoNome==1);   // ESC volta ao campo de P1
    lerTeclado(VK_DOWN,0);assert(campoNome==2);
    lerTeclado(VK_RETURN,0);
    assert(telaMenu==MENU_ATRIBUICAO_CONTROLES && doisJogadores && dispositivoP1==TECLADO && dispositivoP2==JOYSTICK);
    lerTeclado(VK_RIGHT,0);assert(dispositivoP1==JOYSTICK && dispositivoP2==TECLADO);
    lerTeclado('1',0);assert(dispositivoP1==TECLADO && dispositivoP2==JOYSTICK);
    lerTeclado('2',0);assert(dispositivoP1==JOYSTICK && dispositivoP2==TECLADO);
    lerTeclado(VK_RETURN,0);
    lerTeclado('2',0);assert(personagemP1==1 && jogadorEmSelecao==2);
    lerTeclado('4',0);assert(personagemP2==3);
    lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_DIFICULDADE);
    lerTeclado('1',0);lerTeclado(VK_RETURN,0);
    assert(modo==MANUAL && segundoJogadorAtivo() && controleNoJogador(1) && !controleNoJogador(2));

    // Sem joystick, 2 jogadores dividem o teclado e nao ha o que mover.
    modo=MENU;telaMenu=MENU_PRINCIPAL;controleConectado=false;
    lerTeclado('1',0);lerTeclado('3',0);assert(telaMenu==MENU_NOMES && doisJogadores && modoRivais);
    lerTeclado(VK_RETURN,0);lerTeclado(VK_RETURN,0);assert(telaMenu==MENU_ATRIBUICAO_CONTROLES);
    assert(dispositivoP1==TECLADO && dispositivoP2==TECLADO);
    lerTeclado(VK_RIGHT,0);assert(dispositivoP1==TECLADO && dispositivoP2==TECLADO);
    assert(teclasDoJogador(1)==L"WASD + ESPACO" && teclasDoJogador(2)==L"SETAS + ENTER");

    // Joystick navega nos menus e move o proprio cartao na tela de controles.
    controleConectado=true;telaMenu=MENU_ATRIBUICAO_CONTROLES;doisJogadores=true;
    dispositivoP1=TECLADO;dispositivoP2=JOYSTICK;controleMenuAnterior=EntradaControle{};
    entradaControle=direcao(0,-1);processarControleNoMenu();
    assert(dispositivoP1==JOYSTICK && dispositivoP2==TECLADO);
    processarControleNoMenu();assert(dispositivoP1==JOYSTICK); // segurar nao repete
    entradaControle=direcao(0,0,true);processarControleNoMenu();
    assert(telaMenu==MENU_PERSONAGENS);
    entradaControle=EntradaControle{};entradaControle.voltar=true;processarControleNoMenu();
    assert(telaMenu==MENU_ATRIBUICAO_CONTROLES);
    // Tela de fim so com joystick: A/Start repete, B/Select vai ao menu, mas
    // so depois que o painel aparece (A/B tambem sao bomba).
    prepararArena();doisJogadores=false;dispositivoP1=JOYSTICK;controleConectado=true;
    assert(somenteJoystick() && itensNoJoystick());
    estado=DERROTA;tempoFimMs=100;controleMenuAnterior=EntradaControle{};
    entradaControle=direcao(0,0,true);processarControleNoMenu();
    assert(modo==MANUAL && estado==DERROTA); // ainda na animacao de morte
    entradaControle=EntradaControle{};processarControleNoMenu();
    tempoFimMs=DURACAO_MORTE_MS+500;
    entradaControle=direcao(0,0,true);processarControleNoMenu();
    assert(modo==MANUAL && estado==JOGANDO); // A tentou a fase de novo
    bombas.clear();
    processarMovimentoContinuo();assert(bombas.empty()); // o mesmo A nao arma bomba ao reiniciar
    entradaControle=EntradaControle{};processarMovimentoContinuo();
    entradaControle=direcao(0,0,true);processarMovimentoContinuo();assert(bombas.size()==1); // novo toque arma
    estado=FIM_DE_JOGO;tempoFimMs=DURACAO_MORTE_MS+500;
    entradaControle=EntradaControle{};processarControleNoMenu();
    entradaControle.voltar=entradaControle.bomba=true;processarControleNoMenu();
    assert(modo==MENU); // B foi ao menu
    modo=MANUAL;doisJogadores=true;dispositivoP2=TECLADO;
    assert(itensNoJoystick() && !somenteJoystick());
    // Modo AMIGOS: a bomba de um nao fere o outro; a propria continua ferindo.
    auto chamaEm=[](Posicao p,int dono){Bomba b;b.ativa=b.explodindo=true;b.tempoMs=500;b.posicao=p;b.celulasExplosao={p};b.dono=dono;return b;};
    prepararArena();doisJogadores=true;modoRivais=false;imuneAPropriaBomba=false;controleConectado=false;dispositivoP1=dispositivoP2=TECLADO;
    bombas.push_back(chamaEm(jogador2,0));atingirPersonagens(inimigos,estado,pontos);
    assert(estado==JOGANDO);
    bombas.push_back(chamaEm(jogador,0));atingirPersonagens(inimigos,estado,pontos);
    // P1 caiu: sai da fase, P2 continua jogando sozinho.
    assert(estado==JOGANDO && jogadoresAtingidos==1 && jogadorCaido[0] && !jogadorEmCampo(1) && jogadorEmCampo(2));
    bombas.clear();tempoMovimentoMs=tempoMovimentoMs2=0;
    moverJogador(0,1);moverJogador2(0,1);assert(jogador.coluna==5 && jogador2.coluna==8);
    lerTeclado(' ',0);lerTeclado(VK_RETURN,0);assert(bombas.size()==1 && bombas[0].dono==1);
    // O sobrevivente passa de fase: os dois voltam.
    faseAtual=1;modo=MANUAL;avancarFase();
    assert(faseAtual==2 && estado==JOGANDO && jogadorEmCampo(1) && jogadorEmCampo(2) && vidasJogador[0]==3);
    // 3 vidas por jogador: renasce protegido depois da queda.
    prepararArena();
    derrubarJogador(2);assert(estado==JOGANDO && vidasJogador[1]==2 && !jogadorEmCampo(2));
    tempoQuedaMs[1]=DURACAO_MORTE_MS;atualizarVidasJogadores(8);
    assert(jogadorEmCampo(2) && imunidadeJogadorMs[1]>0 && iguais({int(jogador2VisualL),int(jogador2VisualC)},jogador2));
    derrubarJogador(2);assert(vidasJogador[1]==2); // protegido logo apos renascer
    // Sem vidas: fica fora ate a proxima fase, o outro continua.
    for(int k=0;k<2;k++){imunidadeJogadorMs[1]=0;jogadorCaido[1]=false;derrubarJogador(2);}
    assert(vidasJogador[1]==0 && estado==JOGANDO);
    tempoQuedaMs[1]=DURACAO_MORTE_MS*3;atualizarVidasJogadores(8);assert(!jogadorEmCampo(2) && jogadorEmCampo(1));
    // Os dois sem vidas: fim de jogo.
    for(int k=0;k<3;k++){imunidadeJogadorMs[0]=0;jogadorCaido[0]=false;derrubarJogador(1);}
    assert(estado==FIM_DE_JOGO);

    // Modo INIMIGOS: a bomba de P1 derruba P2 e rende pontos a P1.
    prepararArena();modoRivais=true;jogadoresAtingidos=0;pontosJogador[0]=pontosJogador[1]=0;
    bombas.push_back(chamaEm(jogador2,0));atingirPersonagens(inimigos,estado,pontos);
    assert(estado==JOGANDO && jogadorCaido[1] && pontosJogador[0]==PONTOS_POR_INIMIGO && pontosJogador[1]==0);
    atingirPersonagens(inimigos,estado,pontos);assert(pontosJogador[0]==PONTOS_POR_INIMIGO); // conta uma vez
    // Inimigo derrubado pela bomba de P2 conta para P2.
    prepararArena();inimigos={{{3,3},true,100000}};inimigos[0].visualL=3;inimigos[0].visualC=3;
    bombas.push_back(chamaEm({3,3},1));atingirPersonagens(inimigos,estado,pontos);
    assert(!inimigos[0].vivo && pontosJogador[1]==PONTOS_POR_INIMIGO && estado==JOGANDO);
    // Modo INIMIGOS: o Mario fica com quem o libertou e mira o rival.
    prepararArena();marioAliado=MarioAliado{};marioAliado.escondido=true;marioAliado.bloco={5,9};macasMario.clear();
    libertarMario({5,9},1);assert(marioAliado.ativo && marioAliado.dono==1);
    marioAliado.recarga=0;lancarMacaMario();
    assert(macasMario.size()==1 && macasMario[0].tx==jogador.coluna+.5f && macasMario[0].ty==jogador.linha+.5f);
    pontosJogador[1]=0;marioAliado.recarga=100;marioAliado.pensar=100;marioAliado.imunidade=100;jogadoresAtingidos=0;
    atualizarMario(1500);
    assert(estado==JOGANDO && jogadorCaido[0] && pontosJogador[1]==PONTOS_POR_INIMIGO);
    estado=JOGANDO;marioAliado.hp=1;marioAliado.imunidade=0;ferirMario();
    assert(estado==JOGANDO && !marioAliado.ativo); // cair nao encerra a rodada
    marioAliado=MarioAliado{};macasMario.clear();
    // Bomba de inimigo: fere o jogador (mesmo com escudo) e nao fere inimigos.
    prepararArena();doisJogadores=false;modoRivais=false;imuneAPropriaBomba=true;
    inimigos={{{3,3},true,100000}};inimigos[0].visualL=3;inimigos[0].visualC=3;
    bombas.push_back(chamaEm({3,3},DONO_INIMIGO));bombas.push_back(chamaEm(jogador,DONO_INIMIGO));
    atingirPersonagens(inimigos,estado,pontos);
    assert(inimigos[0].vivo && estado==DERROTA);
    imuneAPropriaBomba=false;
    // Inimigo com a habilidade solta bomba quando o jogador esta na linha de fogo.
    prepararArena();inimigos={{{5,7},true,100000}};inimigos[0].visualL=5;inimigos[0].visualC=7;
    inimigos[0].soltaBomba=true;inimigos[0].recargaBombaMs=0;
    soltarBombasInimigos(8);
    assert(bombas.size()==1 && bombas[0].dono==DONO_INIMIGO && iguais(bombas[0].posicao,{5,7}) && inimigos[0].recargaBombaMs>0);
    assert(bombasDoDono(0)==0); // nao ocupa a vaga do jogador
    soltarBombasInimigos(8);assert(bombas.size()==1); // recarga
    prepararArena();inimigos={{{8,8},true,100000}};inimigos[0].visualL=8;inimigos[0].visualC=8;inimigos[0].soltaBomba=true;
    soltarBombasInimigos(8);assert(bombas.empty()); // fora da linha de fogo
    // Proporcao por dificuldade: Facil um, Medio metade, Dificil 75%.
    auto comBomba=[](Dificuldade d){dificuldade=d;faseAtual=1;modo=MANUAL;doisJogadores=false;montarFase();int n=0;for(auto& i:inimigos)n+=i.soltaBomba;return n;};
    assert(comBomba(FACIL)==1 && comBomba(MEDIO)==3 && comBomba(DIFICIL)==5);
    std::cout<<"OK: traducoes, 1P teclado/joystick, 2P nos dois lados, 2P teclado, sem fantasma, fallback, menu, amigos/inimigos e bombas de inimigos.\n";
}
