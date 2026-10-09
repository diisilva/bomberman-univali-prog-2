$ErrorActionPreference = "Stop"

$compilador = (Get-Command g++ -ErrorAction SilentlyContinue).Source

if (-not $compilador) {
    $compiladorMSYS2 = "C:\msys64\ucrt64\bin\g++.exe"
    if (Test-Path -LiteralPath $compiladorMSYS2) {
        $compilador = $compiladorMSYS2
    }
}

if (-not $compilador) {
    Write-Error "g++ nao encontrado. Consulte os pre-requisitos no README.md."
    exit 1
}

$pastaProjeto = Split-Path -Parent $MyInvocation.MyCommand.Path
$arquivoFonte = Join-Path $pastaProjeto "bomberman.cpp"
$arquivoExecutavel = Join-Path $pastaProjeto "bomberman.exe"

& $compilador -std=c++17 -O2 -Wall -Wextra -pedantic -static $arquivoFonte -o $arquivoExecutavel -lgdiplus -lwinmm -mwindows

if ($LASTEXITCODE -ne 0) {
    Write-Error "A compilacao falhou."
    exit $LASTEXITCODE
}

Write-Host "Compilado com sucesso: $arquivoExecutavel"
$fonteInsano = Join-Path $pastaProjeto "modo_insano\batalha_referencia.cpp"
$fontePortavel = Join-Path $pastaProjeto "modo_insano\batalha_portavel.cpp"
$textoInsano = [IO.File]::ReadAllText($fonteInsano)
$padraoRaiz = 'const std::wstring root=L"[^"\r\n]*";'
$raizPortavel = 'const std::wstring root=[](){ wchar_t p[MAX_PATH]{}; GetModuleFileNameW(nullptr,p,MAX_PATH); std::wstring s=p; return s.substr(0,s.find_last_of(L"\\/" )+1)+L"assets\\"; }();'
if ([regex]::Matches($textoInsano, $padraoRaiz).Count -ne 1) {
    throw "Nao foi possivel localizar a unica raiz dos assets no fonte Insano."
}
$textoPortavel = [regex]::Replace($textoInsano, $padraoRaiz, [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $raizPortavel })
[IO.File]::WriteAllText($fontePortavel, $textoPortavel, [Text.UTF8Encoding]::new($false))
& $compilador $fontePortavel -o (Join-Path $pastaProjeto "modo_insano.exe") -std=c++17 -O2 -Wall -Wextra -municode -mwindows -static -lgdiplus -lgdi32 -luser32 -lshell32
if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar o Modo Insano." }
Write-Host "Modo Insano compilado com a mecanica original do estudo."
Write-Host "Execute com: .\bomberman.exe"
