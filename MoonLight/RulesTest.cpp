#include "MoonRules.h"
#include <cassert>
#include <cstdio>
int main(){
 for(float hour:{8.f,12.f,16.f,18.f,6.f})assert(moonrules::state(hour*3600000).weight==0);
 for(float hour:{0.f,2.f,4.f,20.f,22.f})assert(moonrules::state(hour*3600000).weight==1);
 assert(moonrules::state(19*3600000.f).weight==.5f);
 assert(moonrules::state(5*3600000.f).weight==.5f);
 for(int minute=0;minute<1440;minute++){auto a=moonrules::state(minute*60000.f),b=moonrules::state((minute+1)*60000.f);assert(std::abs(a.weight-b.weight)<.013);float n=0;for(float v:a.direction)n+=v*v;assert(std::abs(n-1)<1e-5);if(a.weight>0)assert(a.direction[2]>=0);}
 float native[]={.1,.1,.1,1,.1,.1,.1,1,.6,0,-.8,0},out[12];auto night=moonrules::state(0);moonrules::lighting(out,native,night,.08f);
 for(int i=0;i<3;i++)assert(out[i]>native[i]&&out[i]-native[i]<=.041f);
 assert(out[3]==native[3]&&out[7]==native[7]&&out[11]==native[11]);assert(out[10]>.99);
 moonrules::lighting(out,native,moonrules::state(12*3600000.f),.08f);for(int i=0;i<12;i++)assert(out[i]==native[i]);
 float cloud[]={1,1,1,.7f},tinted[4];moonrules::cloud(tinted,cloud,1,.55f);assert(tinted[0]<.55&&tinted[2]==.55f&&tinted[3]==cloud[3]);
 moonrules::cloud(tinted,cloud,0,.55f);for(int i=0;i<4;i++)assert(tinted[i]==cloud[i]);
 puts("Subtle brightness limits, unchanged daytime/alpha, normalized moon direction, smooth dawn/dusk and night-only cloud dimming: PASS");
}
