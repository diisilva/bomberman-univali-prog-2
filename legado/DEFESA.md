# Defesa — versão legada

## Objetivo da versão

A versão legada mantém a identidade visual do protótipo de console, porém resolve duas limitações: o Windows Terminal não permite ao programa controlar o zoom de forma confiável e um console não renderiza PNG com transparência. Por isso a aparência clássica foi recriada em uma janela Win32.

## Como as duas versões são separadas

`bomberman_legado.cpp` define a macro `VERSAO_LEGADO` e inclui `../bomberman.cpp`. Durante a compilação, os blocos entre `#ifdef VERSAO_LEGADO` selecionam:

- células maiores de 56×56;
- fonte Consolas;
- fundo preto;
- cores mais próximas dos blocos do console;
- molduras textuais e título “Bomberman Legado”;
- nome próprio para a classe e a janela.

Sem essa definição, a mesma fonte produz a versão moderna. A macro afeta apresentação e dimensões, não as regras do jogo.

## Tecnologias

- **Win32 API:** cria a janela, recebe teclado e controla timer, pintura, DPI e fechamento;
- **GDI:** cria o quadro em memória e faz a cópia final com `BitBlt`, evitando flickering;
- **GDI+:** carrega os PNGs com alpha e os desenha proporcionalmente;
- **C++17:** implementa toda a lógica com matriz, structs, vetores e sub-rotinas.

Essas APIs são recursos nativos do Windows, não um motor gráfico. Movimento, colisões e explosões não são fornecidos por elas.

## Assets compartilhados

Como o executável fica em `legado/`, `carregarSprite()` tenta primeiro uma pasta `assets` ao lado dele e depois `../assets`, localizada na raiz. Os objetos `Image` são carregados uma vez, reutilizados durante o loop e liberados no fechamento.

## Pontos para demonstrar

1. comparar a aparência preta e monoespaçada com a edição moderna;
2. mostrar que o mapa é maior, com 840×904 pixels em 100%;
3. demonstrar os PNGs com transparência;
4. executar movimentação, bomba e colisões;
5. explicar que ambas as builds chamam as mesmas sub-rotinas;
6. mostrar que somente os blocos condicionais de renderização mudam.

O arquivo `bomberman_legado_comentado.cpp` reúne comentários detalhados sobre
esses pontos e pode ser aberto durante o estudo da defesa. Como ele inclui o
mesmo núcleo com `VERSAO_LEGADO`, não apresenta uma cópia desatualizada das
regras.

## Pergunta provável

**Por que não manter o console verdadeiro?**  
Porque o zoom pertence ao Windows Terminal e porque o console não desenha PNG. A janela nativa permite preservar o estilo clássico com tamanho previsível e imagens reais.
