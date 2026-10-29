// Verifica selecao, loop e ciclo de vida sem tocar audio nem abrir janela.
#include <windows.h>
#include <mmsystem.h>
#include <digitalv.h>
#include <cassert>
#include <string>

int aberturas = 0, reproducoes = 0, fechamentos = 0;
bool falharAbertura = false;
std::wstring ultimoCaminho;
MCIERROR WINAPI simularComando(MCIDEVICEID id, UINT comando, DWORD_PTR flags, DWORD_PTR dados) {
    if (comando == MCI_OPEN) {
        assert(id == 0 && (flags & MCI_OPEN_ELEMENT) && (flags & MCI_OPEN_TYPE));
        auto* abertura = reinterpret_cast<MCI_OPEN_PARMSW*>(dados);
        assert(std::wstring(abertura->lpstrDeviceType) == L"mpegvideo");
        ultimoCaminho = abertura->lpstrElementName;
        ++aberturas;
        if (falharAbertura) return MCIERR_FILE_NOT_FOUND;
        abertura->wDeviceID = 42;
    } else if (comando == MCI_PLAY) {
        assert(id == 42 && (flags & MCI_DGV_PLAY_REPEAT));
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

int main() {
    modo = MENU;
    iniciarMusica();
    assert(faixaMusicaAtual == L"menu.mp3");
    assert(ultimoCaminho == pastaDoExecutavel() + L"\\assets\\audio\\menu.mp3");
    iniciarMusica();
    assert(aberturas == 1 && reproducoes == 1 && fechamentos == 0);
    modo = MANUAL;
    for (int fase = 1; fase <= TOTAL_FASES; ++fase) {
        faseAtual = fase;
        iniciarMusica();
        assert(faixaMusicaAtual == (fase == 10 ? L"bowser_phase.mp3" : L"phase_" + std::to_wstring(fase) + L".mp3"));
        iniciarMusica();
        assert(aberturas == fase + 1 && reproducoes == fase + 1 && fechamentos == fase);
    }
    modo = MENU;
    iniciarMusica();
    pararMusica();
    assert(dispositivoMusica == 0 && faixaMusicaAtual.empty());
    iniciarMusica(); // Retorno do Insano reabre a faixa.
    assert(dispositivoMusica == 42);
    falharAbertura = true;
    modo = AUTOMATICO;
    faseAtual = 3;
    iniciarMusica();
    int tentativas = aberturas;
    iniciarMusica();
    assert(dispositivoMusica == 0 && aberturas == tentativas);
    pararMusica();
}
