# Gerald the Frog — Generations Fusion 🐸

Nintendo 3DS / 2DS virtual pet game in C, combining **the user-supplied pixel-art Gerald engine** (the detailed `sprites.png`, `bg.png`, and wooden `ui.png` texture sheets, all 22 `.raw` chiptune tracks, and the HOME Menu banner) with an expanded persistent Generations simulation.

## What is implemented

* **Real egg and hatch stage:** Gerald begins as an egg. Wait about 45 seconds or nurture it five times (each nurture advances the real pet egg timer).
* **Growth:** egg, tadpole, froglet, frog and elder, using uploaded animated sprite rows. Ages carry on gently while the game is closed, and Gerald never dies.
* **Genetic frogs:** at adult age, descendants display inherited Azure, Lilac, Gold or Emerald body variants. Level 15+ can unlock one of three new visible special forms: Royal, Starlit or Moss.
* **XP to level 99** and coins from care, feeding, collecting flies, Lily Hop and exploration.
* **Shop:** eight items (snacks, pie, hats, flower, mushroom, duck buddy, firefly jar). Purchased accessories and coins persist and visually decorate Gerald's pond. The lower-screen DECOR tab lets you equip them.
* **Four pond destinations:** level-gated Home Pond (1), Waterfall (3), Moss Cave (6), Blossom (9). Each trip gives XP and coins with a 20-second reward cooldown. The selected destination is saved and adds its own pixel effects to your supplied day/night pond artwork.
* **Family tree:** an adult frog at level 10+ with 100 coins can hatch a descendant. The current genetic profile, generation counter and the last 12 ancestors are stored on SD. Money and unlocked items persist across descendants.
* **Frog passports:** offline 6-digit numeric codes, entered with the 3DS software keyboard. These are simple visitor/friend codes, not Wi-Fi, QR scanning or multiplayer networking.
* **Actual menus:** five touch tabs (Care, Explore, Shop, Family, Decor), textured cards and color-coded stats; two 20-second minigames (Fly Catcher and Lily Hop).
* **Music/SFX:** uploaded day and night chiptune tracks, event effects and a custom HOME Menu banner and jingle.

## Important controls

* **L/R** move between the five wood tabs, or tap them.
* **Touch** buttons in the selected page, or use **D-Pad left/right + A** to activate positions 1–6.
* **X** opens Gerald's profile. **B** goes back to Care / leaves a minigame.
* Care buttons: **FEED, PLAY, CLEAN, SLEEP, MEDS, INFO**. While Gerald is an egg, care buttons nurture the egg.
* Explore: four areas, then Fly Catcher and Lily Hop at the bottom. Fly Catcher uses touch; Lily Hop uses touch or D-pad left/right + A.
* Family: New Gen, Import Code, Tree navigation. New Gen requires adult, level 10 and 100 coins.
* **SELECT** toggles background music; **START** saves and exits.

## Save data and compatibility

The source pet engine saves to `sdmc:/3ds/gerald/save.dat`. Generations metadata saves separately to `sdmc:/3ds/gerald/generations.dat` so older pet data is not discarded. If neither file is present, the importer can read older Gerald Generations v3 `sdmc:/3ds/gerald/save.bin`, retaining the old file. Make an SD backup before trying a new release. Save migration and keeping two save files do not protect against every possible interruption or hardware failure.

## GitHub Actions CIA build

1. Upload **everything inside this folder** into the root of your GitHub repo, preserving `.github/workflows/build.yml` (the `.github` folder is hidden on some file managers).
2. Go to **Actions → Build Gerald Generations Fusion CIA → Run workflow**. Pushing also triggers a run.
3. After a *successful* build download the **Gerald-Generations-Fusion-3DS** artifact, containing `gerald.cia`, `gerald.3dsx`, icon, banner and checksums.
4. Copy/install on an already homebrew-enabled 3DS using your normal homebrew installer, or open the 3DSX in Homebrew Launcher.

The workflow uses the pinned devkitPro ARM container, checks files and gameplay, installs makerom/bannertool, converts the `.t3s` texture sheets to `.t3x`, and generates both outputs. CIA construction or installation **has not yet been verified on real 3DS hardware**.

### Local logic checks

```
cc -std=gnu11 -Wall -Wextra -Werror -Isource tests/test_gerald.c source/gerald.c -o /tmp/gerald_pet_test
/tmp/gerald_pet_test
cc -std=gnu11 -Wall -Wextra -Werror -Isource tests/test_generations.c source/progression.c source/gerald.c -o /tmp/gerald_lineage_test
/tmp/gerald_lineage_test
python3 tools/check_assets.py
```

Files are already generated; you do **not** need to run the art generator before building. To edit art from source scripts (requires Pillow and numpy), run `python3 tools/make_art.py`. The script invokes `tools/extend_gene_art.py` to restore the extra genetic variants after generating the base atlas. Run `python3 tools/make_sound.py` to recreate original audio, if you want to modify it.

## Project layout

| Folder | Purpose |
| --- | --- |
| `source/main.c` | citro2d screen renderer, touch input, sounds, minigames |
| `source/gerald.c/.h` | User's original tested pet/needs simulation |
| `source/progression.c/.h` | XP, genetics, shop, exploration, lineage, saves/import |
| `source/atlas.h` | Uploaded pixel-sprite atlas definitions |
| `gfx/` | Uploaded sprites, day/night background, wooden UI + `.t3s` |
| `romfs/snd/` | Uploaded 22 PCM16 sound files |
| `banner/` | Uploaded 3DS icon, banner and short WAV jingle |
| `cia/` | Home Menu makerom metadata |
| `tools/` | CIA tools installer, sprite/sound generators, asset checks |
| `tests/` | Desktop pet simulation + family progression tests |
| `.github/workflows/build.yml` | Complete GitHub Actions build workflow |

*This is a tested source release, not a precompiled CIA. GitHub compilation and testing on hardware are the remaining steps.*
