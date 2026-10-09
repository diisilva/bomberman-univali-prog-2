"""Recorta as poses de fim de partida da folha fornecida, sem alterar o original."""
from pathlib import Path
from PIL import Image

PASTA = Path(__file__).resolve().parent / "assets"
folha = Image.open(PASTA / "personagem_principal_sprite_v2.png").convert("RGBA")
fundos = {(112, 0, 170), folha.getpixel((10, 10))[:3]}
for nome, y in (("jogador_vitoria.png", 182), ("jogador_derrota.png", 562)):
    atlas = Image.new("RGBA", (16 * 9, 26))
    for quadro in range(9):
        recorte = folha.crop((18 + quadro * 18, y, 34 + quadro * 18, y + 26))
        recorte.putdata([
            (0, 0, 0, 0) if pixel[:3] in fundos else pixel
            for pixel in recorte.get_flattened_data()
        ])
        atlas.paste(recorte, (quadro * 16, 0))
    atlas.save(PASTA / nome)
