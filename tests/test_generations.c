#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "progression.h"
int main(void){
 Gerald g;GeneSave p,q;g_new(&g,1700);gene_new(&p,123456);
 assert(g.stage==G_EGG&&p.generation==1&&p.level==1&&p.coins==75);
 for(int i=0;i<5;i++)gene_eggcare(&p,&g);
 assert(g.egg_time>=45);assert(g_step(&g,.1,1.0)&EV_HATCHED);
 assert(g.stage==G_TADPOLE);
 assert(gene_buy(&p,&g,0));
 assert(!gene_buy(&p,&g,7));
 gene_gain(&p,4000,250,&g);
 assert(p.level>=10);
 assert(gene_explore(&p,2,NULL));assert(p.zone==2);
 assert(!gene_explore(&p,3,NULL)); /* travel cooldown */
 assert(gene_buy(&p,&g,2));assert(p.equip_hat==3);
 uint32_t original_coins=p.coins;
 assert(gene_equip(&p,2)&&p.equip_hat==0);
 assert(p.coins==original_coins);
 assert(g_step(&g,24*3600,1.0)&EV_EVOLVED);
 assert(g.stage>=G_FROG);
 gene_gain(&p,500,0,&g);assert(p.rare>0);
 assert(gene_passport(&p)>=100000);
 assert(!gene_visit(&p,gene_passport(&p)));
 assert(gene_descendant(&p,&g,2000));
 assert(p.generation==2&&p.history_count==1&&p.family[0].generation==1);
 assert(g.stage==G_EGG&&p.level==1&&p.rare==0);
 assert(gene_save(&p,"/tmp/gerald_generations_test.dat"));
 memset(&q,0,sizeof q);
 assert(gene_load(&q,"/tmp/gerald_generations_test.dat"));
 assert(q.generation==2&&q.history_count==1&&q.coins==p.coins);
 assert(gene_visit(&q,123456)==(12345%7==6)); /* valid structure only */
 /* Import an exact v3-format legacy Generations save.bin. */
    typedef struct {uint32_t generation,level,form,color,personality;} OGAncestor;
    typedef struct {
      uint32_t magic,version;float hunger,happy,clean,energy;
      uint32_t savedAt,age,coins,owned,hat,deco,companion,fliesCaught,hopScore;
      uint32_t level,xp,stage,hatch,form,generation,color,pattern,personality;
      uint32_t curiosity,luck,careCount,lastExplore,areaVisited,historyCount;
      uint32_t guestPassport;OGAncestor family[GENE_HISTORY];
    } Legacy;
    Legacy old={0};old.magic=0x47524C44u;old.version=3;old.hunger=50;old.happy=60;old.clean=70;old.energy=80;
    old.stage=4;old.hatch=100;old.age=200000;old.level=15;old.coins=450;old.generation=3;old.color=2;old.owned=1;
    old.historyCount=1;old.family[0]=(OGAncestor){2,18,0,1,2};
    FILE *legacy=fopen("/tmp/gerald_generations_legacy.bin","wb");assert(legacy);
    assert(fwrite(&old,sizeof old,1,legacy)==1);fclose(legacy);
    assert(gene_import_previous(&q,&g,"/tmp/gerald_generations_legacy.bin",200000));
    assert(q.coins==450&&q.generation==3&&q.level==15&&q.color==2&&q.owned==4);
    assert(q.history_count==1&&g.stage==G_FROG&&g.food==50);
    puts("PASS: eggs, evolution, XP, rare forms, shops, equipping, exploration, family generations and saves");
 return 0;
}
