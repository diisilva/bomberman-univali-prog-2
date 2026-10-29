// Mede o ritmo real dos quadros do laco do jogo (rodarLacoDoJogo) com a janela,
// o desenho e a musica de verdade, numa partida andando. A janela fica fora da
// tela, sem foco, e o som mudo. Compilar na raiz (precisa de assets/).
#include <windows.h>
#include <chrono>
#include <vector>
#include <cstdio>
#include <algorithm>

std::vector<double> intervalos, custos;
std::chrono::steady_clock::time_point ultimoQuadro, inicioPasso;
int quadros = 0;
void medirQuadro(HWND janela);
// Cada quadro do laco termina com UpdateWindow: e ali que medimos.
#define UpdateWindow(j) medirQuadro(j)
#include "../bomberman.cpp"
#undef UpdateWindow

void medirQuadro(HWND janela) {
    auto agora = chrono::steady_clock::now();
    ::UpdateWindow(janela); // desenha de verdade
    auto fim = chrono::steady_clock::now();
    if (quadros > 0) intervalos.push_back(chrono::duration<double, milli>(agora - ultimoQuadro).count());
    custos.push_back(chrono::duration<double, milli>(fim - agora).count());
    ultimoQuadro = agora;
    ++quadros;
    if (quadros == 30) { faseSelecionada = 1; novaPartida(MANUAL, FACIL); }
    // Anda para os lados como um jogador segurando a tecla.
    if (quadros > 30) {
        bool direita = (quadros / 60) % 2 == 0;
        teclaPressionada['D'] = direita; teclaPressionada['A'] = !direita;
        estado = JOGANDO;
    }
    if (quadros >= 400) PostQuitMessage(0);
}

void imprimir(const char* nome, vector<double> v) {
    sort(v.begin(), v.end());
    double soma = 0; for (double x : v) soma += x;
    int irregulares = 0; for (double x : v) if (x < 12 || x > 20) irregulares++;
    printf("%-18s media %6.2f  p1 %6.2f  p50 %6.2f  p99 %6.2f  max %7.2f ms  (fora de 12-20 ms: %d de %zu)\n",
           nome, soma / v.size(), v[v.size() / 100], v[v.size() / 2], v[v.size() * 99 / 100], v.back(), irregulares, v.size());
}

int main() {
    waveOutSetVolume(reinterpret_cast<HWAVEOUT>(WAVE_MAPPER), 0);
    Gdiplus::GdiplusStartupInput entrada;
    Gdiplus::GdiplusStartup(&tokenGdiPlus, &entrada, nullptr);
    WNDCLASSW classe{};
    classe.lpfnWndProc = processarMensagem;
    classe.hInstance = GetModuleHandleW(nullptr);
    classe.lpszClassName = L"MedirLaco";
    RegisterClassW(&classe);
    HWND janela = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"MedirLaco", L"medir", WS_POPUP,
                                  -5000, -5000, 1066, 852, nullptr, nullptr, classe.hInstance, nullptr);
    ShowWindow(janela, SW_SHOWNOACTIVATE);
    janelaPrincipal = janela;
    rodarLacoDoJogo(janela);
    DestroyWindow(janela);
    for (size_t i = 0; i < custos.size(); ++i) if (custos[i] > 20) printf("quadro %zu: desenho %.1f ms\n", i, custos[i]);
    imprimir("intervalo quadro", vector<double>(intervalos.begin() + 40, intervalos.end()));
    imprimir("desenho", vector<double>(custos.begin() + 40, custos.end()));
    printf("andou ate a coluna %d (visual %.2f)\n", jogador.coluna, jogadorVisualC);
}
