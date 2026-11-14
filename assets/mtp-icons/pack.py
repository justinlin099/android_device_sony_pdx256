# SPDX-FileCopyrightText: 2026 Justin Lin and contributors
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pack rendered PNG frames and update recipe hashes; requires Pillow."""
import hashlib
import json
from pathlib import Path
import struct
import sys
from PIL import Image

tree = Path(__file__).resolve().parents[2]
pngs = Path(sys.argv[1]).resolve()
recipe_path = tree / 'ramdisk-recipe.json'
recipe = json.loads(recipe_path.read_text())
for color, filename in [('black', 'device.ico'), ('green', 'device-pdx256-187.ico'),
                        ('purple', 'device-pdx256-188.ico')]:
    frames = []
    for size in (16, 24, 32, 48, 64, 96, 128, 256):
        path = pngs / f'{color}-{size}.png'
        with Image.open(path) as im:
            assert im.size == (size, size)
        frames.append((size, path.read_bytes()))
    offset = 6 + 16 * len(frames)
    directory, payload = bytearray(), bytearray()
    for size, data in frames:
        directory.extend(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0,
                                     1, 32, len(data), offset))
        payload.extend(data)
        offset += len(data)
    data = struct.pack('<HHH', 0, 1, len(frames)) + directory + payload
    target = tree / 'ramdisk-overlay/twres' / filename
    with Image.open(__import__('io').BytesIO(data)) as ico:
        assert ico.ico.sizes() == {(n, n) for n in (16, 24, 32, 48, 64, 96, 128, 256)}
        for size, _ in frames:
            ico.ico.getimage((size, size)).load()
    target.write_bytes(data)
    matches = [entry for entry in recipe if entry['name'] == f'twres/{filename}']
    assert len(matches) == 1
    matches[0]['reference_sha256'] = hashlib.sha256(data).hexdigest()
recipe_path.write_text(json.dumps(recipe, indent=2) + '\n', encoding='utf-8')
print('Updated three ICO assets and recipe hashes.')
