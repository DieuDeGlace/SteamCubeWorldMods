#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
namespace fieldboss {
constexpr unsigned Named=1u<<9,Friendly=1u<<15,Mini=1u<<17,Boss=1u<<18;
inline uint64_t hash(int x,int y,uint64_t seed,unsigned slot){uint64_t h=seed^(uint64_t)(int64_t)x*0x9e3779b97f4a7c15ull^(uint64_t)(int64_t)y*0xbf58476d1ce4e5b9ull^(uint64_t)slot*0x94d049bb133111ebull;h=(h^(h>>30))*0xbf58476d1ce4e5b9ull;h=(h^(h>>27))*0x94d049bb133111ebull;return h^(h>>31);}
struct Site{int64_t x,y;};
inline Site site(int rx,int ry,uint64_t seed,unsigned slot){auto h=hash(rx,ry,seed,slot);return {(int64_t)rx*1024+128+(int)(h%768),(int64_t)ry*1024+128+(int)((h>>24)%768)};}
inline void name(char out[16],uint64_t h){const char* first[]={"Vor","Mor","Kel","Zar","Thor","Ash","Grim","Bel","Dra","Ur","Fen","Kor","Mal","Rha","Val","Skal"};const char* last[]={"gath","drak","thorn","vorn","mire","roth","gorn","zul","kar","vex","nox","dral","rak","mor","bane","fang"};std::snprintf(out,16,"%s%s",first[h%16],last[(h>>16)%16]);}
inline bool eligible(int64_t id,int behaviour,unsigned flags,float hp,float maximum,bool active){return id>=1000&&behaviour==1&&!(flags&(Named|Boss|Mini|Friendly))&&!active&&std::isfinite(hp)&&std::isfinite(maximum)&&maximum>0&&hp>=maximum*.95f;}
inline unsigned bossFlags(unsigned flags){return (flags&~(Friendly|Mini))|Named|Boss;}
inline bool withinRadius(Site a,Site b,double radius){long double dx=(long double)a.x-b.x,dy=(long double)a.y-b.y;return dx*dx+dy*dy<=radius*radius;}
}
