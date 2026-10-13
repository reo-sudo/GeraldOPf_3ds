#!/usr/bin/env python3
"""Add colorful genetic frog sprites and three rare adult forms to the uploaded
spritesheet. Keeps source sprites intact. Run after tools/make_art.py."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent.parent
path=root/'gfx/sprites.png'
im=Image.open(path).convert('RGBA')
assert im.size==(512,512)
# Rows 232/264/296: three inherited adult colors.
# Rows 328/360/392: Royal, Starlit, Moss rare frogs.
variants=[
 (232, (75,158,222), (118,194,248), (48,116,186)),
 (264, (174,130,217), (214,180,246), (120,79,175)),
 (296, (225,179,62), (252,224,113), (163,117,48)),
 (328, (246,198,68), (255,237,116), (174,124,48)),
 (360, (153,112,218), (218,174,250), (93,74,154)),
 (392, (60,139,92), (136,208,112), (31,97,72)),
]
base=im.crop((0,96,9*32,128))
old={(92,190,86):(0,), (58,140,70):(2,), (150,226,120):(1,),
     (224,244,168):(1,), (166,196,86):(0,), (126,150,70):(2,),
     (235,235,170):(1,)}
for y,main,light,dark in variants:
    replica=base.copy()
    pix=replica.load()
    for py in range(32):
        for px in range(9*32):
            r,g,b,a=pix[px,py]
            if a==0:continue
            target=old.get((r,g,b))
            if target is not None:
                choice=[main,light,dark][target[0]]
                pix[px,py]=(*choice,a)
    # Add an unobtrusive gold crown above Royal frog pixels,
    # tiny cosmic light above Starlit, and leaf for Moss.
    if y>=328:
        for frame in range(9):
            px=frame*32
            if y==328:
                for xx,yy in [(13,0),(17,0),(21,0),(14,1),(16,1),(18,1),(20,1),(14,2),(15,2),(16,2),(17,2),(18,2),(19,2),(20,2)]:
                    pix[px+xx,yy]=(255,218,69,255)
            if y==360:
                pix[px+15,0]=(245,242,255,255)
                pix[px+14,1]=(245,242,255,255)
                pix[px+16,1]=(245,242,255,255)
            if y==392:
                pix[px+12,1]=(92,220,128,255)
                pix[px+13,0]=(92,220,128,255)
    im.alpha_composite(replica,(0,y))
im.save(path)
print('Extended genetic frog sprites at y=232,264,296,328,360,392')
