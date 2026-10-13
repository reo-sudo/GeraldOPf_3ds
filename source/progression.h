/* Gerald Generations expansion: level, genetics, family, zones, shop.
 * Independent of the 3DS system APIs so it can be tested on a desktop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gerald.h"
#define GENE_HISTORY 12
#define GENE_SHOP_ITEMS 8
#define GENE_ZONES 4

typedef struct {
    uint32_t generation, level, color, pattern, personality, rare;
} Ancestor;
typedef struct {
    uint32_t magic, version;
    uint32_t generation, level, xp, coins;
    uint32_t color, pattern, personality, rare;
    uint32_t owned, equip_hat, equip_decor, equip_buddy;
    uint32_t zone, visited, care_count, luck, best_hop;
    uint32_t last_guest, friends, history_count, last_explore_at;
    uint32_t seed;
    Ancestor family[GENE_HISTORY];
    uint32_t checksum;
} GeneSave;

extern const char *const gene_colors[4];
extern const char *const gene_personalities[4];
extern const char *const gene_patterns[3];
extern const char *const gene_zones[GENE_ZONES];
extern const uint32_t gene_zone_level[GENE_ZONES];
extern const char *const gene_shop_names[GENE_SHOP_ITEMS];
extern const uint32_t gene_shop_prices[GENE_SHOP_ITEMS];

void gene_new(GeneSave *p, uint32_t seed);
bool gene_load(GeneSave *p, const char *path);
bool gene_save(const GeneSave *p, const char *path);
uint32_t gene_need_xp(const GeneSave *p);
int gene_gain(GeneSave *p, int xp, int coins, const Gerald *pet); /* levels earned */
int gene_eggcare(GeneSave *p, Gerald *pet);
bool gene_buy(GeneSave *p, Gerald *pet, int item);
bool gene_equip(GeneSave *p, int item);
bool gene_explore(GeneSave *p, int zone, int *earned);
bool gene_descendant(GeneSave *p, Gerald *pet, int64_t now);
uint32_t gene_passport(const GeneSave *p);
bool gene_visit(GeneSave *p, uint32_t code);
/* Migrates old v3 save.bin into current pet & lineage; preserves source file. */
bool gene_import_previous(GeneSave *p,Gerald *pet,const char *filename,int64_t now);
