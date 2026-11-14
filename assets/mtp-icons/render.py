# SPDX-FileCopyrightText: 2026 Justin Lin and contributors
# SPDX-License-Identifier: GPL-3.0-or-later

"""Render a 488x1054 SVG preview with system librsvg and Cairo on Linux."""
import ctypes as c
import sys
from pathlib import Path
import argparse
import xml.etree.ElementTree as ET

rsvg = c.CDLL('librsvg-2.so.2')
cairo = c.CDLL('libcairo.so.2')
gobject = c.CDLL('libgobject-2.0.so.0')
rsvg.rsvg_handle_new_from_file.argtypes = [c.c_char_p, c.c_void_p]
rsvg.rsvg_handle_new_from_file.restype = c.c_void_p
rsvg.rsvg_handle_render_cairo.argtypes = [c.c_void_p, c.c_void_p]
rsvg.rsvg_handle_render_cairo.restype = c.c_int
cairo.cairo_image_surface_create.argtypes = [c.c_int, c.c_int, c.c_int]
cairo.cairo_image_surface_create.restype = c.c_void_p
cairo.cairo_create.argtypes = [c.c_void_p]
cairo.cairo_create.restype = c.c_void_p
cairo.cairo_surface_write_to_png.argtypes = [c.c_void_p, c.c_char_p]
cairo.cairo_destroy.argtypes = [c.c_void_p]
cairo.cairo_surface_destroy.argtypes = [c.c_void_p]
gobject.g_object_unref.argtypes = [c.c_void_p]

ap = argparse.ArgumentParser()
ap.add_argument('output', type=Path)
ap.add_argument('--full-from', type=int, choices=(16,24,32,48,64,96,128,256))
args = ap.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parent
cairo.cairo_scale.argtypes = [c.c_void_p,c.c_double,c.c_double]
cairo.cairo_translate.argtypes = [c.c_void_p,c.c_double,c.c_double]
for color in ('black','green','purple'):
    for design in ('small','large'):
        path = root / f'{color}-{design}.svg'
        attrs = ET.parse(path).getroot().attrib
        width,height = float(attrs['width']),float(attrs['height'])
        handle = rsvg.rsvg_handle_new_from_file(str(path).encode(),None)
        assert handle
        try:
            for size in (16,24,32,48,64,96,128,256):
                surface = cairo.cairo_image_surface_create(0,size,size)
                ctx = cairo.cairo_create(surface)
                try:
                    scale = (size-2)/max(width,height)
                    cairo.cairo_translate(ctx,(size-width*scale)/2,(size-height*scale)/2)
                    cairo.cairo_scale(ctx,scale,scale)
                    assert rsvg.rsvg_handle_render_cairo(handle,ctx)
                    output = args.output / f'{color}-{design}-{size}.png'
                    assert cairo.cairo_surface_write_to_png(surface,str(output).encode()) == 0
                    if args.full_from is not None and design == ('large' if size>=args.full_from else 'small'):
                        (args.output/f'{color}-{size}.png').write_bytes(output.read_bytes())
                finally:
                    cairo.cairo_destroy(ctx)
                    cairo.cairo_surface_destroy(surface)
        finally:
            gobject.g_object_unref(handle)
print('Rendered 48 previews; ICOs and recipe unchanged.')
