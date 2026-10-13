// Arquivos temporarios isolados, sem janela, teclado do SO ou ranking real.
#define main main_colisoes
#include "verificar_colisoes.cpp"
#undef main

int main(){
    char pasta[MAX_PATH],arquivo[MAX_PATH];assert(GetTempPathA(MAX_PATH,pasta));
    assert(GetTempFileNameA(pasta,"bmr",0,arquivo));
    {ofstream f(arquivo);f<<"01/10/2026; Diego ;100;3;10\n02/10/2026;diego;200;4;20\n03/10/2026;Ana;150;2;5\n04/10/2026;Invalido;abc;2;3\n";}
    auto rank=lerRanking(arquivo);assert(rank.size()==2 && rank[0].pontuacao==200);
    assert(salvarRegistroPartida({"13/10/2026","DIEGO",180,1,1},arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==2 && rank[0].pontuacao==200);
    assert(salvarRegistroPartida({"13/10/2026","Diego",250,2,8},arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==2 && rank[0].pontuacao==250);
    assert(salvarRegistroPartida({"13/10/2026"," diego ",250,1,12},arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==2 && rank[0].bombas==1);
    assert(!salvarRegistroPartida({"13/10/2026","   ",300,0,0},arquivo));
    string especial=wparaStr(L"Joao_42-\u00c1;\"@!");
    assert(salvarRegistroPartida({"13/10/2026",especial,300,2,8},arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==3 && rank[0].jogador==especial);
    assert(chaveNomeRanking(wparaStr(L"\u00c1LVARO"))==chaveNomeRanking(wparaStr(L"\u00e1lvaro")));
    assert(!editarNomeRanking(especial,"Diego",arquivo));
    assert(editarNomeRanking(especial,wparaStr(L"N\u00edvel_2-!"),arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==3 && rank[0].pontuacao==300 && rank[0].jogador==wparaStr(L"N\u00edvel_2-!"));
    assert(excluirRegistroRanking(wparaStr(L"N\u00edvel_2-!"),arquivo));
    assert(!excluirRegistroRanking("Inexistente",arquivo));
    assert(salvarRegistroPartida({"13/10/2026",wparaStr(L"N\u00edvel_2-!"),0,0,0},arquivo));
    rank=lerRanking(arquivo);assert(rank.size()==3 && rank.back().pontuacao==0);
    DeleteFileA(arquivo);DeleteFileA((string(arquivo)+".bak").c_str());

    modo=MENU;telaMenu=MENU_NOMES;doisJogadores=false;campoNome=1;editandoNome=true;selecionarNome=true;nomeJogador=L"Nome antigo";cursorNome=nomeJogador.size();
    digitarNomeJogador(L'D');digitarNomeJogador(L'i');digitarNomeJogador(L'e');digitarNomeJogador(L'g');digitarNomeJogador(L'o');
    assert(nomeJogador==L"Diego");
    lerTeclado(VK_LEFT,0);digitarNomeJogador(L'X');assert(nomeJogador==L"DiegXo");
    digitarNomeJogador(L'\b');assert(nomeJogador==L"Diego");
    lerTeclado('1',0);assert(telaMenu==MENU_NOMES);digitarNomeJogador(L'1');assert(nomeJogador==L"Dieg1o");
    lerTeclado(VK_RETURN,0);assert(!editandoNome);
    nomeJogador=L"   ";assert(!confirmarNomeJogador() && editandoNome);
    nomeJogador=L"Diego";assert(confirmarNomeJogador());
    editandoNome=true;selecionarNome=true;digitarNomeJogador(L'\u00c1');digitarNomeJogador(L'_');digitarNomeJogador(L'-');digitarNomeJogador(L'7');digitarNomeJogador(L';');digitarNomeJogador(L'\"');
    assert(nomeJogador==L"\u00c1_-7;\"" && confirmarNomeJogador());

    prepararCenario({5,5},5,5,{8,8},8,8);inimigos.clear();tempoMovimentoMs=0;
    pontos=100;estatisticas=EstatisticasPartida{};moverJogador(0,1);
    assert(pontos==100 && estatisticas.movimentos==1 && estatisticas.movimentosFase==1);
    colocarBomba();assert(pontos==100 && estatisticas.bombasUsadas==1 && estatisticas.bombasFase==1);
    faseAtual=TOTAL_FASES;mapa[5][6]=PORTAL;avancarOuVencerSePortal({5,6});assert(pontos==594);
    estado=JOGANDO;pontos=100;estatisticas.movimentosFase=1000;avancarOuVencerSePortal({5,6});assert(pontos==100);

    prepararCenario({10,12},10,12,{8,8},8,8);inimigos.clear();faseAtual=2;pontos=100;
    marioAliado.ativo=true;marioAliado.hp=3;marioAliado.l=5;marioAliado.c=5;marioAliado.posicao={5,5};marioAliado.recarga=100;marioAliado.pensar=100;
    macasNoChao={{5,5}};atualizarMario(8);assert(pontos==100);
    macasMario.clear();boss.vivo=true;boss.vida=100;boss.visualL=2;boss.visualC=5;boss.posicao={2,5};
    macasMario={{5.5f,5.5f,6.5f,3.f,0,.65f}};marioAliado.recarga=100;atualizarMario(651);
    assert(boss.vida==97 && pontos==100);
    std::cout<<"OK: ranking unico, recordes, nomes, cursor, pontuacao estavel e bonus da fase.\n";
}
