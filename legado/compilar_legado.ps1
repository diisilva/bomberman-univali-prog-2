$ErrorActionPreference = "Stop"

$compilador = (Get-Command g++ -ErrorAction SilentlyContinue).Source
if (-not $compilador) {
    $compiladorMSYS2 = "C:\msys64\ucrt64\bin\g++.exe"
    if (Test-Path -LiteralPath $compiladorMSYS2) {
        $compilador = $compiladorMSYS2
    }
}
if (-not $compilador) {
    Write-Error "g++ nao encontrado. Consulte o README.md desta pasta."
    exit 1
}

$pastaLegado = Split-Path -Parent $MyInvocation.MyCommand.Path
$fonte = Join-Path $pastaLegado "bomberman_legado.cpp"
$executavel = Join-Path $pastaLegado "bomberman_legado.exe"

& $compilador -std=c++17 -Wall -Wextra -pedantic $fonte -o $executavel -lgdiplus -mwindows
if ($LASTEXITCODE -ne 0) {
    Write-Error "A compilacao da versao legada falhou."
    exit $LASTEXITCODE
}

Write-Host "Versao legada compilada: $executavel"
Write-Host "Execute com: .\legado\bomberman_legado.exe"
