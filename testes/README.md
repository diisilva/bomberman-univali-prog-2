# Verificacao do polimento

Execute na raiz do projeto, com MinGW/G++ no PATH:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_polimento.cpp -o verificacao_polimento.exe -lgdiplus -lwinmm -lgdi32
.\verificacao_polimento.exe
```

O executavel deve ficar na raiz para localizar `assets/`. O teste nao abre janela,
nao envia teclas, nao toca musica e nao escreve no ranking. Usa GDI+ em memoria
para verificar recortes e exportar os previews `testes/preview_*.png`.

Verifica curvas sem cortar caminho, inimigos esperando o fim do passo, poses de
caminhada, assets finais, tres inimigos no Facil e movimento/reinicio automaticos.
Os previews permitem revisar layout; nao substituem avaliar o controle jogando.

Para reconstruir os dois atlas finais a partir do original (Python e Pillow):

```powershell
python preparar_animacoes.py
```

Os PNGs finais ja estao em `assets/`, portanto Python nao e necessario para jogar.

O teste de polimento tambem percorre o fluxo dificuldade > personagem > fase,
confere oito arenas, chefe na fase final, nova tecla com prioridade nas curvas,
portal consumido somente na chegada e animacao de morte antes do reinicio.
Gera a galeria `preview_seletor_fases.png` e todas as `preview_fase_1.png` a
`preview_fase_8.png`. Os nove quadros `preview_morte_0.png` a `preview_morte_8.png`
mostram a morte na arena.

Regressao do Modo Insano (testes originais do estudo):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -static testes/verificar_insano.cpp -o verificacao_insano.exe -lgdiplus -lgdi32 -luser32 -lshell32
.\verificacao_insano.exe
```

Inclui comportamento, movimento, colisoes e dano do boss, ritmo, controle,
combate, Mario e macas. Nenhuma janela e aberta. A validacao de fidelidade e
descrita em `modo_insano/README.md`.

Regressao de colisoes durante o movimento (sem janela nem assets):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_colisoes.cpp -o verificacao_colisoes.exe -lgdiplus -lwinmm -lgdi32
.\verificacao_colisoes.exe
```

Verifica duas casas de separacao vertical/horizontal e uma em cada eixo,
cruzamentos, inimigos e boss, casas vizinhas, chamas, imunidade, P2 cooperativo
e ausencia de dano de um P2 inativo no automatico.

Chefes da campanha e Mario aliado (na raiz, sem abrir janela):

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_boss_fase8.cpp -o verificacao_boss_fase8.exe -lgdiplus -lwinmm -lgdi32
.\verificacao_boss_fase8.exe
g++ -std=c++17 -O2 -Wall -Wextra -pedantic testes/verificar_mario_aliado.cpp -o verificacao_mario_aliado.exe -lgdiplus -lwinmm -lgdi32
.\verificacao_mario_aliado.exe
```

Confere chefes nas fases pares, avisos sem dano, ataques distintos, fogo,
movimento 3x2, bloqueio do caminho, HP e portal. Mario: bloco surpresa somente
nas fases pares, resgate, velocidade, cooldown, voo das macas, impactos,
recarga, tres vidas, imunidade e protecao do aliado/bloco pelo boss.
Polimento exporta `preview_boss_fase_2.png`, `preview_boss_fase_4.png`,
`preview_boss_fase_6.png` com Mario e `preview_boss_fogo.png` para a fase 8.

Ataques atualizados: `verificar_boss_fase8.cpp` tambem confere machados,
energia, rajada de veneno, colisao em subpassos, aviso sem dano, impacto nas
paredes, bonus, Mario/P2, poca circular e investida sem contato antecipado.
Os previews `preview_ataque_fase_{2,4,6}_{0..4}.png` mostram preparacao e voo;
`preview_veneno_poca.png` mostra o efeito persistente com sprites verdes.
