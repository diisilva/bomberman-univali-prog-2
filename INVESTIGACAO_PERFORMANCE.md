# Investigação: jogo lento / input travando por vários segundos

## Sintoma relatado pelo usuário

- Movimento do personagem/inimigos parecia lento.
- Depois: apertar uma tecla de movimento e o protagonista só reagir ~5 segundos depois.
- Depois: às vezes o protagonista não se mexe, ou leva mais de 10 segundos pra responder.
- Usuário reiniciou o PC e o problema persistiu, o que descartou "processo travado em
  background" como causa (confirmado: só havia uma instância de `bomberman.exe` rodando).

## Linha do tempo do que já foi tentado

1. **Hipótese descartada: processos duplicados.** `Get-Process` mostrou só um
   `bomberman.exe` rodando, sem duplicatas. Descartado.

2. **Hipótese parcialmente certa: inimigo perseguindo andava menos quadros/ciclo que o
   aleatório.** Corrigido (`moverIndividualPerseguicao` agora anda 1-3 quadros como o
   aleatório). Isso resolveu uma percepção de lentidão no *balanceamento*, mas não o
   travamento de input.

3. **Medido: 1 thread do processo em ~97% de CPU sustentado**, mesmo parado no jogo.
   Isso apontou pra render loop consumindo CPU sem folga (o timer da lógica dispara a
   cada `PASSO_LOGICA_MS = 25ms`, mas se cada frame demora mais que isso pra renderizar,
   o processo fica sempre ocupado, sem nunca voltar a esperar mensagens do Windows a
   tempo — e é exatamente aí que o input (`WM_KEYDOWN`) fica preso na fila).

4. **Build sem otimização.** `compilar.ps1` compilava sem `-O2`. Adicionado `-O2`.
   Ajudou, mas não resolveu: CPU continuou em ~97% mesmo depois.

5. **Cache de backbuffer HDC/HBITMAP e cache de `Gdiplus::Font`** (antes recriados a
   cada frame). Ajuda real, mas pequena.

6. **Cache do terreno do tabuleiro** (195 células deixaram de ser redesenhadas via GDI+
   a cada frame; passaram a ser um bitmap pré-renderizado, só reconstruído quando uma
   parede é destruída ou o jogo reinicia). Medido com instrumentação (timestamps por
   fase): esse cache sozinho **não** reduziu o tempo por frame. O tempo continuava
   ~90-100ms/frame dentro de `desenharMapa`, mesmo com o terreno cacheado.

7. **Instrumentação cirúrgica** (timestamps em `WM_TIMER`/`WM_PAINT`, depois separando
   HUD / mapa / rodapé, depois separando só o `DrawImage` do terreno cacheado) isolou o
   custo: o `DrawImage` do terreno cacheado levava ~2ms. Sobrava ~85-90ms em algum outro
   lugar de `desenharMapa` — sprites dos inimigos/jogador/bomba (`desenharSpriteNaCelula`,
   chamado até 8x por frame: até 6 inimigos + jogador + bomba).

## Causa raiz encontrada (confirmada por evidência no git, não só suposição)

```
$ git log --oneline --follow -- assets/perfil.png
3ea2816 feat: atualiza sprite dos inimigos para versão V2
76c937a feat: adiciona interface gráfica nativa com sprites

Tamanho do arquivo em cada commit:
76c937a -> perfil.png: 21.018 bytes   (sprite original, pequeno)
3ea2816 -> perfil.png: 773.596 bytes  (sprite "V2", 1254x1254 px)

Para comparação, assets/bomberman.png (sprite do jogador) nunca mudou:
sempre 47.649 bytes, desde o commit que o introduziu. O jogador NUNCA foi
a causa direta — mesmo que o sintoma apareça nos controles do jogador.
```

`perfil.png` (sprite dos inimigos) foi trocado para uma imagem de **1254x1254 pixels**
(773 KB) na atualização "V2". O código (`desenharSpriteNaCelula`) redimensiona essa
imagem *na hora*, via `Gdiplus::Graphics::DrawImage` com interpolação de alta qualidade,
**toda vez que desenha um inimigo — até 6 vezes por frame, a cada frame** (a ~25-40
frames/segundo). Reamostrar uma imagem de 1.57 milhões de pixels de origem 6 vezes por
frame, em GDI+ via software (sem aceleração de GPU), é o que estava consumindo quase um
core inteiro de CPU continuamente.

**Por que o sintoma aparece no protagonista, e não visivelmente nos inimigos:** o jogo
roda em uma única thread de UI (padrão Win32: `GetMessageW` -> `DispatchMessageW`). Se o
processamento de um frame (`WM_PAINT`, que chama `desenhar()`) demora 90-250ms, a thread
fica ocupada esse tempo todo e só volta a checar a fila de mensagens do Windows depois —
inclusive as mensagens `WM_KEYDOWN` do teclado. Ou seja: o custo está no desenho dos
INIMIGOS, mas o efeito perceptível é o PROTAGONISTA "não responder", porque o input dele
fica proporcionalmente sujeito ao mesmo engasgo de frame.

## Correções já aplicadas nesta sessão (no `bomberman.cpp` atual)

1. `compilar.ps1`: adicionado `-O2` (antes compilava sem nenhuma otimização).
2. Backbuffer (`HDC`/`HBITMAP`) reaproveitado entre frames em vez de recriado a cada
   `WM_PAINT`.
3. `Gdiplus::Font` cacheado por tamanho em pixels (`cacheFontes`), em vez de recriado a
   cada chamada de `desenharTexto`.
4. Terreno do tabuleiro (195 células) cacheado em um `Gdiplus::Bitmap`, reconstruído só
   quando `mapa[][]` muda de verdade (parede destruída por explosão, ou `reiniciar()`).
   A explosão (que muda a cada frame) foi separada num overlay leve
   (`desenharExplosaoOverlay`) que só desenha as poucas células realmente afetadas.
5. **A correção que resolveu o problema de fato:** sprites (`bomberman.png`,
   `perfil.png`, `bomba.png`) agora são redimensionados **uma única vez** para o
   tamanho final de tela (`prepararSpriteEscalado`) e cacheados; cada frame só faz um
   `DrawImage` do resultado já pequeno, em vez de reamostrar a imagem original gigante
   repetidamente.

## Resultado medido (antes -> depois), mesma máquina, mesmo teste

Teste: segurar a tecla D (mover) por ~3 segundos durante o jogo, via `PostMessage`
direto pro `HWND` (não depende de foco de janela nem de quem está testando), medindo
`Process.CPU` (segundos de CPU realmente consumidos) sobre o tempo decorrido.

- **Antes:** ~97% de um core sustentado. Tempo de frame medido por instrumentação:
  ~90-250ms por frame (ou seja, ~4-11 fps durante o jogo).
- **Depois:** ~20% de um core. A thread volta a ficar ociosa a maior parte do tempo
  entre frames, o que é exatamente a condição necessária pra `WM_KEYDOWN` ser processado
  sem fila acumulada.
- Verificado visualmente (screenshot) que os sprites continuam com o tamanho/aparência
  corretos depois da mudança (o cache preserva a mesma lógica de escala que já existia).

## O que NÃO foi validado ainda (para quem continuar)

- **Teste manual real do usuário**, jogando de verdade com teclado, ainda não foi feito
  — todas as medições acima foram automatizadas (`PostMessage`/`Get-Process`), porque
  testes anteriores nesta sessão que usaram `SendKeys` e rodaram o jogo repetidamente
  em paralelo ao teste manual do usuário podem ter distorcido medições anteriores (CPU
  disputada entre os processos de teste e o jogo real). Os números acima foram colhidos
  com só o processo do jogo rodando, sem interferência.
- Se o usuário ainda sentir lentidão depois de recompilar com essas mudanças, os
  suspeitos seguintes, em ordem de probabilidade, seriam:
  1. Contenção real de CPU no PC do usuário por outros processos pesados (CLion, Rider
     etc. apareceram consumindo bastante CPU nos testes desta sessão) — nesse caso o
     jogo ficando mais leve ajuda, mas não elimina o efeito de outros programas.
  2. Algum outro asset grande não identificado (só `perfil.png` foi auditado a fundo;
     vale rodar `identify` ou equivalente nos demais PNGs de `assets/` se o problema
     voltar).
  3. Delay especificamente do **Windows** (taxa de repetição de tecla do SO) — o jogo já
     tem `processarMovimentoContinuo()` pra evitar depender disso, mas vale confirmar
     que não regrediu.

## Arquivos alterados

- `bomberman.cpp` (todas as correções de performance acima)
- `compilar.ps1` (`-O2`)
