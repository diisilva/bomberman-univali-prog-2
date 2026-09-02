# Bomberman — versão legada

Esta é a edição de aparência clássica do projeto. Ela preserva o fundo preto, a fonte monoespaçada, as cores fortes e as molduras de texto da versão original de console, mas abre em uma janela nativa maior e usa os sprites PNG.

## Relação com a versão moderna

As duas versões compartilham a mesma lógica em `../bomberman.cpp`. O arquivo `bomberman_legado.cpp` define `VERSAO_LEGADO` antes de incluir a fonte oficial. Essa definição seleciona apenas constantes e estilos visuais da edição clássica.

Não existem duas cópias independentes das regras. Correções de movimentação, bomba, explosão, inimigos, colisão, vitória ou derrota feitas na fonte principal são usadas automaticamente pela build legada.

## Características

- células de 56×56 unidades-base;
- área útil de 840×904 pixels em escala de 100%;
- adaptação ao DPI do Windows;
- aparência inspirada no antigo console;
- PNG para jogador, inimigos e bomba;
- chão, paredes e explosão desenhados geometricamente;
- caminhos relativos que reutilizam `../assets/`;
- fallback textual se um sprite estiver ausente;
- modos manual e automático em 1,5x.

## Como compilar

Na raiz do projeto:

```powershell
powershell -ExecutionPolicy Bypass -File .\legado\compilar_legado.ps1
```

Ou dentro desta pasta:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar_legado.ps1
```

O comando equivalente é:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic bomberman_legado.cpp -o bomberman_legado.exe -lgdiplus -mwindows
```

## Como executar

Na raiz do projeto:

```powershell
.\legado\bomberman_legado.exe
```

Mantenha a pasta `assets` na raiz, um nível acima de `legado`. O executável tenta primeiro `legado/assets` e depois `../assets`.

## Controles

| Tecla | Ação |
| --- | --- |
| `1` | Modo manual |
| `2` | Modo automático 1,5x |
| `W`, `A`, `S`, `D` ou setas | Movimentar |
| `Espaço` | Colocar bomba |
| `R` | Reiniciar |
| `M` | Voltar ao menu |
| `Q` ou `Esc` | Sair |

Consulte `DEFESA.md` para a explicação acadêmica desta versão.

Para estudar o código com explicações em sequência, consulte também
`bomberman_legado_comentado.cpp`. Ele é compilável e ativa exatamente a mesma
configuração legada do arquivo de entrada normal.
