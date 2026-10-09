Analise primeiro todo o projeto e principalmente o arquivo bomberman.cpp antes de alterar qualquer coisa.

IMPORTANTE:
- Não faça commit.
- Não altere as regras ou a lógica do jogo sem necessidade.
- Preserve matriz, structs, vetores e sub-rotinas já existentes.
- Preserve movimentação, inimigos, bomba, explosão, colisões, vitória, derrota e modo automático.
- Faça alterações incrementais e fáceis de explicar academicamente.
- Não transforme o projeto em uma arquitetura exageradamente complexa.
- O projeto continua sendo C++ no Windows.

==================================================
1. PROBLEMA ATUAL DE TAMANHO / ESCALA
   ==================================================

Hoje o executável abre visualmente muito pequeno.

O jogo só fica com uma dimensão confortável depois que eu aumento manualmente o zoom do Windows Terminal com Ctrl + scroll do mouse.

Quero que o jogo JÁ ABRA com uma dimensão visual próxima do tamanho que atualmente obtenho depois de dar zoom.

Hoje existem configurações como:

SetCurrentConsoleFontEx(...)
dwFontSize.Y = 28

COORD tamanhoBuffer{38, 24};

SMALL_RECT tamanhoJanela{0, 0, 37, 23};

Investigue corretamente o motivo de isso não produzir o mesmo tamanho visual quando o executável é aberto no Windows Terminal.

NÃO quero uma solução que dependa do usuário fazer:

Ctrl + mouse wheel
Ctrl + +
configuração manual do Windows Terminal

O jogo deve abrir com tamanho adequado sozinho.

Porém existe uma segunda mudança importante:

vamos começar a utilizar imagens PNG dentro do jogo.

Como um console convencional não é apropriado para renderizar PNGs corretamente, NÃO tente desenhar PNG usando caracteres ASCII, ANSI art, base64, hacks de terminal ou qualquer solução semelhante.

Se for necessário abandonar apenas a camada visual baseada no console e criar uma pequena janela Win32/GDI/GDI+ para renderização, faça isso.

A lógica do jogo deve continuar praticamente independente da renderização.

Priorize uma solução nativa do Windows e C++ que seja simples de compilar.

Evite bibliotecas externas grandes sem necessidade.

==================================================
2. TAMANHO DA JANELA
   ==================================================

Quero uma janela de jogo aproximadamente no tamanho visual mostrado na referência maior que forneci.

Não quero:

- janela minúscula;
- sprites minúsculos;
- mapa ocupando apenas uma pequena parte da tela;
- elementos borrados;
- imagens esticadas;
- janela enorme desnecessariamente.

Defina constantes claras, por exemplo:

TAMANHO_CELULA
LARGURA_JOGO
ALTURA_JOGO

O tamanho de cada célula do mapa deve ser suficientemente grande para que os sprites sejam claramente reconhecíveis.

Como referência inicial, investigue algo na faixa de:

40x40
48x48

pixels por célula.

Não considere esses valores obrigatórios.

Calcule o tamanho final considerando que o mapa atualmente possui:

LINHAS = 13
COLUNAS = 15

A janela deve comportar:

- cabeçalho;
- pontuação;
- quantidade de inimigos;
- informações da bomba;
- mapa;
- instruções na parte inferior.

Não permita clipping de texto ou do mapa.

==================================================
3. ADICIONAR SPRITES PNG
   ==================================================

Vamos substituir alguns símbolos atuais por PNGs.

Assets:

Bomba:
D:\\algoritmos_prog_2\\bomberman-univali-prog-2\\assets\\bomba.png

Jogador:
D:\\algoritmos_prog_2\\bomberman-univali-prog-2\\assets\\bomberman.png

Inimigo:
D:\\algoritmos_prog_2\\bomberman-univali-prog-2\\assets\\perfil.png

Atualmente temos aproximadamente:

PJ = jogador
IN = inimigo
BO = bomba

Esses textos devem deixar de representar visualmente esses elementos quando os sprites estiverem funcionando.

Mapeamento:

bomberman.png
-> jogador

perfil.png
-> todos os inimigos

bomba.png
-> bomba

==================================================
4. QUALIDADE DOS SPRITES
   ==================================================

Tenha MUITO cuidado com tamanho, resolução e proporção dos PNGs.

Não quero imagens:

- borradas;
- achatadas;
- esticadas;
- pixelizadas desnecessariamente;
- cortadas;
- maiores que a célula;
- desalinhadas dentro da célula.

Antes de implementar, verifique programaticamente:

- largura original;
- altura original;
- proporção;
- presença de transparência/alpha.

Ao desenhar um sprite dentro de uma célula:

1. preserve a proporção original;
2. centralize dentro da célula;
3. reserve uma pequena margem;
4. não distorça a imagem;
5. preserve transparência;
6. utilize interpolação apropriada para o tipo de imagem.

Crie uma função reutilizável para esse comportamento.

Algo conceitualmente semelhante a:

desenharSprite(...)
desenharSpriteNaCelula(...)

A função deve calcular automaticamente:

scale = min(
larguraDisponivel / larguraOriginal,
alturaDisponivel / alturaOriginal
)

Depois centralizar a imagem.

Não replique essa lógica separadamente para jogador, inimigo e bomba.

==================================================
5. SVG / ASSETS VETORIAIS FUTUROS
   ==================================================

Prepare a camada de renderização para que futuramente possam existir outros assets.

Caso algum SVG seja introduzido posteriormente, NÃO faça rasterização em uma resolução arbitrária pequena.

A resolução de rasterização deve considerar:

- tamanho real em que o elemento será apresentado;
- DPI da janela;
- proporção original;
- resolução necessária para evitar serrilhamento e borrão.

Não gere um bitmap de baixa resolução para depois ampliá-lo.

O objetivo é evitar aquela aparência de imagem ruim típica de:

SVG -> PNG muito pequeno -> upscale.

==================================================
6. RESOLUÇÃO E DPI DO WINDOWS
   ==================================================

Considere corretamente High DPI / scaling do Windows.

Minha máquina pode estar usando escala de:

100%
125%
150%

ou outro valor.

Não assuma que 1 unidade lógica sempre corresponde visualmente a 1 pixel da mesma maneira em qualquer configuração.

Se estiver usando Win32, torne o aplicativo DPI-aware da maneira mais simples e correta possível.

A janela e os sprites precisam continuar com proporções boas mesmo com scaling do Windows.

==================================================
7. MAPA
   ==================================================

Preserve a identidade visual atual:

- chão verde;
- parede sólida cinza;
- parede frágil azul;
- explosão destacada;
- jogador;
- inimigos;
- bomba.

Por enquanto NÃO precisamos de PNG para:

- chão;
- parede sólida;
- parede frágil;
- explosão.

Eles podem continuar sendo desenhados geometricamente.

Sugestão:

VAZIO:
retângulo verde

PAREDE_SOLIDA:
retângulo cinza com alguma diferenciação visual simples

PAREDE_FRAGIL:
retângulo azul

EXPLOSAO:
laranja/amarelo

Não exagere nos efeitos.

O foco é clareza visual.

==================================================
8. ORGANIZAÇÃO DO CÓDIGO
   ==================================================

Hoje a função desenharCelula() mistura estado do jogo e apresentação visual.

Quero uma pequena separação entre:

LÓGICA
RENDERIZAÇÃO

Mas sem overengineering.

Pode criar funções/sub-rotinas como:

carregarAssets()
liberarAssets()

desenharMapa()
desenharCelula()
desenharSpriteNaCelula()

desenharHUD()
desenharJogador()
desenharInimigo()
desenharBomba()
desenharExplosao()

A lógica atual de:

criarMapa()
moverJogador()
colocarBomba()
adicionarRaio()
iniciarExplosao()
atualizarBomba()
moverInimigos()
verificarVitoria()
atualizarBot()

deve ser preservada sempre que possível.

==================================================
9. PATH DOS ASSETS
   ==================================================

Os caminhos absolutos acima são apenas para você localizar os arquivos agora.

NÃO deixe no código final algo como:

D:\\algoritmos_prog_2\\bomberman-univali-prog-2\\assets\\bomba.png

O executável precisa funcionar dentro do projeto.

Utilize caminhos relativos, preferencialmente:

assets/bomba.png
assets/bomberman.png
assets/perfil.png

A resolução do caminho deve funcionar quando o projeto for executado normalmente.

Se necessário, trate corretamente o diretório do executável.

==================================================
10. TRATAMENTO DE ERROS
    ==================================================

Se algum asset não puder ser carregado:

NÃO deixe o jogo crashar.

Mostre uma mensagem clara indicando qual arquivo não foi encontrado.

Durante desenvolvimento, pode existir fallback visual simples:

PJ
IN
BO

Mas o caminho normal deve utilizar os sprites.

==================================================
11. PERFORMANCE
    ==================================================

Os PNGs devem ser carregados UMA VEZ na inicialização.

NÃO carregue:

bomberman.png
perfil.png
bomba.png

a cada frame.

Fluxo esperado:

inicialização
carregar assets

loop do jogo
atualizar lógica
renderizar usando assets já carregados

encerramento
liberar recursos

Evite flickering.

Se necessário, utilize double buffering.

==================================================
12. NÃO ALTERAR O GAMEPLAY
    ==================================================

Depois da mudança visual, valide novamente obrigatoriamente:

[ ] jogador move em todas as direções
[ ] paredes bloqueiam jogador
[ ] jogador pode colocar bomba
[ ] só existe uma bomba ativa
[ ] nova bomba só pode ser colocada depois da anterior
[ ] explosão continua funcionando
[ ] parede sólida não quebra
[ ] parede frágil quebra
[ ] jogador morre na explosão
[ ] jogador morre ao tocar inimigo
[ ] inimigos continuam se movimentando
[ ] inimigos morrem na explosão
[ ] vitória continua funcionando
[ ] derrota continua funcionando
[ ] modo automático continua funcionando

Não modifique esses comportamentos apenas para adequar a nova renderização.

==================================================
13. COMPILAÇÃO
    ==================================================

Identifique como o projeto atualmente está sendo compilado.

Depois da implementação:

1. compile;
2. corrija warnings relevantes;
3. execute;
4. valide carregamento dos PNGs;
5. valide tamanho inicial da janela;
6. valide resolução;
7. valide movimentação;
8. valide bomba;
9. valide inimigos;
10. valide fechamento da aplicação.

Caso passe a utilizar GDI+, adicione corretamente os includes e bibliotecas necessárias no processo de build.

Não quero que a solução funcione apenas dentro da IDE.

O executável gerado precisa funcionar.

==================================================
14. RESULTADO VISUAL ESPERADO
    ==================================================

Quando eu executar o jogo:

ANTES:
o jogo abre pequeno e eu preciso dar zoom manualmente no Windows Terminal.

DEPOIS:
o jogo já deve abrir em uma janela confortável e com o mapa em tamanho semelhante à minha segunda imagem de referência.

Além disso:

PJ
deve ser substituído visualmente pelo bomberman.png.

IN
deve ser substituído pelo perfil.png.

BO
deve ser substituído pela bomba.png.

Os sprites devem estar:

- centralizados;
- proporcionais;
- nítidos;
- com transparência correta;
- dimensionados conforme a célula.

==================================================
15. ANTES DE CODIFICAR
    ==================================================

Primeiro me explique brevemente:

1. por que o zoom atual do Windows Terminal não é algo que o programa controla de maneira confiável;
2. qual será a estratégia para fazer a aplicação já abrir no tamanho correto;
3. qual tecnologia será utilizada para desenhar os PNGs;
4. como será preservada a lógica atual;
5. quais arquivos serão modificados;
6. como será tratado DPI/scaling;
7. como os sprites serão redimensionados sem deformação.

Depois disso, implemente.

Não faça commit.
Não faça mudanças além do necessário.