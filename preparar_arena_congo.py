"""Adapta materiais do mapa lateral de Congo's Caper para uma arena vista de cima.

Execute na raiz com Python e Pillow. Preserva a imagem original e as regras
da fase 7; os tres materiais e duas variantes de piso usam recortes reais.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageEnhance

ROOT = Path(__file__).resolve().parent
fonte = Image.open(ROOT / "assets/SNES - Congo's Caper - Stages - 1-6 - 1st Boss.png").convert('RGBA')

def tile(caixa):
    return fonte.crop(caixa).resize((46, 46), Image.Resampling.NEAREST)

pisos = [tile((288, 350, 320, 382)), tile((304, 366, 336, 398)), tile((832, 368, 864, 400))]
pisos = [ImageEnhance.Brightness(p).enhance(.63) for p in pisos]
parede = tile((64, 32, 96, 64))
d = ImageDraw.Draw(parede)
d.rectangle((0, 0, 45, 45), outline=(69, 22, 43), width=3)
d.line((3, 42, 3, 3, 42, 3), fill=(249, 148, 169), width=3)
d.line((5, 42, 42, 42, 42, 5), fill=(100, 28, 53), width=4)
# A textura estriada original vira um bloco fragil fossilizado de cor ambar.
caixa = tile((32, 160, 64, 192))
pixels = caixa.load()
for y in range(46):
    for x in range(46):
        r, g, b, a = pixels[x, y]
        luz = (r * 30 + g * 59 + b * 11) // 100
        pixels[x, y] = (min(255, 85 + luz), min(220, 48 + luz), min(130, 15 + luz // 2), a)
d = ImageDraw.Draw(caixa)
d.rectangle((0, 0, 45, 45), outline=(76, 43, 19), width=3)
d.line((3, 42, 3, 3, 42, 3), fill=(255, 221, 144), width=2)
d.line((5, 42, 42, 42, 42, 5), fill=(112, 63, 25), width=3)
atlas = Image.new('RGBA', (46, 230))
for i, material in enumerate([pisos[0], parede, caixa, pisos[1], pisos[2]]):
    atlas.alpha_composite(material, (0, i * 46))
atlas.save(ROOT / 'assets/arena_congo.png')
