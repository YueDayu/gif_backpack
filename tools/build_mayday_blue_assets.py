#!/usr/bin/env python3
"""Build ESP32-ready lyric assets from Mayday.Blue / MayScreen LRC files."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - user-facing CLI guard.
    raise SystemExit(
        "Pillow is required to render glyphs. Install it in a temporary venv with "
        "`python3 -m pip install pillow`, then rerun this script."
    ) from exc


TAG_RE = re.compile(r"\[([a-zA-Z]+):([^\]]*)\]")
TIME_RE = re.compile(r"\[(\d{1,3}):(\d{2})(?:[.:](\d{1,3}))?\]")
DEFAULT_SOURCE = Path(
    "/Users/yuedayu/code/gif_backpack_txt_render_web/data/mayday_blue_analysis/lrc"
)
DEFAULT_FONT_CANDIDATES = [
    Path("/Library/Fonts/Microsoft YaHei.ttf"),
    Path("/Library/Fonts/Microsoft YaHei UI.ttf"),
    Path("/Library/Fonts/msyh.ttc"),
    Path("/Library/Fonts/msyh.ttf"),
    Path.home() / "Library/Fonts/Microsoft YaHei.ttf",
    Path.home() / "Library/Fonts/Microsoft YaHei UI.ttf",
    Path.home() / "Library/Fonts/msyh.ttc",
    Path.home() / "Library/Fonts/msyh.ttf",
    Path("/System/Library/Fonts/Hiragino Sans GB.ttc"),
    Path("/System/Library/Fonts/STHeiti Medium.ttc"),
    Path("/System/Library/Fonts/Supplemental/Songti.ttc"),
]


@dataclass
class Song:
    file: str
    title: str
    artist: str
    album: str
    year: str
    line_count: int
    synthetic_timing: bool
    offset: int
    length: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-lrc-dir", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output-dir", type=Path, default=Path("data/lyrics"))
    parser.add_argument("--font", type=Path, default=None)
    parser.add_argument(
        "--font-index",
        type=int,
        default=None,
        help="TTC face index. Defaults to the bold Hiragino Sans GB face when available.",
    )
    parser.add_argument("--font-px", type=int, default=16)
    parser.add_argument("--threshold", type=int, default=128)
    parser.add_argument(
        "--stroke-px",
        type=int,
        default=0,
        help="Optional binary dilation radius after glyph rasterization; use only if the bold face is still too thin.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    stroke_px = max(0, args.stroke_px)
    source_dir = args.source_lrc_dir.expanduser()
    output_dir = args.output_dir
    if not source_dir.exists():
        raise SystemExit(f"source LRC dir not found: {source_dir}")

    font_path = args.font or find_font()
    if not font_path:
        raise SystemExit("No usable CJK font found. Pass --font /path/to/font.ttc")
    font_index = (
        args.font_index if args.font_index is not None else default_font_index(font_path)
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    for old in output_dir.glob("*.lrc"):
        old.unlink()
    pack_path = output_dir / "lyrics.pack"

    songs: list[Song] = []
    charset: set[str] = {" "}
    pack_chunks: list[bytes] = []
    pack_offset = 0

    for src in sorted(source_dir.glob("*.lrc")):
        text, synthetic_timing = make_playable_lrc(src.read_text(encoding="utf-8"))
        meta = parse_meta(text)
        line_count = count_timed_lines(text)
        if line_count == 0:
            continue
        for line_text in timed_line_texts(text):
            charset.update(line_text)
        encoded = text.encode("utf-8")
        pack_chunks.append(encoded)
        songs.append(
            Song(
                file=src.name,
                title=meta.get("ti") or src.stem,
                artist=meta.get("ar") or "五月天",
                album=meta.get("al") or "",
                year=meta.get("year") or "",
                line_count=line_count,
                synthetic_timing=synthetic_timing,
                offset=pack_offset,
                length=len(encoded),
            )
        )
        pack_offset += len(encoded)

    ordered_chars = sorted(charset, key=ord)
    font_bytes = build_font_bytes(
        ordered_chars,
        font_path=font_path,
        font_index=font_index,
        font_px=args.font_px,
        threshold=args.threshold,
        stroke_px=stroke_px,
    )

    (output_dir / "font.bin").write_bytes(font_bytes)
    (output_dir / "charset.txt").write_text("".join(ordered_chars), encoding="utf-8")
    pack_path.write_bytes(b"".join(pack_chunks))
    write_index(output_dir / "index.json", songs)
    write_firmware_index(output_dir / "index.txt", songs)
    write_summary(
        output_dir / "summary.json",
        songs=songs,
        charset=ordered_chars,
        font_bytes=font_bytes,
        lrc_bytes=pack_offset,
        font_path=font_path,
        font_index=font_index,
        threshold=args.threshold,
        stroke_px=stroke_px,
    )

    print(
        f"Built {len(songs)} songs, {len(ordered_chars)} glyphs, "
        f"{len(font_bytes)} font bytes into {output_dir}"
    )


def find_font() -> Path | None:
    for path in DEFAULT_FONT_CANDIDATES:
        if path.exists():
            return path
    return None


def default_font_index(font_path: Path) -> int:
    if font_path.name == "Hiragino Sans GB.ttc":
        return 2
    return 0


def parse_meta(text: str) -> dict[str, str]:
    meta: dict[str, str] = {}
    for key, value in TAG_RE.findall(text):
        meta[key.lower()] = value.strip()
    return meta


def make_playable_lrc(text: str) -> tuple[str, bool]:
    if timed_line_texts(text):
        return text, False

    plain_lines = []
    meta_lines = []
    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if TAG_RE.fullmatch(stripped):
            meta_lines.append(stripped)
        content = re.sub(r"\[[^\]]+\]", "", raw_line).strip()
        if content:
            plain_lines.append(content)

    if not plain_lines:
        return text, False

    lines = meta_lines + ["[by:Mayday.Blue / MayScreen synthetic timing]", ""]
    for index, content in enumerate(plain_lines):
        lines.append(f"[{format_lrc_timestamp(index * 3500)}]{content}")
    return "\n".join(lines) + "\n", True


def format_lrc_timestamp(ms: int) -> str:
    minutes = ms // 60000
    seconds = (ms % 60000) // 1000
    centiseconds = (ms % 1000) // 10
    return f"{minutes:02d}:{seconds:02d}.{centiseconds:02d}"


def count_timed_lines(text: str) -> int:
    return sum(1 for line_text in timed_line_texts(text) if line_text)


def timed_line_texts(text: str) -> list[str]:
    lines: list[str] = []
    for raw_line in text.splitlines():
        if not TIME_RE.search(raw_line):
            continue
        content = re.sub(r"\[[^\]]+\]", "", raw_line).strip()
        if content:
            lines.append(content)
    return lines


def build_font_bytes(
    chars: list[str],
    font_path: Path,
    font_index: int,
    font_px: int,
    threshold: int,
    stroke_px: int,
) -> bytes:
    chunks: list[bytes] = []
    for ch in chars:
        width = glyph_width(ch)
        chunks.append(
            render_glyph(ch, width, font_path, font_index, font_px, threshold, stroke_px)
        )
    return b"".join(chunks)


def glyph_width(ch: str) -> int:
    return 16 if is_han(ord(ch)) else 8


def is_han(codepoint: int) -> bool:
    return (
        0x3400 <= codepoint <= 0x4DBF
        or 0x4E00 <= codepoint <= 0x9FFF
        or 0xF900 <= codepoint <= 0xFAFF
        or 0x20000 <= codepoint <= 0x2A6DF
        or 0x2A700 <= codepoint <= 0x2B73F
        or 0x2B740 <= codepoint <= 0x2B81F
        or 0x2B820 <= codepoint <= 0x2CEAF
        or 0x2CEB0 <= codepoint <= 0x2EBEF
        or 0x30000 <= codepoint <= 0x3134F
        or 0x31350 <= codepoint <= 0x323AF
    )


def render_glyph(
    ch: str,
    width: int,
    font_path: Path,
    font_index: int,
    font_px: int,
    threshold: int,
    stroke_px: int,
) -> bytes:
    height = 16
    scale = 4
    image = Image.new("L", (width * scale, height * scale), 0)
    draw = ImageDraw.Draw(image)
    px = min(font_px, 14) if width == 8 else font_px
    font = ImageFont.truetype(str(font_path), px * scale, index=font_index)

    if ch != " ":
        bbox = draw.textbbox((0, 0), ch, font=font)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]
        x = (image.width - text_w) / 2 - bbox[0]
        y = (image.height - text_h) / 2 - bbox[1]
        draw.text((x, y), ch, fill=255, font=font)

    raw = image.load()
    bitmap = [[False for _ in range(width)] for _ in range(height)]
    for y in range(height):
        for x in range(width):
            total = 0
            for sy in range(scale):
                for sx in range(scale):
                    total += raw[x * scale + sx, y * scale + sy]
            alpha = total / (scale * scale)
            if alpha >= threshold:
                bitmap[y][x] = True

    if ch != " " and stroke_px > 0:
        bitmap = dilate_bitmap(bitmap, width, height, stroke_px)

    return pack_bitmap(bitmap, width, height)


def dilate_bitmap(
    bitmap: list[list[bool]],
    width: int,
    height: int,
    stroke_px: int,
) -> list[list[bool]]:
    radius = max(0, stroke_px)
    out = [[False for _ in range(width)] for _ in range(height)]
    for y in range(height):
        for x in range(width):
            if not bitmap[y][x]:
                continue
            for dy in range(-radius, radius + 1):
                ny = y + dy
                if ny < 0 or ny >= height:
                    continue
                for dx in range(-radius, radius + 1):
                    nx = x + dx
                    if nx < 0 or nx >= width:
                        continue
                    out[ny][nx] = True
    return out


def pack_bitmap(bitmap: list[list[bool]], width: int, height: int) -> bytes:
    out = bytearray(width * height // 8)
    for y in range(height):
        for x in range(width):
            if bitmap[y][x]:
                bit_index = y * width + x
                out[bit_index >> 3] |= 0x80 >> (bit_index & 7)
    return bytes(out)


def write_index(path: Path, songs: list[Song]) -> None:
    payload = [
        {
            "file": song.file,
            "title": song.title,
            "artist": song.artist,
            "album": song.album,
            "year": song.year,
            "lineCount": song.line_count,
            "syntheticTiming": song.synthetic_timing,
            "offset": song.offset,
            "length": song.length,
        }
        for song in sorted(songs, key=lambda item: (item.title, item.file))
    ]
    path.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")


def write_firmware_index(path: Path, songs: list[Song]) -> None:
    lines = []
    for song in sorted(songs, key=lambda item: item.file):
        title = song.title.replace("\t", " ")
        lines.append(f"{song.file}\t{song.offset}\t{song.length}\t{title}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_summary(
    path: Path,
    songs: list[Song],
    charset: list[str],
    font_bytes: bytes,
    lrc_bytes: int,
    font_path: Path,
    font_index: int,
    threshold: int,
    stroke_px: int,
) -> None:
    han_count = sum(1 for ch in charset if glyph_width(ch) == 16)
    half_count = len(charset) - han_count
    payload = {
        "songCount": len(songs),
        "glyphCount": len(charset),
        "hanGlyphCount": han_count,
        "halfGlyphCount": half_count,
        "fontBytes": len(font_bytes),
        "lrcBytes": lrc_bytes,
        "charsetBytes": len("".join(charset).encode("utf-8")),
        "syntheticTimingSongCount": sum(1 for song in songs if song.synthetic_timing),
        "fontPath": str(font_path),
        "fontIndex": font_index,
        "threshold": threshold,
        "strokePx": stroke_px,
    }
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
