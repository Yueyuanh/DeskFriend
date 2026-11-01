"""Create matching MCU/browser TrueType fonts. Run with fonttools installed.

Uses the same locally installed font families that the original page selected
on Windows. Generated assets are build inputs; this script is not run by PIO.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.recordingPen import DecomposingRecordingPen

out = ROOT / 'assets' / 'fonts'
out.mkdir(parents=True, exist_ok=True)
chars = set(range(32, 127))
for a in range(0xA1, 0xF8):
    for b in range(0xA1, 0xFF):
        try:
            chars.update(map(ord, bytes((a, b)).decode('gb2312')))
        except UnicodeDecodeError:
            pass
chars.update(map(ord, (ROOT / 'DeskFriend_index.html').read_text(encoding='utf-8')))
chars.update(map(ord, '…✓♪♫❤'))
for name, source, selected in [
    ('ui-bold', 'msyhbd.ttc', chars),
    ('ui-regular', 'msyh.ttc', set(range(32, 127)) | set(map(ord, '夜深了，早点休息该吃饭啦早上好下午晚上'))),
    ('clock', 'ARLRDBD.TTF', set(map(ord, '0123456789:'))),
]:
    font_path = Path('C:/Windows/Fonts') / source
    if not font_path.is_file():
        raise SystemExit(f'Missing local font: {font_path}. See assets/fonts/README.md.')
    font = TTFont(font_path, fontNumber=0)
    # YaHei has no musical notes/check mark; bake browser fallback outlines into
    # the SAME font so LCD and browser never select different symbol families.
    if name == 'ui-bold':
        symbols = TTFont('C:/Windows/Fonts/seguisym.ttf')
        source_map = symbols.getBestCmap()
        glyph_set = symbols.getGlyphSet()
        order = list(font.getGlyphOrder())
        for code in (0x2713, 0x266a, 0x266b, 0x2665):
            if code in font.getBestCmap() or code not in source_map:
                continue
            src_name = source_map[code]
            name_out = 'deskSymbol%04X' % code
            recording = DecomposingRecordingPen(glyph_set)
            glyph_set[src_name].draw(recording)
            pen = TTGlyphPen(None)
            recording.replay(pen)
            font['glyf'][name_out] = pen.glyph()
            font['hmtx'][name_out] = symbols['hmtx'][src_name]
            if 'vmtx' in font:
                font['vmtx'][name_out] = (font['head'].unitsPerEm, 0)
            order.append(name_out)
            for table in font['cmap'].tables:
                if table.isUnicode(): table.cmap[code] = name_out
        font.setGlyphOrder(order)
    options = subset.Options()
    options.hinting = False
    options.layout_features = ['kern']
    sub = subset.Subsetter(options=options)
    sub.populate(unicodes=selected)
    sub.subset(font)
    target = out / (name + '.ttf')
    font.save(target)
    print(name, target.stat().st_size, 'bytes;', len(font.getBestCmap()), 'characters')
