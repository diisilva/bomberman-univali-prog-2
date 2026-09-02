# Defesa — versão moderna

Este é o documento oficial de defesa da versão moderna. O arquivo `ROTEIRO_DEFESA.md` pode continuar como material pessoal complementar.

## Visão geral

A versão moderna mantém toda a lógica acadêmica do Bomberman em C++ e substitui somente a antiga apresentação de console por uma janela nativa. Ela usa células de 52×52, área útil de 780×852 pixels em escala de 100% e sprites PNG proporcionais.

## Conteúdos da disciplina

- matriz fixa `mapa[LINHAS][COLUNAS]` para o cenário;
- structs `Posicao`, `Inimigo` e `Bomba`;
- vetores para inimigos e área de explosão;
- sub-rotinas específicas para mapa, movimento, bomba, dano, vitória e bot;
- parâmetros e referências em funções como `atingirPersonagens()`;
- fila e busca em largura em `proximoPassoDoBot()`.

## Interface

- **Win32 API:** `WinMain()` cria a janela e `processarMensagem()` recebe teclado, timer, pintura, DPI e fechamento;
- **GDI:** monta cada quadro em um bitmap de memória e usa `BitBlt()` para evitar flickering;
- **GDI+:** carrega os PNGs com transparência e usa `DrawImage()` para desenhá-los.

Não existe motor gráfico. Essas APIs cuidam de entrada e apresentação; as regras continuam implementadas no projeto.

## Fluxo de execução

1. o processo se declara DPI-aware;
2. GDI+ é inicializado;
3. a janela é criada no tamanho calculado;
4. os PNGs são carregados uma vez;
5. o timer calcula o tempo transcorrido e chama `atualizar()`;
6. `WM_PAINT` desenha HUD, mapa e rodapé em buffer duplo;
7. no fechamento, imagens, timer e GDI+ são liberados.

## Sprites

`desenharSpriteNaCelula()` calcula o menor fator entre largura e altura disponíveis. O mesmo valor é aplicado às duas dimensões, preservando a proporção. A função mantém margem, centraliza a imagem, respeita alpha e usa interpolação bicúbica de alta qualidade.

## Demonstração recomendada

1. abrir o menu e selecionar o modo manual;
2. demonstrar movimento e bloqueio pelas paredes;
3. colocar uma bomba e tentar colocar uma segunda;
4. mostrar explosão, parede frágil destruída e sólida preservada;
5. mostrar vitória ou derrota;
6. executar o modo automático 1,5x;
7. explicar no código a separação entre lógica e renderização.

## Diferença para a edição legada

A edição em `legado/` usa a mesma lógica e os mesmos assets, mas ativa `VERSAO_LEGADO` para selecionar células de 56×56, fonte Consolas, fundo preto, cores fortes e molduras semelhantes ao antigo console.
