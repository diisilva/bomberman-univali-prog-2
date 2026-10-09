# Bomberman — Algoritmos e Programação II

Projeto em C++17 para Windows, com Win32, GDI e GDI+. Desenvolvedores: Diego Silva e Gabriel Bianchessi. Sem engine.

O menu reúne a campanha acadêmica e o **Modo Insano**, que executa o jogo completo de batalha de sprites.

## Compilar e executar

Os executaveis `bomberman.exe` e `modo_insano.exe` estao incluidos no repositorio.
Para jogar no Windows 10/11, baixe a pasta completa e abra `bomberman.exe`;
mantenha `assets/` e `assets_otimizados/` junto dos executaveis. As bibliotecas
do GCC sao incorporadas ao build, sem exigir MinGW instalado para jogar.

Requisitos: Windows 10/11, PowerShell e GCC/MinGW C++17. O script procura `g++` no PATH e em `C:\msys64\ucrt64\bin\g++.exe`.

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
.\bomberman.exe
```

O build gera `bomberman.exe` e `modo_insano.exe`. Feche instâncias anteriores antes de recompilar. Python e Pillow são usados somente pelos scripts opcionais que regeneram os sprites; não são necessários para jogar.

Assets são carregados uma vez, a partir da pasta do executável. Mantenha `assets/`, `assets_otimizados/` e os dois executáveis juntos. A janela permite redimensionar e maximizar, mantendo a proporção e centralizando o desenho.

## Menu

| Opção | Ação |
| --- | --- |
| 1 | Campanha manual |
| 2 | Campanha automática |
| 3–6 | Como jogar, dificuldades, pontuação e ranking |
| 7 | Escolher personagens |
| 8 | Seletor de fases |
| 9 | Modo Insano |
| P | Ativar/desativar P2 cooperativo na campanha |
| Q / Esc | Sair |

Escolha dificuldade, personagem e fase inicial. Nas skins: 1–4 selecionam, Tab alterna P1/P2 e Enter abre as fases. Na galeria: 1–8 ou setas selecionam e Enter inicia. Cartões também aceitam mouse.

Branco, preto, azul e vermelho têm as mesmas habilidades. A escolha muda aparência, caminhada e poses finais.

## Campanha

As três arenas anteriores foram preservadas; as demais vêm do estudo. A campanha segue da fase selecionada até a oitava.

| Fase | Arena |
| --- | --- |
| 1 | Casino Azul |
| 2 | Masmorra |
| 3 | Fábrica Neon |
| 4 | Ruínas de Pedra |
| 5 | Cânion Ceedrun |
| 6 | Selva Viva |
| 7 | Laboratório |
| 8 | Templo Arcano |

Limpe os inimigos e entre no portal. Nas fases 2, 4, 6 e 8, derrote tambem o chefe para abrir o portal. Facil tem tres inimigos normais, perseguicao limitada e ritmo mais tranquilo; Medio e Dificil tem cinco e sete.

| Tecla | Ação na campanha |
| --- | --- |
| WASD ou setas | Mover P1; com cooperativo, setas ficam com P2 |
| Espaço / Enter | Bomba de P1 / P2 |
| E | Chutar bomba, após obter item |
| T | Teletransportar, após obter item |
| F | Detonar bombas-relógio |
| R | Reiniciar; após morte, aguarda a animação terminar |
| M | Menu |
| Q / Esc | Sair |

Vidas, pontuação e itens são compartilhados no cooperativo. Bônus de fase são reiniciados ao trocar/repetir fase. Recompensas usam ícones distintos para fogo, bomba extra, vida, relógio, proteção, atravessar blocos, chute e teleporte. Blocos especiais não mostram pistas do conteúdo.

A morte toca nove poses no local do personagem atingido antes de exibir continuar/game over. O movimento termina cada passo antes da curva; a última direção pressionada tem prioridade. Contato e dano seguem a posição atual dos corpos, sem atingir antecipadamente por uma casa de destino reservada.

O ranking é salvo em `ranking.csv`, ao terminar uma partida manual. A fórmula aparece no menu e usa inimigos, caixas, itens, fases, movimentos e bombas.

Chefes da campanha:

| Fase | Chefe | Poder | HP |
|---|---|---|---|
| 2 | Golem da Masmorra | Machados e facoes giratorios (Metal Slug) direcionados | 48 |
| 4 | Egg-HVC / Demolidor | Disparos de energia e investida ortogonal com preparacao | 64 |
| 6 | Hidra da Selva | Rajadas de veneno animadas que deixam pocas persistentes | 80 |
| 8 | Guardiao Arcano | Avancos e bolas de fogo do Modo Insano | 100 |

Em cada fase de chefe, Mario esta escondido em um bloco quebravel comum, sem
marca. Exploda o bloco para liberta-lo. Ele ajuda automaticamente: movimento
ortogonal a 4,2 casas/s, tres vidas, dois segundos de protecao inicial e 1,6 s
apos sofrer dano. Lanca macas em arco a cada 1,05 s, elimina inimigos, detona
bombas, quebra caixas e causa 3 de dano ao chefe. As macas seguem os tempos,
trajetoria, sprites e regras de impacto do estudo; os alvos sao os inimigos da
campanha. Mario coleta macas extras para recarregar. Proteja-o: perder suas tres
vidas causa derrota, como no Insano. Fases impares nao possuem Mario escondido.

## Modo Insano

Executa uma cópia integral do jogo de `estudos_sprite/batalha.cpp`: quatro Bombermans, Mario, Ceedrun, sentinelas, maçãs, projéteis, oito arenas e boss original de 100 HP. Mecânicas e controles permanecem iguais ao estudo; somente o caminho dos assets foi tornado relativo.

Enter alterna White/automático; M alterna Mario/automático; WASD move; Espaço usa bomba/maçã; T alterna arenas; P pausa; R reinicia; L ativa câmera lenta; Esc fecha. Ao fechar o Insano, o menu principal reaparece. Ele não escreve no ranking CSV da campanha.

Origem, hash do fonte e validação: [modo_insano/README.md](modo_insano/README.md).

## Organização e validação

- `bomberman.cpp`: fonte oficial da campanha; matriz 13×15, structs, vetores e sub-rotinas.
- `modo_insano/batalha_referencia.cpp`: cópia preservada do estudo.
- `compilar.ps1`: gera os dois executáveis; fonte portátil do Insano é gerado no build.
- `testes/`: regressões de colisão, fluidez, seleção, assets e combate original do Insano.
- `PROGRESSO_M2.md` e `HANDOFF_CODEX.md`: estado e decisões recentes.
- `DEFESA.md` e `ROTEIRO_DEFESA.md`: materiais de defesa que ainda precisam da revisão completa para M2.

Win32 cuida de janela, teclado, tempo e DPI. GDI apresenta o buffer duplo; GDI+ desenha sprites com alpha e pixel art. Terreno e fontes usam cache. BFS template, explosão recursiva, sobrecargas, parâmetro default, structs e arquivo permanecem na campanha.

Instruções dos testes e geração de previews: [testes/README.md](testes/README.md). Testes isolados não abrem janela, não enviam teclas e não alteram o ranking. O controle real, maximizar e a transição entre janelas devem ser avaliados jogando.

## Créditos de assets

Assets fornecidos para estudo acadêmico de Bomberman/Hudson/Konami. Folhas trazem créditos de Cyber Wolf, Roviozza, raccoon sam (2012), JCGBR e Boo; Mario por ItsMeRen. Originais foram preservados. Ícones e recortes derivados mantêm esses créditos.
