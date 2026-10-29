// Verifica selecao, loop e ciclo de vida sem tocar audio nem abrir janela.
#include <windows.h>
#include <mmsystem.h>
#include <digitalv.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <string>

// Contadores atomicos: na parte da thread o MCI simulado roda fora do main.
std::atomic<int> aberturas{0}, reproducoes{0}, fechamentos{0};
bool falharAbertura = false, abrirDevagar = false, ultimaEmLoop = false;
std::wstring ultimoCaminho;
MCIERROR WINAPI simularComando(MCIDEVICEID id, UINT comando, DWORD_PTR flags, DWORD_PTR dados) {
    if (comando == MCI_OPEN) {
        assert(id == 0 && (flags & MCI_OPEN_ELEMENT) && (flags & MCI_OPEN_TYPE));
        auto* abertura = reinterpret_cast<MCI_OPEN_PARMSW*>(dados);
        assert(std::wstring(abertura->lpstrDeviceType) == L"mpegvideo");
        if (abrirDevagar) Sleep(150); // como o MCI real (~100 ms)
        ultimoCaminho = abertura->lpstrElementName;
        ++aberturas;
        if (falharAbertura) return MCIERR_FILE_NOT_FOUND;
        abertura->wDeviceID = 42;
    } else if (comando == MCI_PLAY) {
        assert(id == 42);
        ultimaEmLoop = (flags & MCI_DGV_PLAY_REPEAT) != 0;
        ++reproducoes;
    } else if (comando == MCI_CLOSE) {
        assert(id == 42);
        ++fechamentos;
    } else assert(false);
    return 0;
}
#define mciSendCommandW simularComando
#include "../bomberman.cpp"
#undef mciSendCommandW

// Parte sincrona: o que a thread de musica executa a cada pedido.
void tocarDaTela() { tocarFaixa(musicaDaTela()); }
template <typename Condicao> void esperar(Condicao pronto) {
    for (int i = 0; i < 300 && !pronto(); ++i) Sleep(10);
    assert(pronto());
}

int main() {
    modo = MENU;
    tocarDaTela();
    assert(faixaMusicaAtual == L"menu.mp3");
    assert(ultimoCaminho == pastaDoExecutavel() + L"\\assets\\audio\\menu.mp3");
    tocarDaTela();
    assert(aberturas == 1 && reproducoes == 1 && fechamentos == 0);
    modo = MANUAL;
    for (int fase = 1; fase <= TOTAL_FASES; ++fase) {
        faseAtual = fase;
        tocarDaTela();
        assert(faixaMusicaAtual == (fase == 10 ? L"bowser_phase.mp3" : L"phase_" + std::to_wstring(fase) + L".mp3"));
        tocarDaTela();
        assert(aberturas == fase + 1 && reproducoes == fase + 1 && fechamentos == fase);
    }
    modo = MENU;
    tocarDaTela();
    fecharFaixa();
    assert(dispositivoMusica == 0 && faixaMusicaAtual.empty());
    tocarDaTela(); // Retorno do Insano reabre a faixa.
    assert(dispositivoMusica == 42);
    falharAbertura = true;
    modo = AUTOMATICO;
    faseAtual = 3;
    tocarDaTela();
    int tentativas = aberturas;
    tocarDaTela();
    assert(dispositivoMusica == 0 && aberturas == tentativas);
    fecharFaixa();

    // Som de morte: toca uma vez (sem loop) no lugar da musica da fase.
    auto tocandoMorte = [] {
        return faixaMusicaAtual.rfind(L"dead.wav", 0) == 0 && !ultimaEmLoop &&
               ultimoCaminho == pastaDoExecutavel() + L"\\assets\\audio\\dead.wav";
    };
    falharAbertura = false; modo = MANUAL; doisJogadores = false; faseAtual = 2; estado = JOGANDO;
    tocarDaTela();
    assert(faixaMusicaAtual == L"phase_2.mp3" && ultimaEmLoop);
    derrubarJogador(1); // 1 jogador: fica na tela de derrota ate apertar R
    assert(estado == DERROTA);
    tocarDaTela();
    assert(tocandoMorte());
    int abertasMorte = aberturas;
    derrubarJogador(1); tocarDaTela(); // mesma morte: nao recomeca a cada quadro
    assert(aberturas == abertasMorte);
    estado = JOGANDO; tocarDaTela(); // R: renasce e a musica da fase volta
    assert(faixaMusicaAtual == L"phase_2.mp3" && ultimaEmLoop);
    estado = FIM_DE_JOGO; tocarDaTela(); // game over: so o som de morte
    assert(tocandoMorte());
    // 2 jogadores (3 vidas cada): cada queda recomeca o som; a fase volta quando ele acaba.
    doisJogadores = true; estado = JOGANDO; somMorteMs = 0;
    jogadorCaido[0] = jogadorCaido[1] = false; imunidadeJogadorMs[0] = imunidadeJogadorMs[1] = 0;
    vidasJogador[0] = vidasJogador[1] = VIDAS_POR_JOGADOR;
    tocarDaTela();
    assert(faixaMusicaAtual == L"phase_2.mp3");
    derrubarJogador(1);
    assert(estado == JOGANDO && somMorteMs == DURACAO_SOM_MORTE_MS);
    tocarDaTela();
    assert(tocandoMorte());
    abertasMorte = aberturas;
    derrubarJogador(2); tocarDaTela(); // outra queda durante o som: recomeca
    assert(tocandoMorte() && aberturas == abertasMorte + 1);
    tocarDaTela();
    assert(aberturas == abertasMorte + 1);
    somMorteMs = 0; tocarDaTela(); // som acabou (os dois ja renasceram)
    assert(faixaMusicaAtual == L"phase_2.mp3" && ultimaEmLoop);
    // Ultima vida dos dois: game over e o som de morte fica.
    jogadorCaido[0] = false; jogadorCaido[1] = true; vidasJogador[0] = 1; vidasJogador[1] = 0;
    derrubarJogador(1);
    assert(estado == FIM_DE_JOGO);
    somMorteMs = 0; tocarDaTela();
    assert(tocandoMorte());
    doisJogadores = false; estado = JOGANDO; jogadorCaido[0] = jogadorCaido[1] = false;
    fecharFaixa();

    // Thread de musica: mesmo com o MCI demorando, iniciarMusica volta na
    // hora (o jogo nao trava ao trocar de fase) e pedidos repetidos a cada
    // quadro nao reabrem a faixa.
    falharAbertura = false; abrirDevagar = true; modo = MENU;
    int abertasAntes = aberturas, tocadasAntes = reproducoes, fechadasAntes = fechamentos;
    auto inicio = std::chrono::steady_clock::now();
    for (int quadro = 0; quadro < 5; ++quadro) iniciarMusica();
    auto gasto = std::chrono::steady_clock::now() - inicio;
    assert(gasto < std::chrono::milliseconds(50) && threadMusica != nullptr);
    esperar([&] { return reproducoes == tocadasAntes + 1; });
    assert(aberturas == abertasAntes + 1 && faixaMusicaAtual == L"menu.mp3");
    // Troca de fase: fecha a do menu e abre a da fase, fora da thread do jogo.
    modo = MANUAL; faseAtual = 10;
    inicio = std::chrono::steady_clock::now();
    iniciarMusica();
    assert(std::chrono::steady_clock::now() - inicio < std::chrono::milliseconds(50));
    esperar([&] { return reproducoes == tocadasAntes + 2; });
    assert(fechamentos == fechadasAntes + 1 && faixaMusicaAtual == L"bowser_phase.mp3");
    // Modo Insano para a musica; fechar o jogo encerra a thread.
    pararMusica();
    esperar([&] { return fechamentos == fechadasAntes + 2; });
    encerrarMusica();
    assert(threadMusica == nullptr && dispositivoMusica == 0);
}
