// Verificacao isolada: sem janela, teclado, musica ou alteracao do ranking.
#include "../bomberman.cpp"
#include <cassert>
#include <iostream>

void salvarPreview(Gdiplus::Bitmap& bitmap, const wchar_t* arquivo) {
    UINT quantidade=0, tamanho=0;
    Gdiplus::GetImageEncodersSize(&quantidade, &tamanho);
    vector<unsigned char> dados(tamanho);
    auto* codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(dados.data());
    Gdiplus::GetImageEncoders(quantidade,tamanho,codecs);
    for(UINT i=0;i<quantidade;++i)
        if(wcscmp(codecs[i].MimeType,L"image/png")==0) {
            assert(bitmap.Save(arquivo,&codecs[i].Clsid,nullptr)==Gdiplus::Ok);
            return;
        }
    assert(false);
}

int main() {
    Gdiplus::GdiplusStartupInput entrada;
    assert(Gdiplus::GdiplusStartup(&tokenGdiPlus,&entrada,nullptr)==Gdiplus::Ok);
    carregarAssets();
    assert(spriteInimigoVerde);
    personagemP1=3;assert(imagemInimigoAtual()==spriteInimigoVerde.get());
    personagemP1=0;personagemP2=3;modo=MANUAL;doisJogadores=true;
    assert(imagemInimigoAtual()==spriteInimigoVerde.get());
    doisJogadores=false;assert(imagemInimigoAtual()==spriteInimigoDirecional.get());
    personagemP2=1;modo=MENU;
    assert(spriteVitoria && spriteDerrota && spriteBombaAnimada && spriteExplosaoAnimada);
    assert(spriteItens && spriteItens->GetWidth()==256);
    for(int i=0;i<4;++i) assert(personagens[i] && posesVitoria[i] && posesDerrota[i]);
    for(int i=0;i<4;++i) assert(posesDerrota[i]->GetWidth()==144 && posesDerrota[i]->GetHeight()==26);
    ajustarAreaDesenho(1920,1080);
    assert(px(LARGURA_JOGO)<=1920 && px(ALTURA_JOGO)<=1080 && margemJanelaX>0);
    ajustarAreaDesenho(1080,852);
    modo=MENU; telaMenu=MENU_PRINCIPAL;
    nomeJogador=L"Teste de interface";nomeConfirmadoRanking=chaveNomeRanking(wparaStr(nomeJogador));
    nomeJogador2=L"Segundo jogador";nomeConfirmadoRanking2=chaveNomeRanking(wparaStr(nomeJogador2));
    // Fluxo: JOGAR > quantidade > nomes > controles > personagens > dificuldade > fase.
    lerTeclado('1',0); assert(telaMenu==MENU_QUANTIDADE_JOGADORES);
    lerTeclado('2',0); assert(doisJogadores && !modoRivais && telaMenu==MENU_NOMES);
    lerTeclado(VK_RETURN,0); lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_ATRIBUICAO_CONTROLES);
    lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_PERSONAGENS);
    lerTeclado('3',0); assert(personagemP1==2);
    assert(jogadorEmSelecao==2);
    lerTeclado('4',0); assert(personagemP2==3);
    lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_DIFICULDADE);
    lerTeclado('1',0); assert(telaMenu==MENU_FASES && dificuldade==FACIL);
    lerTeclado('6',0); assert(faseSelecionada==6);
    lerTeclado(VK_RETURN,0); assert(modo==MANUAL && faseAtual==6 && segundoJogadorAtivo());
    faseSelecionada=1;
    modo=MENU; telaMenu=MENU_PRINCIPAL;
    lerTeclado('1',0); lerTeclado('1',0); assert(!doisJogadores && telaMenu==MENU_NOMES);
    lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_ATRIBUICAO_CONTROLES);
    lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_PERSONAGENS && jogadorEmSelecao==1);
    lerTeclado(VK_RETURN,0); assert(telaMenu==MENU_DIFICULDADE);
    lerTeclado('2',0); assert(telaMenu==MENU_FASES && dificuldade==MEDIO);
    lerTeclado('8',0); lerTeclado(VK_RETURN,0); assert(faseAtual==8 && !segundoJogadorAtivo());
    modo=MENU;telaMenu=MENU_FASES;lerTeclado('0',0);lerTeclado(VK_RETURN,0);assert(faseAtual==10);
    assert(any_of(inimigos.begin(),inimigos.end(),[](const Inimigo& i){return i.especial;}));
    faseSelecionada=1;
    assert(spriteVitoria->GetWidth()==144 && spriteVitoria->GetHeight()==26);
    modo=MANUAL;
    inimigos.clear(); boss=Boss{}; bombas.clear();
    for(auto& linha:mapa) for(auto& celula:linha) celula=VAZIO;
    jogador={5,5}; jogadorVisualL=5; jogadorVisualC=5;
    moverJogador(0,1);
    assert(jogador.coluna==6);
    avancarVisual(jogadorVisualL,jogadorVisualC,jogador,80);
    tempoMovimentoMs=0;
    moverJogador(-1,0); // A curva deve esperar o centro da casa.
    assert(jogador.linha==5 && jogador.coluna==6);
    assert(jogadorVisualL==5);
    for(int i=0;i<40;++i) {
        avancarVisual(jogadorVisualL,jogadorVisualC,jogador,8);
        assert(jogadorVisualL==5 && jogadorVisualC<=6);
    }
    assert(visualNoDestino(jogadorVisualL,jogadorVisualC,jogador));
    moverJogador(-1,0);
    assert(jogador.linha==4 && jogador.coluna==6);
    assert(quadroCaminhada(0,false)==1 && quadroCaminhada(.5f,true)!=quadroCaminhada(0,true));
    inimigos={{{8,8},true,0}};
    inimigos[0].visualL=8;
    inimigos[0].visualC=7.5f;
    moverInimigos(8);
    assert(inimigos[0].posicao.linha==8 && inimigos[0].posicao.coluna==8);
    inimigos.clear();
    modo=MENU;
    relogioAnimacaoMs=450;
    {
        Gdiplus::Bitmap imagem(LARGURA_JOGO,ALTURA_JOGO,PixelFormat32bppPARGB);
        Gdiplus::Graphics g(&imagem);
        g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        desenharMenu(g);
        salvarPreview(imagem,L"testes/preview_menu.png");
        vector<RegistroPontuacao> exemploRanking={{"13/10/2026","Diego_42-!",2500,15,120},{"13/10/2026",wparaStr(L"\u00c1lvaro_7;\"@!"),1800,22,150}};
        // Tela de nomes (depois da escolha de 1 ou 2 jogadores) com o TOP 10.
        {
            vector<RegistroPontuacao> top=exemploRanking;
            for(int i=0;i<10;++i) top.push_back({"13/10/2026","Jogador "+to_string(i+1),1500-i*120,10,90});
            bool doisAntesNomes=doisJogadores;telaMenu=MENU_NOMES;
            doisJogadores=false;editarCampoNome(1);selecionarNome=false;cursorNome=4;
            desenharNomes(g,top);salvarPreview(imagem,L"testes/preview_nome_cursor.png");
            doisJogadores=true;modoRivais=false;editarCampoNome(2);selecionarNome=false;
            desenharNomes(g,top);salvarPreview(imagem,L"testes/preview_nomes_2p.png");
            editandoNome=false;campoNome=1;doisJogadores=doisAntesNomes;telaMenu=MENU_PRINCIPAL;
        }
        desenharRanking(g,exemploRanking);salvarPreview(imagem,L"testes/preview_ranking.png");
        wstring nomePreview=nomeJogador;nomeOriginalRanking="Diego_42-!";nomeJogador=L"Diego_42-!";cursorNome=5;editandoNome=true;
        desenharRanking(g,exemploRanking);salvarPreview(imagem,L"testes/preview_ranking_edicao.png");editandoNome=false;nomeOriginalRanking.clear();nomeJogador=nomePreview;
        desenharPontuacaoInfo(g);salvarPreview(imagem,L"testes/preview_pontuacao.png");
        desenharPersonagens(g);
        salvarPreview(imagem,L"testes/preview_personagens.png");
        // Telas de jogadores/controles com e sem joystick (estado simulado).
        bool doisAntes=doisJogadores;
        controleConectado=true;nomeControle=L"Nintendo Switch Pro Controller";
        doisJogadores=true;modoRivais=false;desenharQuantidadeJogadores(g);salvarPreview(imagem,L"testes/preview_quantidade_jogadores.png");
        modoRivais=true;desenharQuantidadeJogadores(g);salvarPreview(imagem,L"testes/preview_quantidade_inimigos.png");
        // Partida 2P INIMIGOS: placar separado e bombas de cada um.
        novaPartida(MANUAL,DIFICIL);pontosJogador[0]=300;pontosJogador[1]=150;pontos=450;
        colocarBomba();colocarBomba2();assert(bombasDoDono(0)==1 && bombasDoDono(1)==1);
        derrubarJogador(2);tempoQuedaMs[1]=DURACAO_MORTE_MS;assert(estado==JOGANDO); // P2 fora ate a proxima fase
        fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_partida_2p_inimigos.png");
        estado=DERROTA;tempoFimMs=DURACAO_MORTE_MS+500;jogadoresAtingidos=2;
        fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);desenharFimPartida(g);
        salvarPreview(imagem,L"testes/preview_fim_2p_inimigos.png");
        modo=MENU;estado=JOGANDO;modoRivais=false;
        doisJogadores=false;dispositivoP1=JOYSTICK;desenharAtribuicaoControles(g);salvarPreview(imagem,L"testes/preview_controles_1p_joystick.png");
        doisJogadores=true;dispositivoP1=TECLADO;dispositivoP2=JOYSTICK;desenharAtribuicaoControles(g);salvarPreview(imagem,L"testes/preview_controles_2p.png");
        jogadorEmSelecao=2;desenharPersonagens(g);salvarPreview(imagem,L"testes/preview_personagens_2p.png");jogadorEmSelecao=1;
        controleConectado=false;nomeControle.clear();dispositivoP1=dispositivoP2=TECLADO;
        desenharAtribuicaoControles(g);salvarPreview(imagem,L"testes/preview_controles_sem_joystick.png");
        // Partida so no joystick: rodape, itens e painel final com botoes.
        controleConectado=true;nomeControle=L"Nintendo Switch Pro Controller";doisJogadores=false;dispositivoP1=JOYSTICK;
        novaPartida(MANUAL,FACIL);possuiCartaChute=possuiCartaTeleporte=temBombaRelogio=true;
        fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_partida_joystick.png");
        estado=DERROTA;tempoFimMs=DURACAO_MORTE_MS+500;
        fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);desenharFimPartida(g);
        salvarPreview(imagem,L"testes/preview_fim_joystick.png");
        possuiCartaChute=possuiCartaTeleporte=temBombaRelogio=false;
        controleConectado=false;nomeControle.clear();dispositivoP1=TECLADO;modo=MENU;estado=JOGANDO;
        doisJogadores=doisAntes;
        desenharSeletorFases(g);
        salvarPreview(imagem,L"testes/preview_seletor_fases.png");
        for(int fase=1;fase<=TOTAL_FASES;++fase) {
            novaPartida(MANUAL,FACIL); faseAtual=fase; montarFase();
            fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
            salvarPreview(imagem,(L"testes/preview_fase_"+to_wstring(fase)+L".png").c_str());
            if(fase==6){int anterior=personagemP1;personagemP1=3;
                fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
                salvarPreview(imagem,L"testes/preview_selva_jogador_vermelho.png");personagemP1=anterior;}
        }
        novaPartida(MANUAL,FACIL);
        for(int i=0;i<8;++i) itensNoChao.push_back({{5,i+1},static_cast<TipoItem>(i)});
        fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_recompensas.png");
        telaMenu=MENU_DIFICULDADE;
        desenharEscolhaDificuldade(g);
        salvarPreview(imagem,L"testes/preview_dificuldade.png");
        novaPartida(MANUAL,FACIL);
        assert(inimigosVivos()==3);
        colocarBomba();
        fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_partida.png");
        estado=DERROTA; jogadoresAtingidos=1; tempoFimMs=0; atualizar(200);
        assert(tempoFimMs==200);
        fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_morte_arena.png");
        for(int frame=0;frame<9;++frame) {
            tempoFimMs=frame*130;
            fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
            salvarPreview(imagem,(L"testes/preview_morte_"+to_wstring(frame)+L".png").c_str());
        }
        tempoFimMs=1400;
        desenharFimPartida(g);
        salvarPreview(imagem,L"testes/preview_derrota.png");
        estado=VITORIA; tempoFimMs=1500;
        fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g); desenharFimPartida(g);
        salvarPreview(imagem,L"testes/preview_vitoria.png");
        estado=JOGANDO; faseAtual=TOTAL_FASES; iniciarBoss();
        fundoInterface(g); desenharHUD(g); desenharMapa(g); desenharPainelLateral(g); desenharRodape(g);
        salvarPreview(imagem,L"testes/preview_boss.png");
        assert(spriteFogoBoss && spriteFogoBoss->GetLastStatus()==Gdiplus::Ok);
        fogosBoss={{6.5f,5.5f,0,0,4},{7.5f,8.5f,0,0,4}};
        desenharMapa(g);
        salvarPreview(imagem,L"testes/preview_boss_fogo.png");
        for(int fase:{2,4,6,10}){
            faseAtual=fase;montarFase();inimigos.clear();iniciarBoss();criarPoderBoss();
            assert(imagemBossAtual());
            Posicao segredo=marioAliado.bloco;
            assert(marioAliado.escondido);
            mapa[segredo.linha][segredo.coluna]=VAZIO;invalidarCacheMapa();libertarMario(segredo);
            lancarMacaMario();
            auto recorte=recortarEmBitmapCompacto(imagemBossAtual(),0,0,160,160);
            auto* b=bossAnimado()?recorte.get():dynamic_cast<Gdiplus::Bitmap*>(imagemBossAtual());
            Gdiplus::Color canto;b->GetPixel(0,0,&canto);assert(canto.GetA()==0);
            fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
            salvarPreview(imagem,(L"testes/preview_boss_fase_"+to_wstring(fase)+L".png").c_str());
        }
        // Ataques em voo, sobre terreno real mas com um corredor livre para inspecao.
        for(int fase:{2,4,6,10}){
            faseAtual=fase;montarFase();inimigos.clear();iniciarBoss();
            for(int l=1;l<LINHAS-1;++l)for(int c=1;c<COLUNAS-1;++c)mapa[l][c]=VAZIO;
            invalidarCacheMapa();marioAliado=MarioAliado{};
            boss.posicao={3,3};boss.destino=boss.posicao;boss.visualL=3;boss.visualC=3;
            boss.tempoProximoMovimentoMs=100000;boss.tempoProximoAtaqueMs=100000;
            jogador={4,11};jogadorVisualL=4;jogadorVisualC=11;
            criarPoderBoss();
            assert(spriteMachadoBoss && posesEggBoss[1]);
            for(auto& f:facoesBoss){assert(f);Gdiplus::Color canto;f->GetPixel(0,0,&canto);assert(canto.GetA()==0);}
            for(int f=0;f<4;++f)assert(energiaBoss[f] && venenoBoss[f] && impactoBoss[f]);
            for(int quadro=0;quadro<5;++quadro){
                if(quadro)atualizarBoss(quadro==1?700:250);
                if(fase==6 && quadro==4){
                    assert(!projeteisBoss.empty());
                }
                relogioAnimacaoMs=quadro*125;
                fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
                salvarPreview(imagem,(L"testes/preview_ataque_fase_"+to_wstring(fase)+L"_"+to_wstring(quadro)+L".png").c_str());
            }
            if(fase==6){areasBoss={{{6,7},0,2200,VENENO}};projeteisBoss.clear();desenharMapa(g);salvarPreview(imagem,L"testes/preview_veneno_poca.png");}
        }
        for(int fase:{6,10}){
            faseAtual=fase;montarFase();inimigos.clear();iniciarBoss();
            marioAliado=MarioAliado{};
            for(int acao=0;acao<(fase==6?4:5);++acao){
                boss.acaoEspecial=acao;boss.tempoEspecialMs=acao==1?600:acao==3?1000:acao==4?500:acao?1800:0;boss.investindo=acao==2;boss.avisoInvestidaMs=0;
                if(fase==6){const EtapaHornet etapas[]={H_IDLE,H_LANCAR,H_DASH,H_GIRO};
                    entrarEtapaHornet(etapas[acao],660);boss.tempoEtapaHornetMs=330;boss.alturaHornet=acao==3?65:0;}
                boss.relogioPoseMs=380;
                fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
                salvarPreview(imagem,(L"testes/preview_pose_boss_"+to_wstring(fase)+L"_"+to_wstring(acao)+L".png").c_str());
            }
        }
        faseAtual=10;montarFase();inimigos.clear();iniciarBoss();marioAliado=MarioAliado{};
        for(bool direita:{false,true}){
            boss.olhandoDireita=direita;
            fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
            salvarPreview(imagem,direita?L"testes/preview_bowser_direita.png":L"testes/preview_bowser_esquerda.png");
        }
        // Todas as etapas reais da Hornet, inclusive preparo, dano e recuperacao.
        faseAtual=6;montarFase();inimigos.clear();iniciarBoss();marioAliado=MarioAliado{};
        boss.posicao={3,3};boss.destino={3,7};boss.visualL=3;boss.visualC=3;
        for(int etapa=H_IDLE;etapa<=H_RECUP_DANO;++etapa){
            entrarEtapaHornet(static_cast<EtapaHornet>(etapa),1000);boss.tempoEtapaHornetMs=400;
            boss.imunidadeMs=0;boss.avisoInvestidaMs=0;boss.investindo=false;
            boss.alturaHornet=(etapa==H_AEREO || etapa==H_GIRO)?48:0;
            int linha=linhaPoseBoss(),quadro=quadroAnimacaoBoss();
            assert(quadro>=ANIM_HORNET[linha].inicio && quadro<ANIM_HORNET[linha].inicio+ANIM_HORNET[linha].quantidade);
            fundoInterface(g);desenharHUD(g);desenharMapa(g);desenharPainelLateral(g);desenharRodape(g);
            salvarPreview(imagem,(L"testes/preview_hornet_etapa_"+to_wstring(etapa)+L".png").c_str());
        }
        // O modo automatico precisa conseguir deslocar com o novo passo.
        gerador.seed(42);
        novaPartida(AUTOMATICO,FACIL);
        bool andou=false;
        for(int i=0;i<2000;++i) { atualizar(8); andou=andou || estatisticas.movimentos>0; }
        assert(andou);
        estado=DERROTA; tempoFimMs=0;
        atualizar(1000);
        assert(estado==DERROTA);
        atualizar(400);
        assert(estado==JOGANDO);
        // Portal so e consumido depois que o jogador realmente chega.
        modo=MANUAL; faseAtual=1; montarFase();
        inimigos.clear(); boss=Boss{}; bombas.clear();
        for(auto& linha:mapa) for(auto& celula:linha) celula=VAZIO;
        jogador={5,5}; jogadorVisualL=5; jogadorVisualC=5;
        portalAtivo=true; portalPosicao={5,6}; mapa[5][6]=PORTAL;
        moverJogador(0,1); assert(faseAtual==1);
        for(int i=0;i<40 && faseAtual==1;++i) atualizar(8);
        assert(faseAtual==2);
        // Morte permanece visivel na arena antes de aceitar reiniciar.
        estado=DERROTA; jogadoresAtingidos=1; tempoFimMs=200;
        lerTeclado('R',0); assert(estado==DERROTA && tempoFimMs==200);
        tempoFimMs=DURACAO_MORTE_MS;
        lerTeclado('R',0); assert(estado==JOGANDO);
        // Nova tecla tem prioridade; se a curva estiver bloqueada, segue o corredor.
        inimigos.clear(); boss=Boss{}; bombas.clear(); portalAtivo=false;
        for(auto& linha:mapa) for(auto& celula:linha) celula=VAZIO;
        jogador={5,5}; jogadorVisualL=5; jogadorVisualC=5; tempoMovimentoMs=0;
        teclaPressionada['W']=teclaPressionada['D']=true;
        ordemTecla['W']=1; ordemTecla['D']=2;
        processarMovimentoContinuo(); assert(jogador.coluna==6 && jogador.linha==5);
        jogador={5,5}; jogadorVisualL=5; jogadorVisualC=5; tempoMovimentoMs=0;
        mapa[5][6]=PAREDE_SOLIDA;
        processarMovimentoContinuo(); assert(jogador.linha==4 && jogador.coluna==5);
    }
    liberarAssets();
    Gdiplus::GdiplusShutdown(tokenGdiPlus);
    std::cout << "OK: curvas, animacao, modo automatico, assets e previews isolados.\n";
}
