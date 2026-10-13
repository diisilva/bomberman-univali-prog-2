"""Recorta os sprites originais catalogados e adapta os mapas para tiles de arena.

Requer Pillow e numpy. Fontes e coordenadas ficam no catalogo versionado;
nenhuma imagem de origem e sobrescrita. Execute da raiz do projeto.
"""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / 'assets'
catalogo = json.loads((ROOT / 'testes/catalogo_sprites_novos.json').read_text(encoding='utf-8'))
header = ['// Gerado por preparar_bosses_arenas.py. Celulas transparentes 160x160.',
          'struct LinhaAnimacaoBoss { int inicio, quantidade; };']
for nome, dados in catalogo.items():
    fonte = Image.open(ASSETS / dados['fonte']).convert('RGBA')
    if nome == 'hornet':
        pixels = np.array(fonte)
        pixels[np.all(pixels[:, :, :3] == [0, 115, 147], axis=2), 3] = 0
        fonte = Image.fromarray(pixels)
    quadros, linhas = [], []
    for nlinha, linha in enumerate(dados['linhas']):
        recortes = []
        for x, y, w, h in linha['quadros']:
            recorte = fonte.crop((x, y, x+w, y+h))
            caixa = recorte.getbbox()
            recortes.append(recorte.crop(caixa) if caixa else Image.new('RGBA', (1, 1)))
        # Escala constante dentro da sequencia: os pulsos/poses nao mudam de tamanho.
        escala = min(144 / max(i.width for i in recortes), 144 / max(i.height for i in recortes))
        linhas.append((len(quadros), len(recortes)))
        for recorte in recortes:
            recorte = recorte.resize((max(1, round(recorte.width*escala)), max(1, round(recorte.height*escala))), Image.Resampling.NEAREST)
            quadro = Image.new('RGBA', (160, 160))
            efeito = nome == 'hornet' and nlinha in (12,13,14,17,21,22,27,28,29,30,35,39,40,41,42)
            quadro.alpha_composite(recorte, ((160-recorte.width)//2, (160-recorte.height)//2 if efeito else 152-recorte.height))
            quadros.append(quadro)
    atlas = Image.new('RGBA', (16*160, ((len(quadros)+15)//16)*160))
    for i, quadro in enumerate(quadros):
        atlas.alpha_composite(quadro, ((i%16)*160, (i//16)*160))
    atlas.save(ASSETS / f'{nome}_animacoes.png', optimize=True)
    header.append(f'const LinhaAnimacaoBoss ANIM_{nome.upper()}[] = {{' + ','.join('{%d,%d}' % i for i in linhas) + '};')
    print(nome, len(linhas), 'sequencias,', len(quadros), 'quadros')
(ROOT / 'bosses_animacoes.h').write_text('\n'.join(header)+'\n', encoding='utf-8')

# Mapas de mundo nao sao tilesets: montamos pecas quadradas a partir de seus
# materiais, com bordas legiveis e textura original no centro.
mapa = Image.open(ASSETS / 'TurboGrafx-16 - Quiz Caravan Cult Q - World Maps - Bomberman World Map.png').convert('RGBA')
lang = Image.open(ASSETS / 'Sega Genesis - Langrisser II (JPN) - Maps - Scenario 1.png').convert('RGBA')
def recorte(fonte, caixa):
    return fonte.crop(caixa).resize((46,46), Image.Resampling.NEAREST)
def bloco(fonte, caixa, borda, luz):
    tile = recorte(fonte, caixa)
    d = ImageDraw.Draw(tile)
    d.rectangle((0,0,45,45), outline=borda, width=3)
    d.line((3,42,3,3,42,3), fill=luz, width=2)
    d.line((4,42,42,42,42,4), fill=borda, width=3)
    return tile
tiles = [
    [recorte(mapa,(117,230,129,237)), bloco(mapa,(154,56,174,68),(36,57,61),(187,214,210)), bloco(mapa,(179,102,200,113),(108,66,25),(255,229,89))],
    [recorte(lang,(170,193,202,225)), bloco(lang,(2,2,26,26),(42,43,48),(174,174,176)), bloco(lang,(73,193,95,215),(66,37,19),(209,166,84))]
]
atlas = Image.new('RGBA',(92,184))
for col, grupo in enumerate(tiles):
    for row, tile in enumerate(grupo):
        atlas.alpha_composite(tile,(col*46,row*46))
# Variantes de piso: relva do mapa de Langrisser fora das trilhas; arbustos
# pequenos do mapa Bomber em algumas casas. A colisao continua na matriz.
grama = recorte(lang,(413,222,445,254))
atlas.alpha_composite(grama,(46,138))
relva = tiles[0][0].copy()
arbusto = mapa.crop((194,238,209,251)).resize((17,14),Image.Resampling.NEAREST)
relva.alpha_composite(arbusto,(27,30))
atlas.alpha_composite(relva,(0,138))
atlas.save(ASSETS/'arenas_novas.png')
atlas.resize((368,736),Image.Resampling.NEAREST).save(ROOT/'testes/preview_tiles_novos.png')
