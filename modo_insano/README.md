# Modo Insano: jogo completo do estudo

`batalha_referencia.cpp` e uma copia integral e sem edicoes de
`D:\algoritmos_prog_2\estudos_sprite\batalha.cpp` recebida nesta sessao.
O caminho informado `D:\algoritmos_prog_2\batalha.cpp` nao existia.

SHA-256 do fonte original e da copia:
`FCB858CA97A9B3CF09E32587A7E9163E40C3310E7F9DC9F577C73B8717F065D2`.

`compilar.ps1` gera `batalha_portavel.cpp` trocando **somente** a declaracao
da raiz absoluta dos assets pela pasta `assets/` ao lado do executavel.
Esse arquivo gerado nao e versionado. Nenhuma regra, constante, controle,
IA, animacao, arena, ataque, colisao, vida, pontuacao ou comportamento do boss
foi adaptado. O atlas original esta em `assets_otimizados/arenas.png`.

O resultado e `modo_insano.exe`. A opcao 9 do menu abre esse jogo e oculta
a campanha. Ao fechar a batalha, o menu reaparece e a musica retorna.
O Modo Insano conserva inclusive a janela redimensionavel e os controles:

| Tecla | Acao original |
| --- | --- |
| Enter | Alternar controle do White / automatico |
| M | Alternar controle do Mario / automatico |
| WASD | Mover o personagem controlado |
| Espaco | Bomba / maca, conforme personagem |
| T | Alternar entre as oito arenas |
| P | Pausar |
| R | Reiniciar |
| L | Camera lenta |
| Esc | Fechar e voltar ao menu principal |

O Insano preserva Mario, Ceedrun, sentinelas, quatro Bombermans, macas,
projeteis, combate e o boss original com 100 HP. Ele e independente das
regras academicas da campanha: o ranking CSV da campanha nao e alterado.

## Verificacao

- `testes/verificar_insano.cpp` copia os testes do estudo, alterando apenas
  o caminho do include. Cobre combate, ritmo, controle, reinicio e boss.
- Uma compilacao separada do fonte original sem qualquer adaptacao gerou
  a mesma imagem do Insano, pixel a pixel, na simulacao `--snapshot` de
  8 segundos na Fabrica Neon. A copia original tambem tem hash identico.
- Nenhuma janela ou teclado do usuario foi automatizado nos testes.

Reproduzir testes na raiz:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -static testes/verificar_insano.cpp -o verificacao_insano.exe -lgdiplus -lgdi32 -luser32 -lshell32
.\verificacao_insano.exe
.\modo_insano.exe --snapshot testes/preview_insano.png 8 0
```

A abertura pelo menu e o retorno ao fecha-lo precisam de avaliacao manual.
