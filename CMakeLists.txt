cmake_minimum_required(VERSION 3.16)
project(bomberman_univali LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Este projeto usa Win32. No Linux, configure com -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-linux.cmake em uma pasta de build nova.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(bomberman WIN32 bomberman.cpp)
target_link_libraries(bomberman PRIVATE gdiplus winmm dinput8 dxguid)

# A referencia do Insano conserva seu caminho original; gerar uma copia no build.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/modo_insano/batalha_referencia.cpp" insano)
set(raiz_relativa [=[const std::wstring root=[](){ wchar_t p[MAX_PATH]{}; GetModuleFileNameW(nullptr,p,MAX_PATH); std::wstring s=p; return s.substr(0,s.find_last_of(L"\\/")+1)+L"assets\\"; }()]=])
# O ponto e virgula separa listas no CMake; contar apenas ate a aspa final.
string(REGEX MATCHALL "const std::wstring root=L\"[^\"\r\n]*\"" raizes "${insano}")
list(LENGTH raizes total_raizes)
if(NOT total_raizes EQUAL 1)
    message(FATAL_ERROR "Nao foi possivel localizar a unica raiz dos assets do Insano.")
endif()
string(REPLACE "${raizes}" "${raiz_relativa}" insano "${insano}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/batalha_portavel.cpp" "${insano}")
add_executable(modo_insano WIN32 "${CMAKE_CURRENT_BINARY_DIR}/batalha_portavel.cpp")
target_link_libraries(modo_insano PRIVATE gdiplus gdi32 user32 shell32)

foreach(alvo bomberman modo_insano)
    if(MINGW)
        target_compile_options(${alvo} PRIVATE -Wall -Wextra -pedantic)
        target_link_options(${alvo} PRIVATE -static)
    endif()
endforeach()
if(MINGW)
    target_link_options(modo_insano PRIVATE -municode)
endif()

# Os caminhos de sprites sao relativos aos executaveis.
foreach(pasta assets assets_otimizados)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/${pasta}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
configure_file(SDL2.dll SDL2.dll COPYONLY)
