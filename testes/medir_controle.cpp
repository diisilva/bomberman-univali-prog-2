// Separa o custo de cada parte da busca de controle (sem joystick conectado).
#include "../bomberman.cpp"
#include <cstdio>

double ms(chrono::steady_clock::time_point a, chrono::steady_clock::time_point b) {
    return chrono::duration<double, milli>(b - a).count();
}

int main() {
    HWND janela = CreateWindowW(L"STATIC", L"x", 0, 0, 0, 10, 10, nullptr, nullptr, nullptr, nullptr);
    for (int r = 0; r < 5; r++) {
        auto t0 = chrono::steady_clock::now();
        carregarSDL();
        EntradaControle e;
        bool sdlOk = lerControleSDL(e);
        auto t1 = chrono::steady_clock::now();
        carregarXInput();
        int xi = 0;
        if (obterEstadoXInput) for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) { XINPUT_STATE s{}; if (obterEstadoXInput(i, &s) == ERROR_SUCCESS) xi++; }
        auto t2 = chrono::steady_clock::now();
        if (!entradaDireta) DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<void**>(&entradaDireta), nullptr);
        auto t3 = chrono::steady_clock::now();
        entradaDireta->EnumDevices(DI8DEVCLASS_GAMECTRL, descobrirControleGenerico, janela, DIEDFL_ATTACHEDONLY);
        auto t4 = chrono::steady_clock::now();
        printf("rodada %d  SDL(%s) %7.2f  XInput(%d) %6.2f  DI create %6.2f  EnumDevices %7.2f ms\n", r,
               sdl.carregada ? (sdlOk ? "ok" : "sem ctrl") : "sem dll", ms(t0, t1), xi, ms(t1, t2), ms(t2, t3), ms(t3, t4));
        Sleep(300);
    }
}
