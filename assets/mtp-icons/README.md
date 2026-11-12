# Phone icon source assets

Three colors share the reviewed geometry, textured rear, vertical telephoto
opening and overlapping front/rear layout. Small compositions crop the upper
rear. Front SVGs embed the actual TWRP home screenshot (twrp-home.png), with
6-unit screen corners. This screenshot contains no credentials or filenames.
SONY and XPERIA wordmarks are third-party assets; see wordmark-sources.txt.
Do not describe those wordmarks or the TWRP UI as original artwork by this port.

## Rebuild

Linux requires librsvg-2.so.2, libcairo.so.2 and libgobject-2.0.so.0.
Use librsvg: ImageMagick's internal SVG renderer did not faithfully render
these gradients and patterns. Python Pillow is required for pack.py.

    python3 render.py /path/to/previews

Selected composition: upper rear below 64 px; overlapping front/rear at 64 px and above:

    python3 render.py /path/to/previews --full-from 64
    python3 pack.py /path/to/previews

Both previews and ICOs include 16,24,32,48,64,96,128,256 px. All three colors
use the same 64 px composition threshold.

Earlier Git commits contain photo-derived icons; replacing current files does
not remove historical assets before public release.
