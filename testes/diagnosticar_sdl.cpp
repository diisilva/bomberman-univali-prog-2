// Diagnostico do controle pela mesma rotina SDL2 do jogo (SDL2.dll ao lado do exe).
// Uso: .\diagnosticar_sdl.exe [segundos]   (padrao 20 s; mostra so mudancas)
#include "../bomberman.cpp"
#include <iostream>
#include <thread>

int main(int argc,char** argv){
    int segundos=argc>1?max(1,atoi(argv[1])):20;
    HWND janela=CreateWindowExW(0,L"STATIC",L"diagnostico-sdl",WS_OVERLAPPED,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    janelaPrincipal=nullptr; // sem janela de jogo: leitura nao depende de foco
    atualizarEstadoControle(janela);
    std::cout<<"SDL2.dll: "<<(sdl.carregada?"carregada":"AUSENTE (coloque SDL2.dll ao lado do exe)")<<"\n";
    std::cout<<"Controle: "<<(controleConectado?wparaStr(nomeControle):std::string("nenhum"))
             <<" | caminho: "<<(origemControle==CONTROLE_SDL?"SDL2":origemControle==CONTROLE_XINPUT?"XInput":
                                 origemControle==CONTROLE_GENERICO?"DirectInput":"-")<<"\n";
    std::cout<<"Aperte direcional, analogico, A/B/X/Y, L/R, Start e Select por "<<segundos<<" s...\n";
    std::string anterior;
    auto fim=chrono::steady_clock::now()+chrono::seconds(segundos);
    while(chrono::steady_clock::now()<fim){
        atualizarEstadoControle(janela);
        const EntradaControle& e=entradaControle;
        std::string atual=std::string(controleConectado?"conectado":"DESCONECTADO")+
            " mov=("+std::to_string(e.dl)+","+std::to_string(e.dc)+")"+
            (e.bomba?" BOMBA":"")+(e.voltar?" VOLTAR":"")+(e.chute?" CHUTE":"")+(e.teleporte?" TELEPORTE":"")+
            (e.detonar?" DETONAR":"")+(e.start?" START":"")+(e.select?" SELECT":"");
        if(atual!=anterior){std::cout<<atual<<"\n";anterior=atual;}
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    encerrarControle();
    DestroyWindow(janela);
    return controleConectado?0:1;
}
