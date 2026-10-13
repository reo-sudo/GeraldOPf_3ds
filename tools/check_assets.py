#!/usr/bin/env python3
"""Dependency-free source asset preflight, independent of devkitARM."""
from pathlib import Path
from struct import unpack
import wave
root=Path(__file__).resolve().parent.parent
required={'gfx/sprites.png':(512,512),'gfx/bg.png':(256,512),'gfx/ui.png':(512,256),
          'banner/icon.png':(48,48),'banner/banner.png':(256,128)}
for fn,expected in required.items():
    data=(root/fn).read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n',f'{fn}: invalid PNG'
    assert unpack('>II',data[16:24])==expected,f'{fn}: incorrect dimensions'
for fn in ('sprites','bg','ui'):
    desc=(root/'gfx'/f'{fn}.t3s').read_text()
    assert f'{fn}.png' in desc, f'{fn}.t3s missing file reference'
    assert '-f rgba8888' in desc, f'{fn}.t3s unexpected pixel format'
with wave.open(str(root/'banner/banner.wav'),'rb') as w:
    assert w.getnchannels()==1 and w.getsampwidth()==2, 'banner WAV must be PCM16 mono'
    assert w.getframerate()==22050 and w.getnframes()/w.getframerate()<3,'banner WAV rate/length invalid'
names='ribbit eat happy sad plop clean select back confirm tongue catch miss evolve hatch sick alert medicine lights_off lights_on gameover bgm_day bgm_night'.split()
for name in names:
    path=root/'romfs/snd'/f'{name}.raw'; sz=path.stat().st_size
    assert sz>200 and sz%2==0, f'{name}.raw missing/empty/not PCM16'
for fn in ('source/main.c','source/gerald.c','source/gerald.h','source/progression.c','source/progression.h','source/atlas.h','cia/gerald.rsf','Makefile','.github/workflows/build.yml'):
    assert (root/fn).is_file(),f'Missing: {fn}'
source=(root/'source/main.c').read_text()
for feature in ('gene_descendant','gene_passport','draw_family_page','draw_shop_page','draw_hop_page','frog_genetics'):
    assert feature in source or feature in (root/'source/progression.c').read_text(),f'Missing gameplay feature: {feature}'
print(f'PASS: {len(required)} PNGs, {len(names)} audio tracks, HOME jingle, 3 T3S files and generation systems')
