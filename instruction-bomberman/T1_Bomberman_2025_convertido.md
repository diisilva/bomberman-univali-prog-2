# Definição do trabalho da M1 - BOMBERMAN

**Universidade do Vale do Itajaí - UNIVALI**  
**Escola Politécnica**  
**Curso:** Ciência da Computação  
**Disciplina:** Algoritmos e Programação II (22817)

## Informações gerais

- **Data da defesa:** a partir de **15/04/2026**, a partir das **19:00**.
- **Modalidade:** em **duplas ou trios**.
- **Observação:** trabalhos individuais perderão **20% da nota**.

## Visão geral

Bomberman é uma série de jogos de estratégia, inicialmente desenvolvida pela fabricante Hudson Soft. O jogo original foi lançado em 1983 e novos jogos da série ainda continuam a ser lançados. Hoje, Bomberman aparece em mais de 60 jogos diferentes. *(Wikipedia, 2023).*

### Descrição da imagem de referência do jogo

A página apresenta uma imagem de uma versão clássica de Bomberman vista de cima, em um cenário construído como uma grade. O mapa contém paredes sólidas, paredes destrutíveis e corredores livres. O personagem controlado pelo jogador aparece na parte inferior do mapa. Há vários inimigos espalhados pelo cenário e uma bomba posicionada próxima ao centro. A imagem serve como referência visual para a organização geral do tabuleiro e para a interação entre jogador, inimigos, paredes e bombas.

---

# Composição do jogo

## Jogador

- O jogador deverá se mover por todas as posições livres e por posições onde exista bomba.
- O jogador poderá colocar bombas na posição em que está.
- O jogador morrerá se estiver na área de explosão de uma bomba ou se colidir com algum inimigo.

## Inimigo

- De tempos em tempos, o inimigo deverá se mover **1, 2 ou 3 quadrados** em uma direção aleatória que esteja livre, ou até encontrar um obstáculo.
- O inimigo morre se estiver na área de uma bomba quando ela explodir.

## Bomba

- A bomba deverá ser colocada no cenário pelo jogador.
- Depois de determinado tempo, a bomba deverá explodir.
- Apenas **uma bomba** poderá estar presente no cenário por vez.
- O jogador não poderá colocar uma nova bomba enquanto a anterior ainda estiver no mapa.
- Depois que a bomba anterior detonar, o jogador poderá colocar uma nova bomba.
- A explosão deverá ser **visível** no jogo.
- Quando a bomba explodir, deverá destruir todos os jogadores e inimigos que estiverem em sua proximidade.
- A explosão também deverá destruir paredes frágeis.
- A explosão **não poderá destruir paredes sólidas**.

### Convenção visual usada no enunciado

- **Quadrado cinza:** parede sólida.
- **Quadrado azul:** parede frágil.
- **Quadrado laranja:** área de explosão.
- **Quadrado preto:** bomba.

### Descrição da imagem: bomba antes, durante e depois da explosão em campo livre

A imagem mostra três pequenas grades lado a lado:

1. **Antes da explosão:** existe uma bomba preta posicionada aproximadamente no centro da grade.
2. **Durante a explosão:** a bomba desaparece e a explosão ocupa o quadrado central e os quadrados imediatamente acima, abaixo, à esquerda e à direita, formando uma cruz. A área atingida é indicada em laranja.
3. **Depois da explosão:** a grade volta a ficar vazia, sem bomba e sem área de explosão.

Isso indica que o alcance da bomba ocorre nas quatro direções ortogonais, formando uma cruz.

### Descrição da imagem: bomba antes, durante e depois da explosão em campo com paredes

A imagem apresenta novamente três grades, mas agora existem obstáculos:

1. **Antes da explosão:** a bomba preta está no centro. Há uma parede sólida cinza acima da bomba e uma parede frágil azul à esquerda dela.
2. **Durante a explosão:** a explosão laranja se propaga para os espaços livres e atinge a parede frágil. A parede sólida permanece bloqueando a propagação naquela direção.
3. **Depois da explosão:** a parede sólida continua no cenário, enquanto a parede frágil atingida pela explosão desaparece.

A figura reforça que paredes sólidas não podem ser destruídas, enquanto paredes frágeis podem ser removidas pela explosão.

---

# Paredes

- O cenário deverá ser composto por **paredes sólidas** e **paredes frágeis**.
- Todas as paredes deverão impedir a passagem do jogador.
- Todas as paredes deverão impedir a passagem dos inimigos.
- As paredes frágeis poderão ser destruídas por bombas.
- As paredes sólidas não poderão ser destruídas.

---

# Condição de vitória e derrota

## Vitória

O jogador vencerá se permanecer vivo quando todos os inimigos tiverem morrido.

## Derrota

O jogador perderá se:

- morrer ao colidir com um inimigo; ou
- morrer atingido pela explosão de uma bomba.

---

# Observação sobre o objetivo do trabalho

O jogo tem como objetivo avaliar a utilização das técnicas de programação aprendidas na disciplina, principalmente quanto ao uso de **sub-rotinas**.

Todo o código deverá ser produzido considerando a possibilidade de que novas funcionalidades sejam solicitadas futuramente.

Isso implica estruturar a solução de forma organizada, modular e extensível, evitando concentrar toda a lógica em um único trecho de código.

---

# Entrega

- Postar um arquivo texto contendo o link para o trabalho no **GitHub**; ou
- publicar o trabalho diretamente no material didático.

---

# Defesa do trabalho

- O professor realizará a defesa diretamente com o grupo.
- Não será necessário apresentar o trabalho para toda a turma.
- O professor poderá fazer perguntas sobre **qualquer parte do código** para **qualquer integrante do grupo**.
- A nota da apresentação será **individual**.
- As notas relacionadas ao código também dependem do desempenho do aluno durante a apresentação.
- **Sem apresentação, o trabalho terá nota zero.**

## Etapas da defesa

A apresentação será feita em duas etapas.

### Etapa 1 - apresentação da equipe

A equipe apresenta o trabalho em conjunto.

Durante essa etapa, podem ser feitas perguntas individuais para qualquer integrante.

Mesmo sendo uma apresentação em grupo, a nota da apresentação continua sendo individual.

### Etapa 2 - alteração individual do código

O professor solicitará modificações no código apresentado.

Cada integrante deverá realizar individualmente as alterações solicitadas, **sem auxílio dos demais membros da equipe**.

---

# Critérios de avaliação

## 1. Funcionalidades - 60% da nota

- A parte de funcionalidades corresponde a **60% da nota**.
- Existem **15 itens**.
- Cada item vale **0,4 ponto**.
- O item só será pontuado se o aluno souber explicar como foi implementado.

### Checklist de funcionalidades

- [ ] O jogador se move corretamente para todas as direções.
- [ ] O jogador consegue se mover em uma área com bomba.
- [ ] O jogador é bloqueado por qualquer tipo de parede.
- [ ] O jogador se move sem bugs.
- [ ] O jogador consegue colocar bombas onde está.
- [ ] O jogador não consegue colocar uma segunda bomba enquanto outra está no mapa.
- [ ] O jogador consegue colocar uma segunda bomba após a explosão da anterior.
- [ ] O jogador morre quando colide com um inimigo.
- [ ] O jogador morre quando uma bomba explode perto dele.
- [ ] O inimigo se move conforme solicitado.
- [ ] A bomba não destrói paredes sólidas.
- [ ] A bomba destrói paredes frágeis.
- [ ] A explosão da bomba é visível.
- [ ] A condição de vitória é atingida corretamente.
- [ ] A condição de derrota é atingida corretamente.

## 2. Técnicas - 40% da nota

- A parte técnica corresponde a **40% da nota**.
- Existem **4 itens**.
- Cada item vale **1,0 ponto**.
- O item só será pontuado se o aluno souber explicar como foi implementado.

### Checklist técnico

- [ ] Identificação dos desenvolvedores e comentários pertinentes no código.
- [ ] O programa possui sub-rotinas.
- [ ] As sub-rotinas utilizam parâmetros e referências corretamente.
- [ ] Há boa segmentação das sub-rotinas e bom emprego das técnicas estudadas.

---

# Penalidades e observações finais

> **Obs. 1:** cópia de soluções de colegas ou da internet acarretará **nota zero para todos os envolvidos**.

> **Obs. 2:** trabalhos entregues e/ou apresentados em atraso receberão um desconto inicial de **20% na nota final**, além de um desconto progressivo de **10% por semana de atraso**.

Exemplo apresentado no enunciado: se o trabalho for apresentado na semana seguinte à data estipulada, a nota máxima passa a ser **70%**.

---

# Resumo das regras funcionais para outra IA

Para facilitar a interpretação por uma IA que venha a implementar ou analisar o projeto, o comportamento esperado pode ser resumido assim:

1. O jogo ocorre em um mapa baseado em grade.
2. Existem quatro elementos fundamentais de cenário:
   - espaços livres;
   - paredes sólidas;
   - paredes frágeis;
   - bombas.
3. O jogador pode se deslocar nas quatro direções permitidas pelo mapa.
4. Paredes bloqueiam movimentação.
5. O jogador pode ocupar ou atravessar uma posição onde exista bomba, conforme exigido pelo enunciado.
6. O jogador pode colocar uma bomba na própria posição.
7. Só pode existir uma bomba ativa por vez.
8. Após determinado tempo, a bomba explode.
9. A explosão deve aparecer visualmente.
10. A explosão se propaga em formato de cruz nas quatro direções ortogonais.
11. Paredes sólidas não são destruídas.
12. Paredes frágeis atingidas são destruídas.
13. Jogador ou inimigo dentro da área da explosão morre.
14. O inimigo realiza movimentos aleatórios de 1, 2 ou 3 quadrados, respeitando obstáculos.
15. O jogador morre ao colidir com um inimigo.
16. O jogador vence quando todos os inimigos morrerem e ele continuar vivo.
17. O jogador perde quando morrer por colisão com inimigo ou por explosão.
18. A implementação deve priorizar sub-rotinas, modularização e facilidade de extensão futura.
