#include "progression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#define MAGIC 0x454E4547u /* GENE */
#define VERSION 1u
const char *const gene_colors[4] = {"EMERALD","AZURE","LILAC","GOLD"};
const char *const gene_personalities[4] = {"CURIOUS","PLAYFUL","GENTLE","BRAVE"};
const char *const gene_patterns[3] = {"SPOTS","STRIPES","PLAIN"};
const char *const gene_zones[GENE_ZONES] = {"HOME POND","WATERFALL","MOSS CAVE","BLOSSOM"};
const uint32_t gene_zone_level[GENE_ZONES] = {1,3,6,9};
const char *const gene_shop_names[GENE_SHOP_ITEMS] = {"FLY SNACK","BERRY PIE","STAR CAP","SUN HAT","PINK LOTUS","MUSHROOM","DUCK BUDDY","FIREFLY JAR"};
const uint32_t gene_shop_prices[GENE_SHOP_ITEMS] = {8,19,55,80,30,42,68,85};
static uint32_t rng(GeneSave *p) { uint32_t x=p->seed?p->seed:0x9E3779B9u; x^=x<<13;x^=x>>17;x^=x<<5; return p->seed=x; }
static float cap(float v) {return v<0?0:(v>100?100:v);}
static uint32_t hash(const GeneSave *p) {const unsigned char *b=(const unsigned char *)p;uint32_t h=2166136261u;for(size_t i=0;i<offsetof(GeneSave,checksum);i++){h^=b[i];h*=16777619u;}return h;}
void gene_new(GeneSave *p,uint32_t seed){memset(p,0,sizeof *p);p->magic=MAGIC;p->version=VERSION;p->generation=1;p->level=1;p->coins=75;p->color=0;p->pattern=1;p->personality=0;p->luck=35;p->visited=1;p->seed=seed?seed:12345;}
uint32_t gene_need_xp(const GeneSave *p){return 30+p->level*18;}
static bool sane(const GeneSave *p){return p->magic==MAGIC&&p->version==VERSION&&p->generation>0&&p->generation<100000&&p->level>=1&&p->level<=99&&p->coins<=999999&&p->color<4&&p->pattern<3&&p->personality<4&&p->zone<GENE_ZONES&&p->history_count<=GENE_HISTORY&&p->checksum==hash(p);}
bool gene_save(const GeneSave *p,const char *path){char tmp[320], bak[320];snprintf(tmp,sizeof tmp,"%s.tmp",path);snprintf(bak,sizeof bak,"%s.bak",path);GeneSave copy=*p;copy.checksum=hash(&copy);FILE *f=fopen(tmp,"wb");if(!f)return false;bool ok=fwrite(&copy,sizeof copy,1,f)==1;if(fclose(f))ok=false;if(!ok){remove(tmp);return false;}remove(bak);rename(path,bak);if(rename(tmp,path)==0){remove(bak);return true;}rename(bak,path);return false;}
static bool read_save(GeneSave *p,const char *path){FILE *f=fopen(path,"rb");if(!f)return false;GeneSave t;bool ok=fread(&t,sizeof t,1,f)==1 && fgetc(f)==EOF;fclose(f);if(!ok||!sane(&t))return false;*p=t;return true;}
bool gene_load(GeneSave *p,const char *path){if(read_save(p,path))return true;char bak[320];snprintf(bak,sizeof bak,"%s.bak",path);return read_save(p,bak);}
int gene_gain(GeneSave *p,int xp,int coins,const Gerald *pet){if(xp<0)xp=0;if(coins<0)coins=0;if(p->coins+(uint32_t)coins>999999)p->coins=999999;else p->coins+=(uint32_t)coins;if(p->level>=99){p->xp=0;return 0;}p->xp+=(uint32_t)xp;int levels=0;while(p->level<99&&p->xp>=gene_need_xp(p)){p->xp-=gene_need_xp(p);p->level++;levels++;p->coins+=15;}if(p->level==99)p->xp=0;if(p->coins>999999)p->coins=999999;if(pet&&pet->stage>=G_FROG&&p->level>=15&&!p->rare){p->rare=1+(rng(p)%3);}return levels;}
int gene_eggcare(GeneSave *p,Gerald *pet){if(pet->stage!=G_EGG)return 0;pet->egg_time+=9.0;p->care_count++;gene_gain(p,4,1,pet);return pet->egg_time>=45.0?1:0;}
bool gene_buy(GeneSave *p,Gerald *pet,int item){if(item<0||item>=GENE_SHOP_ITEMS||p->coins<gene_shop_prices[item])return false;uint32_t flag=1u<<item;if(item>=2&&(p->owned&flag))return false;if(item<2 &&(!pet||pet->stage==G_EGG||pet->fainted||pet->sleeping))return false;p->coins-=gene_shop_prices[item];if(item==0){pet->food=cap(pet->food+28);}else if(item==1){pet->food=cap(pet->food+55);pet->joy=cap(pet->joy+8);}else {p->owned|=flag;gene_equip(p,item);}gene_gain(p,5,0,pet);return true;}
bool gene_equip(GeneSave *p,int item){if(item<2||item>=GENE_SHOP_ITEMS||!(p->owned&(1u<<item)))return false;if(item==2||item==3)p->equip_hat=(p->equip_hat==(uint32_t)(item+1)?0:(uint32_t)item+1);else if(item==6)p->equip_buddy=(p->equip_buddy==(uint32_t)(item+1)?0:(uint32_t)item+1);else p->equip_decor=(p->equip_decor==(uint32_t)(item+1)?0:(uint32_t)item+1);return true;}
bool gene_explore(GeneSave *p,int zone,int *earned){if(zone<0||zone>=GENE_ZONES||p->level<gene_zone_level[zone])return false;time_t now=time(NULL);if(p->last_explore_at && now>=(time_t)p->last_explore_at && now-(time_t)p->last_explore_at<20)return false;p->last_explore_at=(uint32_t)now;int first=(p->visited&(1u<<zone))==0;int coins=6+zone*5+(first?18:0);p->zone=(uint32_t)zone;p->visited|=1u<<zone;gene_gain(p,8+zone*4,coins,NULL);if(earned)*earned=coins;return true;}
bool gene_descendant(GeneSave *p,Gerald *pet,int64_t now){if(!pet||pet->stage<G_FROG||p->level<10||p->coins<100||p->generation>=99999)return false;Ancestor a={p->generation,p->level,p->color,p->pattern,p->personality,p->rare};if(p->history_count==GENE_HISTORY){memmove(&p->family[0],&p->family[1],sizeof(Ancestor)*(GENE_HISTORY-1));p->history_count--;}p->family[p->history_count++]=a;p->generation++;p->coins-=100;p->level=1;p->xp=0;p->rare=0;p->care_count=0;p->zone=0;p->visited=1;p->color=(rng(p)%5==0)?rng(p)%4:a.color;p->pattern=(rng(p)%4==0)?rng(p)%3:a.pattern;p->personality=(rng(p)%4==0)?rng(p)%4:a.personality;p->luck=25+(rng(p)%55);g_new(pet,now);return true;}
uint32_t gene_passport(const GeneSave *p){uint32_t n=10000+((p->generation*1321u+p->color*961u+p->personality*97u+p->pattern*211u+p->seed%20000u)%90000u);return n*10+(n%7);}
bool gene_visit(GeneSave *p,uint32_t code){uint32_t body=code/10;if(body<10000||body>99999||body%7!=code%10||code==gene_passport(p)||code==p->last_guest)return false;p->last_guest=code;p->friends++;gene_gain(p,5,20,NULL);return true;}

/* One-time importer for the earlier Gerald: Generations save.bin.
   Do not overwrite that file: only import it if neither merged save exists. */
typedef struct {uint32_t generation,level,form,color,personality;} OldAncestor;
typedef struct {
    uint32_t magic,version;
    float hunger,happy,clean,energy;
    uint32_t savedAt,age,coins,owned,hat,deco,companion,fliesCaught,hopScore;
    uint32_t level,xp,stage,hatch,form,generation,color,pattern,personality;
    uint32_t curiosity,luck,careCount,lastExplore,areaVisited,historyCount;
    uint32_t guestPassport;
    OldAncestor family[GENE_HISTORY];
} OldGeneSave;
bool gene_import_previous(GeneSave *p,Gerald *pet,const char *filename,int64_t now){
    FILE *f=fopen(filename,"rb");if(!f)return false;
    OldGeneSave o; bool ok=fread(&o,sizeof o,1,f)==1 && fgetc(f)==EOF;
    fclose(f);
    if(!ok||o.magic!=0x47524C44u||o.version!=3||o.stage>4||o.level<1||o.level>99||o.generation<1||o.generation>99999||o.historyCount>GENE_HISTORY)return false;
    gene_new(p,(uint32_t)now);
    g_new(pet,now);
    p->generation=o.generation;p->level=o.level;p->xp=o.xp;p->coins=o.coins>999999?999999:o.coins;
    p->color=o.color%4;p->pattern=o.pattern%3;p->personality=o.personality%4;
    p->rare=o.stage==4?1+o.form%3:0;
    p->owned=o.owned<<2; /* earlier item bit0 = Star Cap, now bit2 */
    p->equip_hat=o.hat>0&&o.hat<=2?o.hat+2:0;
    p->equip_decor=o.deco==1?5:(o.deco==2?6:(o.deco==3?8:0));
    p->equip_buddy=o.companion==1?7:0;
    p->care_count=o.careCount;p->luck=o.luck<=100?o.luck:30;
    p->visited=o.areaVisited|1u;
    p->history_count=o.historyCount;
    p->best_hop=o.hopScore;
    for(uint32_t i=0;i<o.historyCount;i++){
        p->family[i]=(Ancestor){o.family[i].generation,o.family[i].level,o.family[i].color%4,1,o.family[i].personality%4,o.family[i].form};
    }
    pet->food=cap(o.hunger);pet->joy=cap(o.happy);pet->clean=cap(o.clean);
    pet->energy=cap(o.energy);
    pet->age=o.age;
    pet->egg_time=(double)o.hatch*0.45;
    pet->stage=o.stage==0?G_EGG:(o.stage==1?G_TADPOLE:(o.stage==2?G_FROGLET:G_FROG));
    return true;
}
