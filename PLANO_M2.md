# Plano de Implementação — M2 Bomberman

> Baseado em `instruction-bomberman/T1 - Bomberman - 2025-part2.pdf`. Este documento é para
> aprovação antes de qualquer código ser alterado. Todas as decisões de design já foram
> confirmadas com você (seção 5); não há mais pontos em aberto bloqueando o início da Fase 1.

## 1. Como isso está organizado

O jogo inteiro mora em `bomberman.cpp`, único arquivo-fonte real.

> Atualização (2026-10-08): a pasta `legado/` e toda a distinção `VERSAO_LEGADO` citadas no resto
> deste documento foram removidas a pedido do usuário — existe uma única versão do jogo agora.
> Ver `PROGRESSO_M2.md`, seção "Limpeza: remoção da versão legado".

## 2. Itens do M1 que já valem para o M2 (reaproveitados)

| Critério do PDF | Situação | Onde |
|---|---|---|
| Jogo otimizado | Buffer de frame, cache de fontes, cache de mapa e sprites pré-escalados já existem (ver `INVESTIGACAO_PERFORMANCE.md`) | topo do arquivo |
| Não deve piscar/deslocar blocos fora da dinâmica | Double buffer com `BitBlt`, cache de mapa só invalida quando o mapa muda de verdade | `desenhar()`, `cacheMapaSujo` |
| Design caprichado | Sprites PNG com alpha, HUD, menu | `desenharHUD`, `desenharMenu` |
| Blocos quebráveis aleatórios | `criarMapa()` já sorteia parede frágil com 36% de chance | linha 150 |
| Espaço garantido para estourar bomba | `sortearPosicoesIniciais` + `limparArea` já abrem uma cruz ao redor de cada spawn | linha 182 |
| Opção do computador jogar sozinho | Modo `AUTOMATICO` já existe (bot via BFS) | `proximoPassoDoBot()` |
| Perseguição dos inimigos por caminho mais curto | `proximoPassoParaAlvo()` já faz BFS e já é usado probabilisticamente (`CHANCE_PERSEGUICAO`) | linha 352 |
| Parâmetro default | `temInimigo(Posicao p, int ignorar = -1)` já existe | linha 130 |
| Pontuação por inimigo morto | `matarInimigo()` soma 100 pontos | linha 310 |

Boa notícia: a base de BFS, structs e vetores já é sólida. O trabalho do M2 é principalmente
**estender** essa base (fases, dificuldades, itens, boss) e **fechar os critérios técnicos e de
persistência** que o M1 não exigia.

## 3. Critérios parcialmente atendidos (precisam de ajuste, não de reescrita)

- **Menu** — existe, mas só tem "jogar manual" / "jogar automático". Faltam: explicação de como
  jogar, explicação de dificuldades, explicação de pontuação, ranking ordenado.
- **Continuar/reiniciar ao morrer** — hoje `R` reinicia a partida inteira do zero, sem vidas.
  Precisa virar um sistema de vidas com reinício da fase atual (decisão tomada, ver seção 5).
- **Informações em tela** — já mostra pontos, inimigos vivos e raio da bomba. Faltam: tempo
  decorrido, contagem de movimentos, contagem de bombas usadas, itens coletados.
- **Itens especiais** — existem `CHUTE` e `TELEPORTE`, mas não são o conjunto mínimo pedido pelo
  enunciado (raio+1, bomba+1, vida extra, bomba-relógio, sobreviver à bomba, atravessar bloco
  quebrável). Kick/teleporte podem continuar existindo como itens extras do jogo, mas os itens
  obrigatórios precisam ser adicionados.
- **Explosão e bloco frágil** — hoje a explosão para no primeiro bloco frágil que encontra e o
  destrói (sempre 1 bloco). O enunciado pede que a explosão atravesse **tantos blocos quebráveis
  quanto o nível do fogo permitir** — isso muda a regra (ver seção 6, vira a função recursiva).

## 4. Critérios totalmente ausentes

**Jogabilidade / pontuação (0.4 cada, salvo indicação):**
- Fases (mínimo 3) com portal para a próxima.
- Inimigo especial com movimento mais inteligente na última fase.
- 3 dificuldades com contagem/comportamento de inimigo específico (0.5).
- Boss (parte da nota de dificuldades, 0.5).
- Explicação de pontuação com cálculo real (movimento, bombas usadas, inimigos abatidos, caixas
  destruídas) e tela de Rank ordenado.
- Salvar partida em arquivo (data, jogador, pontuação, bombas, movimentos) (0.4).

**Itens obrigatórios mínimos (0.5):** raio+1 cumulativo, bomba+1 cumulativa, vida extra
cumulativa, bomba-relógio, sobreviver à bomba, atravessar bloco quebrável — todos resetam ao
trocar de fase.

**Itens extra (2.0 — pontuação alta, vale priorizar):**
- Segundo jogador (cooperativo local).
- Música no jogo.
- *(Computador jogar sozinho já está pronto — ver seção 2.)*

**Critérios técnicos (3.0):**
- Arquivo: criar, ler e inserir dados (hoje não existe nenhuma leitura/escrita em disco).
- Sobrecarga de função (hoje só existe parâmetro default, não há duas funções com o mesmo nome e
  assinaturas diferentes).
- Template (hoje não existe nenhum `template<...>` no código).
- Recursividade (hoje toda a busca usa `queue`/laços; não há função recursiva).

## 5. Decisões já tomadas com você

1. **Boss:** aparece só na última fase, com um padrão de ataque (vida própria, persegue o
   jogador, solta bombas/obstáculos simples). Vale para qualquer dificuldade.
2. **Vidas / continuar:** o jogador começa com vidas (proposta: 3). Ao morrer com vidas restantes,
   a fase atual reinicia (bônus da fase são perdidos, como já manda o enunciado). Ao zerar as
   vidas, aparece uma tela de Game Over com opção de reiniciar do zero (fase 1, vidas cheias) ou
   voltar ao menu. Pontuação acumulada até ali é salva no arquivo de ranking antes de resetar.
3. **Música:** gerada por código (sem depender de arquivos externos nem downloads), usando
   `winmm`/`PlaySound` com um `.wav` sintetizado em memória (ex.: loop simples tipo chiptune para
   o fundo + um "blip" curto para bomba/explosão/coleta de item). Sem dependências externas, sem
   questão de licença.
4. **Vidas iniciais:** 3.
5. **Vida do boss:** 5 acertos de explosão, usando o mesmo mecanismo que já acerta inimigos
   comuns (`atingirPersonagens`), só que com um contador de vida em vez de morte no primeiro
   acerto.
6. **Segundo jogador:** pontuação e vidas **individuais** por jogador, lado a lado no HUD; a
   partida só termina quando os dois perderem todas as vidas (ou os dois chegarem ao fim do
   jogo). Jogador 2 usa setas + `Enter` para bomba; jogador 1 mantém WASD + espaço.
7. **Mapa entre fases:** tamanho fixo 13×15 em todas as fases (sem recalcular janela/HUD).
   O que muda visualmente entre as fases são as cores/skins dos blocos e, se der tempo, um sprite
   de bloco de madeira pegando fogo (animação curta no momento em que o bloco frágil é destruído
   pela explosão, antes de virar `VAZIO`) — ver seção 11.

## 6. Como cada critério técnico será satisfeito (e por quê faz sentido no jogo, não só "pra cumprir tabela")

- **Recursividade →** a regra "a explosão atravessa tantos blocos quebráveis quanto o nível do
  fogo permitir" vira uma função recursiva `propagarExplosao(pos, direcao, alcanceRestante,
  blocosQuebraveisRestantes)`: a cada chamada avança uma célula, decide se para (parede sólida),
  destrói bloco frágil e recursa com um quebrável a menos, ou continua livre e recursa com o
  mesmo contador. É recursão de verdade, não um `for` disfarçado, e resolve uma regra que o
  enunciado pede.
- **Template →** as duas buscas (perseguição de inimigo comum e busca do bot automático) hoje
  duplicam quase o mesmo BFS (`proximoPassoParaAlvo` e `proximoPassoDoBot`). Viram uma única
  função template `template<typename Predicado> Posicao bfsProximoPasso(Posicao origem, Predicado
  ehAlvo)`, parametrizada pelo critério de parada (alvo = jogador, ou alvo = "é parede frágil ou
  inimigo"). Um template que remove duplicação de verdade, não um genérico forçado.
- **Sobrecarga →** `salvarRegistroPartida(RegistroPontuacao)` e uma segunda versão
  `salvarRegistroPartida(string caminho, RegistroPontuacao)` (grava em arquivo alternativo, usada
  por exemplo para testes/depuração); e `calcularPontuacao(const EstatisticasPartida&)` com uma
  sobrecarga `calcularPontuacao(int movimentos, int bombas, int inimigos, int caixas)` para quem
  já tem os valores soltos. Duas assinaturas reais, não só um parâmetro default a mais.
- **Struct →** `RegistroPontuacao` (data, jogador, pontos, bombas, movimentos) e
  `EstatisticasPartida` (contadores da partida atual) são novas structs; `Posicao`, `Inimigo`,
  `Bomba` continuam.
- **Arquivo →** `ranking.csv` ao lado do executável. Escreve uma linha por partida finalizada
  (vitória final ou fim de vidas) e lê todas as linhas para montar a tela de Rank (ordenada por
  pontuação, maior primeiro). Formato simples de CSV com `;` como separador (evita conflito com
  datas `dd/mm/aaaa`).

## 7. Fases, dificuldades e boss — como ficam as regras

**Dificuldades** (escolhidas no menu antes de começar a jogar):

| Dificuldade | Inimigos por fase | Chance de perseguir o jogador |
|---|---|---|
| Fácil | 3 | comportamento atual do T1 (BFS só dentro de 6 células, 65% de chance) |
| Médio | mínimo 5 | 50% de chance a cada movimento, sem limite de distância |
| Difícil | mínimo 7 | 75% de chance a cada movimento, sem limite de distância |

**Fases:** 3 fases, mapa regerado a cada uma (blocos frágeis sorteados de novo). Dificuldade
escolhida define quantos inimigos cada fase spawna. Ao morrer o último inimigo da fase, um
**portal** aparece em uma célula livre sorteada; pisar nele avança de fase (bônus da fase
resetam, vidas voltam ao valor base, pontuação e estatísticas acumuladas continuam).

**Fase 3 (última):** além dos inimigos normais da dificuldade, spawna 1 **inimigo especial**
(BFS sempre ativo, sem chance/distância — persegue a cada movimento) e, ao fim dessa leva de
inimigos, o **boss** aparece no lugar do portal — só depois de vencer o boss é que o portal de
"fim de jogo" aparece.

## 8. Itens — comportamento proposto

Todos resetam ao trocar de fase (conforme o enunciado), exceto vidas (também resetam ao valor
base, conforme decisão da seção 5).

| Item | Efeito |
|---|---|
| Raio de fogo +1 | cumulativo, aumenta `alcanceExplosao` (já existe o conceito, só falta virar item avulso em vez de ganho automático por kill) |
| Bomba +1 | cumulativo, aumenta quantas bombas o jogador pode ter ativas ao mesmo tempo (**mudança estrutural:** `Bomba bomba` vira `vector<Bomba> bombas`, com limite = 1 + itens coletados) |
| Vida extra | cumulativa, +1 vida (reseta ao trocar de fase) |
| Bomba-relógio | a próxima bomba plantada não explode sozinha: o jogador detona com uma tecla (ex.: `F`) |
| Sobreviver à bomba | enquanto ativo, o jogador não morre na própria explosão |
| Atravessar bloco quebrável | enquanto ativo, o jogador anda por cima de parede frágil sem destruí-la |

> Observação sobre o enunciado: a segunda lista de "itens especiais (2.0)" do PDF lista segundo
> jogador / música / bot sozinho — isso claramente não são itens de caixa, é um reaproveitamento
> do mesmo título de bullet no documento original. Vou tratar essas três coisas como
> funcionalidades de jogo (seção 9), não como itens que caem de caixa.

## 9. Pontuação — fórmula proposta (ajustável)

```
pontos = 100 × inimigos_abatidos
       + 20  × caixas_destruidas
       + 30  × itens_coletados
       + 500 × fases_concluidas
       - 1   × movimentos
       - 5   × bombas_usadas
```

Clampada em 0 (nunca negativa). Essa fórmula e os pesos aparecem na tela "Como funciona a
pontuação" do menu, e viram realmente o valor salvo no `ranking.csv`. Se quiser outros pesos, me
diga os números e eu ajusto — isso é só proposta inicial.

## 10. Segundo jogador

Cooperativo local, mesma tela e teclado: jogador 2 usa as setas + `Enter` para bomba (jogador 1
fica com WASD + espaço, como já é hoje). Pontuação e vidas são **individuais**: cada jogador tem
seu próprio contador de vidas e seus próprios pontos, exibidos lado a lado no HUD. Um jogador que
perde todas as vidas fica "fora" (sem spawnar de novo naquela fase) enquanto o outro continua; a
partida termina quando os dois perdem todas as vidas. O ranking salvo em arquivo registra os dois
jogadores (duas linhas, uma por jogador) ao final da partida.

## 11. Sprite do boss (`assets/boss.png`)

Já conferi o arquivo: é 240×320, **totalmente opaco** (céu e chão atrás do robô, sem
transparência real apesar de já estar em RGBA). Para usar como sprite do jogo preciso:

1. Remover o fundo (céu/chão) e deixar só a figura do robô com alpha transparente, do mesmo jeito
   que `bomberman.png`/`perfil.png` já são usados.
2. Recortar a margem e redimensionar para a célula do jogo, do mesmo jeito que
   `desenharSpriteNaCelula()` já faz para os outros sprites (escala pelo menor fator, mantém
   proporção, centraliza).
3. Salvar como `assets/boss.png` (ou `assets/boss_sprite.png`, mantendo o arquivo original como
   referência) pronto para `carregarAssets()`.

Isso entra como tarefa técnica da Fase 1 da implementação (seção 12), antes de o boss aparecer no
jogo.

**Assets de referência recebidos (2026-10-08):** `assets/NES - Bomberman - Backgrounds -
Playfield.png` (tileset limpo, em grade — grama + pilar de pedra — dá pra fatiar e usar direto
como skin da Fase 1) e duas capturas de tela inteiras do Super Bomberman (SNES),
`Stage 2-5.png` (piso losangulado azul, blocos dourados, moldura marrom) e `Stage 6-3.png` (piso
escuro, blocos magenta, moldura metálica). As duas do SNES são telas compostas (com texto de UI,
gradiente de fundo), não tilesets limpos — fatiar blocos exatos delas é frágil pelo ganho que dá.
Vão servir como **referência de paleta/estilo** (cores de piso/bloco por fase), não como sprites
fatiados; só o NES Playfield entra como tileset real.

**Assets de personagem recebidos (2026-10-08):** `assets/novos_perssonagens.png` (folha do Super
Bomberman 1 com 4 variantes de cor completas — verde/ciano/azul/vermelho — cada uma com
walk/punch/stun/knockout/teleporte; boa base pronta para uma tela de seleção de personagem),
`assets/Black Bomberman.png` (sprite sheet completo em preto, dá para usar como skin alternativa
ou para diferenciar visualmente o inimigo especial da fase final) e
`assets/personagem_principal_sprite.png` (folha bem mais completa do Bomberman clássico: idle,
andar, carregar item, chute, dano, teleporte, bomba e explosão pequena — upgrade em potencial para
o sprite atual do jogador). Todas têm aviso de direitos da Hudson/Konami e pedido de crédito ("By
Cyber Wolf, Original of Hudson") — dar os créditos no README ao usá-las. Isso é cosmético puro
("design caprichado" + diferenciação visual de P1/P2/inimigo especial), então entra na etapa de
polimento (seção 12, passo 8), não bloqueia a Fase 1 em andamento.

**Asset adicional (2026-10-08): `assets/personagem_principal_sprite_v2.png`** — folha ainda mais
organizada (frames de 16×32, grade limpa, duas paletas "Bomberman" e "recolorable") com poses de
vitória marcadas como `loop` e uma sequência de derrota/desmaio clara. Vai virar duas novas
animações no fluxo de fim de partida: **tela de vitória** (sprite em loop de comemoração em vez
do texto estático atual) e **tela de game over** (sprite tocando a sequência de desmaio uma vez,
parado no último frame). Entra junto com a tela de Game Over da seção 5/item 2 (Fase 4 do
cronograma, seção 12) — a animação usa esse asset, a lógica de vidas e o gatilho são os mesmos já
planejados, só troca texto estático por sprite animado.

**Variação visual por fase e bloco de madeira pegando fogo:** cada fase recebe uma paleta de cor
diferente para parede sólida/frágil (ex.: fase 1 cinza/azul atual, fase 2 tons mais escuros, fase
3 tons de "área do boss"), reaproveitando `desenharCelula()` com a cor escolhida pela fase atual
em vez de uma constante fixa. Se der tempo dentro do cronograma, acrescento um sprite curto de
"madeira pegando fogo" (2–3 quadros, ou um único PNG com chama sobreposta) desenhado no instante
em que um bloco frágil é destruído pela explosão, antes da célula virar `VAZIO` — puramente
cosmético, não deve atrasar as fases com pontuação maior do plano.

## 12. Ordem de implementação sugerida

1. **Base de dados/infra:** `RegistroPontuacao`, `EstatisticasPartida`, leitura/escrita de
   `ranking.csv`, contagem de movimentos/bombas/itens em tempo real. Preparar sprite do boss.
2. **Fases e dificuldades:** menu de seleção de dificuldade, estrutura de fase atual, regeneração
   de mapa por fase, portal, inimigo especial e boss na fase 3.
3. **Itens obrigatórios:** os 6 itens mínimos, bombas múltiplas (`vector<Bomba>`), explosão
   recursiva atravessando blocos conforme nível de fogo.
4. **Vidas e game flow:** sistema de vidas, reinício de fase, tela de Game Over, gravação no
   ranking ao final.
5. **Telas de menu:** como jogar, dificuldades, pontuação (com a fórmula visível), rank ordenado
   (lido do CSV).
6. **Critérios técnicos restantes:** template do BFS, sobrecargas, limpeza final de
   default-params.
7. **Itens de 2.0:** segundo jogador, música procedural.
8. **Polimento:** HUD com tempo/movimentos/bombas/itens em tela, ajuste fino de otimização,
   atualização de `README.md`/`DEFESA.md`/`ROTEIRO_DEFESA.md` com as novas mecânicas.

**Asset adicional (2026-10-08): `assets/BombsExplosions.png` e `assets/BombsExplosionsv2.png`.**
O primeiro é uma folha pequena e limpa com variantes de cor de bomba + uma sequência de explosão
radial. O segundo é uma folha enorme com peças de explosão direcionais no estilo clássico
(centro/braço/ponta, como o Bomberman original monta a cruz da explosão) em ~10 paletas de cor
diferentes, mais bombas correspondentes. Dá pra fazer a explosão/bomba atuais (hoje um retângulo
laranja simples) ficarem com a cara clássica, e as paletas de cor reforçam a variação visual por
fase (seção 11). Entra na mesma etapa de polimento visual das outras folhas de sprite.

**Asset adicional (2026-10-08): `assets/icons_blocos.png`.** Ícones pequenos de bomba (várias
cores) e 4 ícones de item (teleporte, pedra, caveira, mão) que encaixam bem como ícones dos itens
obrigatórios da seção 8 no HUD/caixas, mais um fantasma Boo (Mario) utilizável como skin extra de
inimigo. Aviso de crédito ("Ripped By Boo, Give Credit If Used") — idem seção 11, creditar no
README. Entra na etapa de polimento junto com os demais.

**Diretriz do usuário para a etapa de polimento (2026-10-08):** os assets de personagem recebidos
têm ciclos de animação direcionais completos (frente/trás/esquerda/direita, não só um frame
parado) e várias variantes de explosão/cenário — explorar isso de verdade na etapa de polimento
(trocar de frame conforme a direção/estado do personagem, variar a explosão), não só colar um
sprite estático no lugar do retângulo atual.

## 12.1. Extras opcionais (fora da lista de nota, só se sobrar tempo)

- **Mario ajudante:** `assets/mario.png` (sprite chibi estilo Bomberman, 8 frames, by ItsMeRen).
  Ideia do usuário: na fase final, um bloco especial (sempre presente, posição aleatória) libera um
  Mario ao ser destruído; ele vira um ajudante que segue o jogador e joga maçãs nos inimigos.
  Isso é uma mecânica nova de verdade (NPC aliado com IA própria + sistema de projétil, que hoje
  não existe no jogo) — **não está em nenhum critério do PDF**, então só entra depois que todo o
  resto da lista de nota (seções 3, 4, 7, 8) estiver pronto e testado. Registrado aqui pra não
  esquecer, não para priorizar.

## 13. Status das decisões

Todas as decisões de design necessárias para começar foram tomadas (seção 5, itens 1–7, e seção
10). Não há bloqueio para iniciar a Fase 1. Só pequenos detalhes de polimento (ex.: exatamente
quantos quadros a animação de madeira pegando fogo vai ter) ficam a meu critério durante a
implementação, e qualquer ajuste é trivial de revisar depois.

Pode me dar sinal verde para começar pela Fase 1 (base de dados/infra + preparo do sprite do
boss), ou avisar se quiser revisar algo antes.
