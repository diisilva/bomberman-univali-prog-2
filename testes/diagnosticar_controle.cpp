// Diagnostico do controle real usando a mesma rotina de deteccao do jogo.
// Executar na raiz do projeto. Nao abre janela visivel nem altera arquivos.
#include "../bomberman.cpp"
#include <iostream>
#include <thread>

int dispositivosEnumerados=0;
BOOL CALLBACK listarControle(const DIDEVICEINSTANCEW* info,void*){
    ++dispositivosEnumerados;
    std::cout<<"DirectInput: "<<wparaStr(info->tszProductName)<<"\n";
    return DIENUM_CONTINUE;
}

int main(int argc,char** argv){
    HWND janela=CreateWindowExW(0,L"STATIC",L"diagnostico-controle",WS_OVERLAPPED,
                                0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!janela){std::cerr<<"Falha ao criar janela invisivel: "<<GetLastError()<<"\n";return 2;}
    carregarXInput();
    bool xinput=false;
    if(obterEstadoXInput)for(DWORD i=0;i<XUSER_MAX_COUNT;++i){
        XINPUT_STATE estado{};
        if(obterEstadoXInput(i,&estado)==ERROR_SUCCESS){
            std::cout<<"XInput porta "<<i<<": conectado\n";
            xinput=true;
        }
    }
    if(!xinput)std::cout<<"XInput: nenhum controle conectado\n";
    if(!entradaDireta){
        HRESULT resultado=DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,
            IID_IDirectInput8W,reinterpret_cast<void**>(&entradaDireta),nullptr);
        if(FAILED(resultado))std::cout<<"DirectInput indisponivel: "<<std::hex<<resultado<<std::dec<<"\n";
    }
    if(entradaDireta)entradaDireta->EnumDevices(DI8DEVCLASS_GAMECTRL,listarControle,nullptr,DIEDFL_ATTACHEDONLY);
    if(!dispositivosEnumerados)std::cout<<"DirectInput: nenhum dispositivo de jogo enumerado\n";
    atualizarEstadoControle(janela);
    std::cout<<"Jogo reconheceu: "<<(controleConectado?"SIM":"NAO")<<"\n";
    if(controleConectado)std::cout<<"Caminho usado: "
        <<(origemControle==CONTROLE_XINPUT?"XInput":"DirectInput generico")
        <<" | movimento=("<<entradaControle.dl<<","<<entradaControle.dc
        <<") | bomba="<<(entradaControle.bomba?"pressionada":"solta")<<"\n";
    if(argc>1 && std::string(argv[1])=="--live" && controleDireto){
        std::cout<<"Amostrando 15 segundos; mova direcional/analogico e aperte os botoes.\n"<<std::flush;
        std::string anterior;
        for(int i=0;i<150;++i){
            DIJOYSTATE2 bruto{};
            if(SUCCEEDED(controleDireto->Poll()) && SUCCEEDED(controleDireto->GetDeviceState(sizeof(bruto),&bruto))){
                DIJOYSTATE2 normalizado=bruto;
                normalizarEstadoControle(normalizado);
                EntradaControle e=traduzirDirectInput(normalizado);
                unsigned int mascara=0;
                for(int b=0;b<16;++b)if(bruto.rgbButtons[b]&0x80)mascara|=1u<<b;
                std::ostringstream saida;
                saida<<"raw X/Y/Rx/Ry="<<bruto.lX<<"/"<<bruto.lY<<"/"<<bruto.lRx<<"/"<<bruto.lRy
                     <<" POV="<<bruto.rgdwPOV[0]<<" botoes="<<mascara
                     <<" -> jogo dl/dc/bomba="<<e.dl<<"/"<<e.dc<<"/"<<e.bomba;
                if(saida.str()!=anterior){std::cout<<saida.str()<<"\n"<<std::flush;anterior=saida.str();}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    encerrarControle();
    DestroyWindow(janela);
    return controleConectado?0:1;
}
