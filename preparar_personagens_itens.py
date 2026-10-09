"""Prepara os assets existentes: personagens, poses finais e icones de itens."""
from pathlib import Path
from PIL import Image, ImageDraw

PASTA = Path(__file__).resolve().parent / "assets"
folha = Image.open(PASTA / "novos_perssonagens.png").convert("RGBA")

def recortar(x, y, w, h, fundo):
    imagem = folha.crop((x, y, x+w, y+h))
    imagem.putdata([(0, 0, 0, 0) if p[:3] == fundo else p for p in imagem.get_flattened_data()])
    return imagem

for i, nome in enumerate(("branco", "preto", "azul", "vermelho")):
    x, y = (196 if i%2 else 3), (305 if i>=2 else 47)
    fundo = (64, 144, 56) if i==0 else (40, 160, 192)
    atlas = Image.new("RGBA", (48, 96))
    for direcao in range(4):
        for quadro in range(3):
            atlas.paste(recortar(x+quadro*17, y+direcao*25, 16, 24, fundo), (quadro*16, direcao*24))
    atlas.save(PASTA / f"personagem_{nome}.png")
    # A folha original oferece sete poses de cada estado, por variante.
    for estado, linha, altura in (("derrota", 158, 24), ("vitoria", 205, 32)):
        atlas = Image.new("RGBA", (112, 32))
        for quadro in range(7):
            atlas.paste(recortar(x+quadro*17, linha+(258 if i>=2 else 0), 16, altura, fundo), (quadro*16, 32-altura))
        atlas.save(PASTA / f"{nome}_{estado}.png")

# Usa a sequencia completa de morte (queda e desaparecimento), e nao apenas
# a sequencia Knock-Out. Preserva face/contorno; adapta a paleta do capacete.
morte = Image.open(PASTA / "jogador_derrota.png").convert("RGBA")
for nome, claro, medio, escuro in (
    ("branco", (248,248,248),(224,224,224),(96,96,96)),
    ("preto", (88,88,88),(48,48,48),(24,24,24)),
    ("azul", (64,96,255),(0,0,224),(0,0,112)),
    ("vermelho", (255,112,40),(248,56,0),(144,24,0)),
):
    pixels=[]
    for r,g,b,a in morte.get_flattened_data():
        if a and r==g==b and r>=96:
            r,g,b = claro if r>=240 else medio if r>=180 else escuro
        elif nome=="vermelho" and a and r>120 and b>80 and g<120:
            r,g,b = (40,200,40)
        pixels.append((r,g,b,a))
    variante=morte.copy(); variante.putdata(pixels)
    variante.save(PASTA / f"{nome}_derrota.png")

icones = Image.open(PASTA / "icons_blocos.png").convert("RGBA")
atlas = Image.new("RGBA", (256,32))
# Ordem do enum TipoItem: chute, teleporte, fogo, bomba+, vida, relogio, protecao, atravessar.
origens = ((6,54),(6,75),(46,75),(26,75),None,(46,54),(86,54),(26,54))
for i, origem in enumerate(origens):
    if origem:
        x,y=origem
        icone=icones.crop((x,y,x+16,y+16)).resize((32,32), Image.Resampling.NEAREST)
    else:
        icone=Image.new("RGBA",(32,32),(255,70,130,255))
        icone.paste(recortar(3,47,16,24,(64,144,56)).resize((16,24),Image.Resampling.NEAREST),(8,4))
    desenho=ImageDraw.Draw(icone)
    if i==4: # Vida extra: retrato e cruz, em vez de letra.
        desenho.rectangle((20,19,29,28),fill=(255,255,255,255))
        desenho.rectangle((24,20,25,27),fill=(230,40,90,255))
        desenho.rectangle((21,23,28,24),fill=(230,40,90,255))
    if i==5: # Bomba-relogio: relogio sobre o sprite de bomba.
        desenho.ellipse((16,16,30,30),fill=(255,245,210,255),outline=(20,25,40,255),width=2)
        desenho.line((23,18,23,23,27,23),fill=(20,25,40,255),width=2)
    if i==7: # Atravessar: seta cruzando o bloco original.
        desenho.line((4,17,27,17),fill=(255,255,255,255),width=3)
        desenho.polygon(((23,12),(29,17),(23,22)),fill=(255,255,255,255))
    atlas.paste(icone,(i*32,0))
atlas.save(PASTA / "itens_especiais.png")
