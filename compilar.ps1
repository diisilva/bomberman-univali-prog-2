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

& $compilador -std=c++17 -Wall -Wextra -pedantic $arquivoFonte -o $arquivoExecutavel

if ($LASTEXITCODE -ne 0) {
    Write-Error "A compilacao falhou."
    exit $LASTEXITCODE
}

Write-Host "Compilado com sucesso: $arquivoExecutavel"
Write-Host "Execute com: .\bomberman.exe"
