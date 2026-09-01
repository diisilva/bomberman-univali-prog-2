# Bomberman para Algoritmos e Programação II

Trabalho M1 da disciplina de Algoritmos e Programação II da UNIVALI.

O jogo roda diretamente no terminal do Windows e **não utiliza GLUT, OpenGL ou outra biblioteca gráfica externa**. A interface colorida é desenhada com recursos do próprio console do Windows.

## Antes da entrega

Edite o início de `bomberman.cpp` e substitua:

```cpp
Desenvolvedores: PREENCHER COM OS NOMES DA EQUIPE
```

pelos nomes de todos os integrantes. O enunciado exige a identificação dos desenvolvedores e a defesa deve ser feita em dupla ou trio. Trabalhos individuais recebem a penalidade descrita pelo professor.

## Pré-requisitos

- Windows 10 ou Windows 11;
- compilador GCC/G++ com suporte ao C++17;
- PowerShell para usar o script de compilação;
- nenhuma biblioteca gráfica é necessária.

O código utiliza `windows.h` e `conio.h` para controlar cores, tamanho da janela e leitura das teclas sem precisar pressionar Enter. Por isso esta versão foi preparada especificamente para Windows.

## Como verificar o compilador

Abra o PowerShell nesta pasta e execute:

```powershell
g++ --version
```

Se aparecer a versão do GCC, o compilador está pronto. Se o comando não existir, instale o MSYS2 e o compilador UCRT64:

1. Baixe e instale o MSYS2 em <https://www.msys2.org/>.
2. Abra o terminal **MSYS2 UCRT64**.
3. Execute:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

4. Adicione `C:\msys64\ucrt64\bin` à variável de ambiente `Path` do Windows.
5. Feche e abra novamente o PowerShell.

## Como compilar

No PowerShell, entre na pasta do projeto:

```powershell
cd D:\algoritmos_prog_2\bomberman-univali-prog-2
```

Use o script incluído:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
```

Ou compile manualmente:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic bomberman.cpp -o bomberman.exe
```

Se `g++` não estiver no `Path`, use o caminho completo do MSYS2:

```powershell
& "C:\msys64\ucrt64\bin\g++.exe" -std=c++17 -Wall -Wextra -pedantic bomberman.cpp -o bomberman.exe
```

Uma compilação correta não deve apresentar erros nem avisos.

## Como executar

Depois de compilar:

```powershell
.\bomberman.exe
```

O jogo abre na própria janela do terminal.

## Menu e modos de jogo

Ao abrir o programa, o menu oferece duas opções:

1. `Jogar manualmente`, para controlar o personagem normalmente.
2. `Jogo automático 2x`, para observar o programa jogar sozinho até vencer.

No modo automático, o jogador procura inimigos e paredes frágeis por meio de uma busca em largura. Ele coloca bombas quando encontra um alvo, tenta sair da área da explosão e continua jogando em velocidade 2x. Se for derrotado, uma nova tentativa começa automaticamente e permanece ativa até concluir a partida.

## Controles

| Tecla | Ação |
| --- | --- |
| `W`, `A`, `S`, `D` | Movimentar o jogador |
| Setas | Movimentar o jogador |
| `Espaço` | Colocar uma bomba |
| `R` | Reiniciar a partida |
| `Q` ou `Esc` | Encerrar o jogo |
| `M` | Voltar ao menu |
| `1` no menu | Iniciar o modo manual |
| `2` no menu | Iniciar o modo automático 2x |

Não é necessário pressionar Enter durante a partida.

## Elementos do mapa

| Elemento | Aparência | Comportamento |
| --- | --- | --- |
| Jogador | Bloco azul `PJ` | Personagem controlado pelo usuário |
| Inimigo | Bloco vermelho `IN` | Move-se aleatoriamente |
| Bomba | Bloco preto `BO` | Explode depois de aproximadamente 2,2 segundos |
| Explosão | Bloco laranja `**` | Mata personagens e destrói paredes frágeis |
| Parede sólida | Bloco cinza `##` | Bloqueia e não pode ser destruída |
| Parede frágil | Bloco azul `+=` | Bloqueia e pode ser destruída |

## Verificação dos requisitos do enunciado

### Funcionalidades

- [x] O jogador se move corretamente nas quatro direções.
- [x] O jogador pode entrar e sair de uma posição onde existe bomba.
- [x] Paredes sólidas e frágeis bloqueiam o jogador.
- [x] Há intervalo de movimento para evitar repetição descontrolada de tecla.
- [x] A bomba é colocada na posição atual do jogador.
- [x] Apenas uma bomba pode existir por vez.
- [x] Uma nova bomba pode ser colocada depois que a explosão termina.
- [x] O jogador morre ao colidir com um inimigo.
- [x] O jogador morre ao entrar ou permanecer na explosão.
- [x] Cada inimigo tenta mover de 1 a 3 quadrados em uma direção aleatória, parando diante de obstáculos.
- [x] Paredes sólidas bloqueiam a explosão e não são destruídas.
- [x] A primeira parede frágil atingida em cada direção é destruída e bloqueia a continuação daquele raio.
- [x] A explosão fica visível durante aproximadamente 650 milissegundos.
- [x] O jogador vence quando todos os inimigos morrem e ele permanece vivo.
- [x] Há derrota por colisão com inimigo ou por explosão.
- [x] O menu permite escolher entre jogo manual e demonstração automática em velocidade 2x.

### Técnicas

- [ ] Identificação dos desenvolvedores, portanto é necessário **preencher os nomes antes da entrega**.
- [x] O programa é dividido em sub-rotinas pequenas e específicas.
- [x] As sub-rotinas recebem parâmetros e usam referências quando precisam alterar os dados originais, como `atingirPersonagens(vector<Inimigo>&, EstadoJogo&, int&)`.
- [x] Mapa, jogador, inimigos, bomba, atualização, entrada e desenho estão segmentados.

## Organização do código

- `TipoCelula` representa os espaços vazios e os dois tipos de parede.
- `EstadoJogo` representa partida em andamento, vitória e derrota.
- `Posicao`, `Inimigo` e `Bomba` agrupam os dados das entidades.
- `criarMapa()` monta o cenário.
- `moverJogador()` e `moverInimigos()` cuidam dos movimentos.
- `colocarBomba()`, `iniciarExplosao()` e `atualizarBomba()` controlam a bomba.
- `atingirPersonagens()` trata mortes causadas pela explosão.
- `verificarVitoria()` controla o fim da partida.
- `desenhar()` atualiza a interface colorida do console.
- `lerTeclado()` recebe os comandos sem exigir Enter.
- `proximoPassoDoBot()` usa busca em largura para escolher o caminho automático.
- `atualizarBot()` decide quando andar, fugir ou colocar uma bomba.

## Arquivos do projeto

- `bomberman.cpp`: código-fonte do jogo;
- `compilar.ps1`: script de compilação para PowerShell;
- `README.md`: documentação, requisitos e instruções.

O arquivo `bomberman.exe` é gerado localmente pela compilação.
