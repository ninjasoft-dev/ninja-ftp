"""Gera os ícones do Ninja Transfer a partir da marca oficial da NinjaSoft."""

from __future__ import annotations

import base64
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
RESOURCE_DIR = ROOT / "src" / "interface" / "resources"
LIGHT_MARK_PATH = RESOURCE_DIR / "ninjasoft-ninja-light.png"
DARK_MARK_PATH = RESOURCE_DIR / "ninjasoft-ninja-dark.png"
ADAPTIVE_MARK_PATH = RESOURCE_DIR / "ninjasoft-ninja-adaptive.png"

MUTED = (185, 187, 209, 255)
ACCENT = (157, 114, 239, 255)
BLUE = (78, 97, 196, 255)
SUCCESS = (85, 214, 163, 255)


def _remove_white_background(image: Image.Image) -> Image.Image:
    """Transforma o fundo branco da versão azul em transparência suave."""
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, source_alpha = pixels[x, y]
            distance_from_white = max(255 - red, 255 - green, 255 - blue)
            alpha = source_alpha * distance_from_white // 255
            pixels[x, y] = red, green, blue, alpha
    return image


def _square_mark(
    source_path: Path,
    size: int,
    *,
    remove_white_background: bool = False,
) -> Image.Image:
    """Centraliza uma variante da marca em uma tela quadrada transparente."""
    with Image.open(source_path) as source:
        mark = source.convert("RGBA")
    if remove_white_background:
        mark = _remove_white_background(mark)
    available = round(size * 0.9)
    scale = min(available / mark.width, available / mark.height)
    mark = mark.resize(
        (round(mark.width * scale), round(mark.height * scale)),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    position = ((size - mark.width) // 2, (size - mark.height) // 2)
    canvas.alpha_composite(mark, position)
    return canvas


def generate_application_icons() -> None:
    """Atualiza os ícones adaptativos da janela e dos pacotes de distribuição."""
    light_master = _square_mark(
        LIGHT_MARK_PATH, 512, remove_white_background=True
    )
    dark_master = _square_mark(DARK_MARK_PATH, 512)
    adaptive_master = _square_mark(ADAPTIVE_MARK_PATH, 512)
    for size in (16, 32, 48, 480):
        target_dir = RESOURCE_DIR / f"{size}x{size}"
        target_dir.mkdir(parents=True, exist_ok=True)
        adaptive_master.resize((size, size), Image.Resampling.LANCZOS).save(
            target_dir / "filezilla.png"
        )
        light_master.resize((size, size), Image.Resampling.LANCZOS).save(
            target_dir / "ninjatransfer_light.png"
        )
        dark_master.resize((size, size), Image.Resampling.LANCZOS).save(
            target_dir / "ninjatransfer_dark.png"
        )

    ico_sizes = [
        (16, 16),
        (24, 24),
        (32, 32),
        (48, 48),
        (64, 64),
        (128, 128),
        (256, 256),
    ]
    adaptive_master.save(
        RESOURCE_DIR / "NinjaTransfer.ico", format="ICO", sizes=ico_sizes
    )

    encoded_mark = base64.b64encode(ADAPTIVE_MARK_PATH.read_bytes()).decode("ascii")
    svg = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" '
        'xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 512 512">\n'
        '  <image x="26" y="26" width="460" height="460" '
        'preserveAspectRatio="xMidYMid meet" '
        f'xlink:href="data:image/png;base64,{encoded_mark}"/>\n'
        '</svg>\n'
    )
    (RESOURCE_DIR / "filezilla.svg").write_text(
        svg, encoding="utf-8", newline="\n"
    )


def _new_toolbar_icon() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    """Cria uma tela transparente compatível com o tema de ícones padrão."""
    image = Image.new("RGBA", (480, 480), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def _draw_folder(
    draw: ImageDraw.ImageDraw,
    box: tuple[int, int, int, int],
    colour: tuple[int, ...],
) -> None:
    """Desenha uma pasta simples sem preenchimento claro."""
    left, top, right, bottom = box
    tab_width = (right - left) // 3
    draw.line(
        (
            left,
            top + 30,
            left,
            top,
            left + tab_width,
            top,
            left + tab_width + 24,
            top + 30,
        ),
        fill=colour,
        width=28,
        joint="curve",
    )
    draw.rounded_rectangle(
        (left, top + 30, right, bottom), radius=24, outline=colour, width=28
    )


def generate_toolbar_icons() -> None:
    """Substitui os ícones com blocos claros por símbolos transparentes."""
    target_dir = RESOURCE_DIR / "default" / "480x480"

    image, draw = _new_toolbar_icon()
    draw.rounded_rectangle((74, 70, 406, 410), radius=36, outline=MUTED, width=30)
    draw.line((74, 142, 406, 142), fill=BLUE, width=30)
    for y in (218, 286, 354):
        draw.ellipse((112, y - 14, 140, y + 14), fill=ACCENT)
        draw.line((172, y, 350, y), fill=MUTED, width=24)
    image.save(target_dir / "logview.png")

    image, draw = _new_toolbar_icon()
    _draw_folder(draw, (48, 76, 198, 206), ACCENT)
    draw.line((122, 212, 122, 382, 218, 382), fill=MUTED, width=26)
    draw.line((122, 292, 218, 292), fill=MUTED, width=26)
    _draw_folder(draw, (234, 222, 430, 340), BLUE)
    _draw_folder(draw, (234, 330, 430, 438), SUCCESS)
    image.save(target_dir / "localtreeview.png")

    image, draw = _new_toolbar_icon()
    for y, colour in ((82, ACCENT), (202, BLUE), (322, SUCCESS)):
        draw.rounded_rectangle(
            (70, y, 410, y + 82), radius=22, outline=colour, width=28
        )
        draw.ellipse((108, y + 28, 134, y + 54), fill=colour)
        draw.line((172, y + 41, 350, y + 41), fill=MUTED, width=22)
    image.save(target_dir / "remotetreeview.png")

    image, draw = _new_toolbar_icon()
    for y in (110, 230, 350):
        draw.rounded_rectangle(
            (52, y - 35, 310, y + 35),
            radius=20,
            outline=MUTED,
            width=26,
        )
    draw.line((370, 82, 370, 346), fill=ACCENT, width=34)
    draw.line((310, 288, 370, 354, 430, 288), fill=ACCENT, width=34, joint="curve")
    draw.ellipse((92, 96, 120, 124), fill=SUCCESS)
    image.save(target_dir / "queueview.png")

    image, draw = _new_toolbar_icon()
    draw.arc((72, 72, 408, 408), 202, 350, fill=ACCENT, width=38)
    draw.polygon(((399, 101), (423, 206), (319, 180)), fill=ACCENT)
    draw.arc((72, 72, 408, 408), 22, 170, fill=SUCCESS, width=38)
    draw.polygon(((81, 379), (57, 274), (161, 300)), fill=SUCCESS)
    image.save(target_dir / "refresh.png")


def main() -> None:
    """Executa a geração completa e falha cedo quando a marca não está disponível."""
    missing_marks = [
        path
        for path in (LIGHT_MARK_PATH, DARK_MARK_PATH, ADAPTIVE_MARK_PATH)
        if not path.exists()
    ]
    if missing_marks:
        raise FileNotFoundError(f"Marcas não encontradas: {missing_marks}")
    generate_application_icons()
    generate_toolbar_icons()


if __name__ == "__main__":
    main()
