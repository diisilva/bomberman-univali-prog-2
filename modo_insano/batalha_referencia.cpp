#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>
using namespace Gdiplus;
constexpr int W=15,H=11,T=46,OX=38,OY=158;
const std::wstring root=L"D:\\algoritmos_prog_2\\bomberman-univali-prog-2\\assets\\";
std::unique_ptr<Bitmap> people,enemy,fx,boss,stage,stage2,items,fire;
std::unique_ptr<Bitmap> lobby,dungeon,minigame,marioSheet,appleTexture;
std::array<std::array<std::unique_ptr<Bitmap>,3>,4> marioFrames;
std::array<std::unique_ptr<Bitmap>,4> dragonFrames;
std::array<std::array<std::unique_ptr<Bitmap>,3>,4> minionFrames;
constexpr int themeCount=8;
const wchar_t* themeNames[]={L"FABRICA NEON",L"CASINO AZUL",L"RUINAS DE PEDRA",L"CANION CEEDRUN",L"MASMORRA",L"SELVA VIVA",L"LABORATORIO",L"TEMPLO ARCANO"};
std::array<std::array<std::array<std::unique_ptr<Bitmap>,3>,4>,4> characterFrames;
// Compose the entire client area off screen; present it in one BitBlt.
HDC backDC=nullptr;
HBITMAP backBitmap=nullptr;
HGDIOBJ backOriginal=nullptr;
int backWidth=0,backHeight=0;
void releaseBackBuffer(){
    if(backDC){SelectObject(backDC,backOriginal);DeleteObject(backBitmap);DeleteDC(backDC);}
    backDC=nullptr;backBitmap=nullptr;backOriginal=nullptr;backWidth=backHeight=0;
}
bool ensureBackBuffer(HDC dc,int width,int height){
    if(backDC&&width==backWidth&&height==backHeight)return true;
    releaseBackBuffer();
    backDC=CreateCompatibleDC(dc);if(!backDC)return false;
    backBitmap=CreateCompatibleBitmap(dc,width,height);
    if(!backBitmap){DeleteDC(backDC);backDC=nullptr;return false;}
    backOriginal=SelectObject(backDC,backBitmap);backWidth=width;backHeight=height;return true;
}
std::mt19937 rng(42);
int board[H][W],loot[H][W],theme=0,score=0,combo=0;
float clockTime=0,bossHP=100,bossCooldown=5,bossHitTimer=0;
enum class MatchState { Playing,Victory,Defeat };
MatchState matchState=MatchState::Playing;
struct BossMotion {int x=6,y=1,targetX=6,targetY=1;float px=6,py=1,wait=4,damageWait=0;bool moving=false,horizontal=true;} bossMotion;
constexpr float bossMoveInterval=4;
bool paused=false,manual=false,slow=false,keys[256]{};
bool controlMario=false;
constexpr float moveSpeed=4.2f;
struct Actor { int x,y,dir=0,hp=3,kills=0,range=2; float px,py,think=0,inv=0,bombCD=1; int moveDir=-1;float walkDistance=0;bool walking=false; };
std::array<unsigned long long,256> keyOrder{};
unsigned long long inputOrder=0;
int bufferedDir=-1;
float inputBuffer=0;
double simulationRemainder=0;
using SteadyClock=std::chrono::steady_clock;
SteadyClock::time_point lastTick;
std::array<Actor,4> actors;
Actor mario;
float appleCooldown=0,throwPose=0;
int applesThrown=0,appleHits=0;
struct Apple { float sx,sy,tx,ty,age=0,duration=1; };
std::vector<Apple> apples;
struct Dragon { int x=7,y=7,hp=3;float px=7,py=7,think=0,inv=0,spit=5; } dragon;
struct Minion:Actor {float wait=0;bool dead=false;};
std::array<Minion,2> minions;
struct Bomb { int x,y,owner,range; float fuse; };
struct Flame { int x,y; float life; };
struct Particle { float x,y,vx,vy,life,total; Color color; };
struct Shot { float x,y,vx,vy,life; };
std::vector<Bomb> bombs; std::vector<Flame> flames; std::vector<Particle> particles;
std::vector<Shot> shots; std::deque<std::wstring> feed;
const int dx[]={0,-1,0,1},dy[]={1,0,-1,0};
bool walkable(int x,int y);
int keyDirection(int key){
    switch(key){case 'S':case VK_DOWN:return 0;case 'A':case VK_LEFT:return 1;case 'W':case VK_UP:return 2;case 'D':case VK_RIGHT:return 3;default:return -1;}
}
void pressKey(int key){
    if(key<0||key>=256)return;
    if(!keys[key]){keyOrder[key]=++inputOrder;int dir=keyDirection(key);if(dir>=0){bufferedDir=dir;inputBuffer=.16f;}}
    keys[key]=true;
}
void clearInput(){std::fill(std::begin(keys),std::end(keys),false);bufferedDir=-1;inputBuffer=0;}
int requestedDirection(){
    int result=-1;unsigned long long newest=0;
    constexpr int movementKeys[]={'S',VK_DOWN,'A',VK_LEFT,'W',VK_UP,'D',VK_RIGHT};
    for(int key:movementKeys)if(keys[key]&&(result<0||keyOrder[key]>=newest)){result=keyDirection(key);newest=keyOrder[key];}
    return result>=0?result:(inputBuffer>0?bufferedDir:-1);
}
bool startMove(Actor&a,int dir){
    if(dir<0||a.moveDir>=0)return false;
    a.dir=dir;
    if(!walkable(a.x+dx[dir],a.y+dy[dir]))return false;
    a.x+=dx[dir];a.y+=dy[dir];a.moveDir=dir;return true;
}
float advanceActor(Actor&a,float dt){
    if(a.moveDir<0)return dt;
    float distance=std::hypot(a.x-a.px,a.y-a.py),step=std::min(distance,moveSpeed*dt);
    if(distance>0){a.px+=(a.x-a.px)*step/distance;a.py+=(a.y-a.py)*step/distance;}
    a.walkDistance+=step;a.walking=step>0;
    if(distance-step<.0001f){a.px=(float)a.x;a.py=(float)a.y;a.moveDir=-1;return std::max(0.f,dt-step/moveSpeed);}
    return 0;
}
void moveManually(Actor&a,float dt){
    a.walking=false;int wanted=requestedDirection();
    if(a.moveDir>=0&&wanted==(a.moveDir+2)%4){
        int previousX=a.x-dx[a.moveDir],previousY=a.y-dy[a.moveDir];
        if(walkable(previousX,previousY)){a.x=previousX;a.y=previousY;a.moveDir=wanted;a.dir=wanted;}
    }
    for(int n=0;n<4&&dt>0;++n){
        if(a.moveDir<0){if(!startMove(a,wanted))break;inputBuffer=0;}
        dt=advanceActor(a,dt);
    }
}
int walkFrame(const Actor&a){
    constexpr int cycle[]={0,1,0,2};return a.walking?cycle[(int)(a.walkDistance*2.5f)%4]:1;
}
const wchar_t* names[]={L"WHITE",L"SHADOW",L"AZURE",L"EMBER"};
Color colors[]={Color(226,239,255),Color(204,163,255),Color(75,203,255),Color(255,170,64)};
float random01(){return (rng()%10000)/10000.f;}
void log(const std::wstring&s){feed.push_front(s);if(feed.size()>5)feed.pop_back();}
bool inside(int x,int y){return x>0&&y>0&&x<W-1&&y<H-1;}
bool bossTile(int x,int y){return bossHP>0&&x+1>bossMotion.px+.001f&&x<bossMotion.px+3-.001f&&y+1>bossMotion.py+.001f&&y<bossMotion.py+2-.001f;}
bool bossBlocksTile(int x,int y){
    if(bossTile(x,y))return true;
    if(!bossMotion.moving||bossHP<=0)return false;
    return x>=std::min(bossMotion.x,bossMotion.targetX)&&x<std::max(bossMotion.x,bossMotion.targetX)+3&&
           y>=std::min(bossMotion.y,bossMotion.targetY)&&y<std::max(bossMotion.y,bossMotion.targetY)+2;
}
bool bombAt(int x,int y){for(auto&b:bombs)if(b.x==x&&b.y==y)return true;return false;}
bool walkable(int x,int y){return inside(x,y)&&board[y][x]==0&&!bombAt(x,y)&&!bossBlocksTile(x,y);}
void burst(float x,float y,Color c,int n=22){for(int i=0;i<n;++i){float a=random01()*6.283f,s=30+random01()*145,l=.3f+random01()*.6f;particles.push_back({x,y,std::cos(a)*s,std::sin(a)*s-35,l,l,c});}}
void reset(){
    rng.seed(42); clockTime=0;bossHP=100;bossCooldown=5;bossHitTimer=0;score=combo=0;
    matchState=MatchState::Playing;bossMotion=BossMotion{};paused=false;
    simulationRemainder=0;clearInput();
    bombs.clear();flames.clear();particles.clear();shots.clear();feed.clear();
    apples.clear();applesThrown=appleHits=0;appleCooldown=1;throwPose=0;
    mario=Actor{};mario.x=7;mario.y=9;mario.px=7;mario.py=9;mario.hp=3;mario.inv=2;
    dragon=Dragon{};
    for(int i=0;i<2;++i){auto&m=minions[i];m=Minion{};m.x=7;m.y=i?9:5;m.px=7;m.py=(float)m.y;m.wait=i*.3f;}
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){board[y][x]=(!inside(x,y)||(x%2==0&&y%2==0))?1:((rng()%100<43)?2:0);loot[y][x]=0;}
    const int sx[]={1,13,1,13},sy[]={1,9,9,1};
    for(int i=0;i<4;++i){actors[i]=Actor{};auto&a=actors[i];a.x=sx[i];a.y=sy[i];a.px=(float)a.x;a.py=(float)a.y;a.think=i*.09f;
        for(int y=1;y<H-1;++y)for(int x=1;x<W-1;++x)if(std::abs(x-a.x)+std::abs(y-a.y)<=2&&board[y][x]!=1)board[y][x]=0;
    }
    for(int y=1;y<=2;++y)for(int x=6;x<=8;++x)board[y][x]=0;
    // Keep Mario's entry and the dragon's nest connected to the arena.
    for(int y=7;y<=9;++y)for(int x=6;x<=8;++x)if(board[y][x]!=1)board[y][x]=0;
    log(L"A arena esta aberta!");log(L"Destrua caixas. Enfrente o chefe.");
    log(L"Mario chegou com uma cesta de macas!");
}
bool danger(int x,int y){
    for(auto&f:flames)if(f.x==x&&f.y==y)return true;
    for(auto&b:bombs){if(b.x==x&&b.y==y)return true;
        for(int d=0;d<4;++d)for(int k=1;k<=b.range;++k){int xx=b.x+dx[d]*k,yy=b.y+dy[d]*k;if(!inside(xx,yy)||board[yy][xx]==1)break;
            if(xx==x&&yy==y)return true;
            if(board[yy][xx]==2||bossTile(xx,yy))break;}}
    return false;
}
void place(int i){auto&a=actors[i];if(a.hp<=0||a.bombCD>0||bombAt(a.x,a.y))return;
    int count=0;for(auto&b:bombs)if(b.owner==i)++count;if(count>=2)return;
    bombs.push_back({a.x,a.y,i,a.range,2.5f});a.bombCD=1.8f;burst(OX+(a.x+.5f)*T,OY+(a.y+.5f)*T,colors[i],5);
}
void eliminateFighter(int i,int owner){
    auto&a=actors[i];if(a.hp<=0)return;
    a.hp=0;a.walking=false;a.moveDir=-1;
    burst(OX+(a.px+.5f)*T,OY+(a.py+.5f)*T,colors[i]);score+=250;
    if(owner==4)++mario.kills;else if(owner>=0&&owner<4&&owner!=i)++actors[owner].kills;
    log(std::wstring(names[i])+L" eliminado definitivamente!");
}
void hurt(int i,int owner){auto&a=actors[i];if(matchState!=MatchState::Playing||a.hp<=0||a.inv>0)return;
    if(a.hp==1){eliminateFighter(i,owner);return;}a.hp--;a.inv=1.6f;
    burst(OX+(a.px+.5f)*T,OY+(a.py+.5f)*T,colors[i]);
}
void hurtMario(int owner){
    if(matchState!=MatchState::Playing||mario.hp<=0||mario.inv>0)return;
    --mario.hp;mario.inv=1.6f;burst(OX+(mario.px+.5f)*T,OY+(mario.py+.5f)*T,Color(255,82,78));
    if(mario.hp==0){matchState=MatchState::Defeat;clearInput();log(L"Mario foi derrotado. Deseja voltar?");if(owner>=0&&owner<4)actors[owner].kills++;}
}
void hurtDragon(){
    if(matchState!=MatchState::Playing||dragon.hp<=0||dragon.inv>0)return;
    --dragon.hp;dragon.inv=1.3f;burst(OX+(dragon.px+.5f)*T,OY+(dragon.py+.5f)*T,Color(255,158,196));
    if(dragon.hp==0){score+=300;log(L"Ceedrun eliminado definitivamente! +300");}
}
void damageBoss(float damage){
    if(matchState!=MatchState::Playing||bossHP<=0)return;
    bossHP=std::max(0.f,bossHP-damage);score+=50;
    bossHitTimer=.5f;
    bossMotion.damageWait=.7f;
    if(bossHP==0){matchState=MatchState::Victory;clearInput();log(L"MARIO VENCEU O CHEFE! +2000");score+=2000;burst(OX+(bossMotion.px+1.5f)*T,OY+(bossMotion.py+1)*T,Color(115,245,222),130);}
}
bool canMoveBoss(int targetX,int targetY){
    if(targetX<1||targetY<1||targetX+3>W-1||targetY+2>H-1)return false;
    int left=std::min(bossMotion.x,targetX),top=std::min(bossMotion.y,targetY);
    int right=std::max(bossMotion.x,targetX)+3,bottom=std::max(bossMotion.y,targetY)+2;
    auto overlaps=[&](float px,float py){return px+.85f>left&&px+.15f<right&&py+.85f>top&&py+.15f<bottom;};
    auto fighterBlocks=[&](const Actor&a){return a.hp>0&&(overlaps(a.px,a.py)||overlaps((float)a.x,(float)a.y));};
    if(fighterBlocks(mario))return false;
    for(auto&a:actors)if(fighterBlocks(a))return false;
    for(auto&m:minions)if(!m.dead&&fighterBlocks(m))return false;
    if(dragon.hp>0&&(overlaps(dragon.px,dragon.py)||overlaps((float)dragon.x,(float)dragon.y)))return false;
    for(auto&b:bombs)if(b.x>=left&&b.x<right&&b.y>=top&&b.y<bottom)return false;
    return true;
}
bool startBossMove(int dir,int distance){
    int x=bossMotion.x+dx[dir]*distance,y=bossMotion.y+dy[dir]*distance;
    if(!canMoveBoss(x,y))return false;
    // The large boss opens a passage through interior obstacles; border walls remain intact.
    for(int yy=std::min(bossMotion.y,y);yy<std::max(bossMotion.y,y)+2;++yy)
        for(int xx=std::min(bossMotion.x,x);xx<std::max(bossMotion.x,x)+3;++xx){
            if(board[yy][xx])burst(OX+(xx+.5f)*T,OY+(yy+.5f)*T,Color(178,152,116),6);
            board[yy][xx]=0;loot[yy][xx]=0;
        }
    bossMotion.targetX=x;bossMotion.targetY=y;bossMotion.moving=true;
    bossMotion.horizontal=!bossMotion.horizontal;log(L"Chefe avancando! Saia do caminho.");return true;
}
void updateBoss(float dt){
    if(bossHP<=0||matchState!=MatchState::Playing)return;
    bossMotion.damageWait=std::max(0.f,bossMotion.damageWait-dt);
    if(bossMotion.moving){float distance=std::hypot(bossMotion.targetX-bossMotion.px,bossMotion.targetY-bossMotion.py);
        float step=std::min(distance,dt*1.5f);
        if(distance>0){bossMotion.px+=(bossMotion.targetX-bossMotion.px)*step/distance;bossMotion.py+=(bossMotion.targetY-bossMotion.py)*step/distance;}
        if(distance-step<.0001f){bossMotion.x=bossMotion.targetX;bossMotion.y=bossMotion.targetY;bossMotion.px=(float)bossMotion.x;bossMotion.py=(float)bossMotion.y;bossMotion.moving=false;bossMotion.wait=bossMoveInterval;}
    }else{bossMotion.wait-=dt;if(bossMotion.wait<=0){
        int preferred=bossMotion.horizontal?(rng()%2?1:3):(rng()%2?0:2);
        int otherAxis=bossMotion.horizontal?0:1;
        bool moved=false;
        for(int distance:{2,1}){for(int dir:{preferred,(preferred+2)%4,otherAxis,(otherAxis+2)%4})if(startBossMove(dir,distance)){moved=true;break;}if(moved)break;}
        if(!moved)bossMotion.wait=.75f;
    }}
    if(bossMotion.damageWait<=0)for(auto&f:flames)if(f.life>0&&bossTile(f.x,f.y)){damageBoss(4);break;}
}
void explode(Bomb b){
    ++combo;score+=30;
    auto add=[&](int x,int y){flames.push_back({x,y,.68f});burst(OX+(x+.5f)*T,OY+(y+.5f)*T,Color(255,177,52),10);
        for(auto&other:bombs)if(other.x==x&&other.y==y)other.fuse=std::min(other.fuse,.02f);
        for(int i=0;i<4;++i)if(actors[i].x==x&&actors[i].y==y)hurt(i,b.owner);
        if(mario.x==x&&mario.y==y)hurtMario(b.owner);
        if(dragon.x==x&&dragon.y==y)hurtDragon();
        for(auto&m:minions)if(!m.dead&&m.x==x&&m.y==y){m.dead=true;score+=150;log(L"Sentinela eliminada! +150");}
        if(bossTile(x,y))damageBoss(4);
    };
    add(b.x,b.y);
    for(int d=0;d<4;++d)for(int k=1;k<=b.range;++k){int x=b.x+dx[d]*k,y=b.y+dy[d]*k;
        if(!inside(x,y)||board[y][x]==1)break;
        bool box=board[y][x]==2,hitBoss=bossTile(x,y);add(x,y);
        if(hitBoss)break;
        if(box){board[y][x]=0;loot[y][x]=rng()%3==0?1+rng()%2:0;score+=10;break;}}
}
int escapeDirection(Actor&a){
    struct Node{int x,y,first;};std::deque<Node> q;bool seen[H][W]{};seen[a.y][a.x]=true;q.push_back({a.x,a.y,-1});
    while(!q.empty()){auto n=q.front();q.pop_front();if(n.first>=0&&!danger(n.x,n.y))return n.first;
        int offset=rng()%4;for(int k=0;k<4;++k){int d=(offset+k)%4,x=n.x+dx[d],y=n.y+dy[d];if(walkable(x,y)&&!seen[y][x]){seen[y][x]=true;q.push_back({x,y,n.first<0?d:n.first});}}}
    return -1;
}
void throwApple(){
    if(mario.hp<=0||appleCooldown>0)return;
    float tx=mario.px+dx[mario.dir]*3+.5f,ty=mario.py+dy[mario.dir]*3+.5f;
    if(!(manual&&controlMario)){
        float nearest=1e9f;
        for(auto&a:actors)if(a.hp>0){float d=std::hypot(a.px-mario.px,a.py-mario.py);
            if(d<nearest){nearest=d;tx=a.x+.5f;ty=a.y+.5f;}}
        if(bossHP>0&&std::hypot(mario.px-(bossMotion.px+1),mario.py-bossMotion.py)<nearest){tx=bossMotion.px+1.5f;ty=bossMotion.py+1;}
        if(dragon.hp>0&&std::hypot(mario.px-dragon.px,mario.py-dragon.py)<nearest*.7f){tx=dragon.x+.5f;ty=dragon.y+.5f;}
    }
    tx=std::clamp(tx,1.5f,W-1.5f);ty=std::clamp(ty,1.5f,H-1.5f);
    float sx=mario.px+.5f,sy=mario.py+.5f;
    apples.push_back({sx,sy,tx,ty,0,std::clamp(std::hypot(tx-sx,ty-sy)/5,.65f,1.45f)});
    appleCooldown=1.05f;throwPose=.28f;++applesThrown;
}
void updateGuests(float dt){
    if(matchState!=MatchState::Playing)return;
    mario.inv=std::max(0.f,mario.inv-dt);appleCooldown-=dt;throwPose=std::max(0.f,throwPose-dt);
    if(mario.hp>0){
        mario.walking=false;
        const bool controlled=manual&&controlMario;
        if(controlled)moveManually(mario,dt);else advanceActor(mario,dt);
        for(auto&f:flames)if(f.x==mario.x&&f.y==mario.y)hurtMario(-1);
        if(matchState!=MatchState::Playing)return;
        if(loot[mario.y][mario.x]){loot[mario.y][mario.x]=0;score+=100;appleCooldown=0;log(L"Mario coletou uma maca extra!");}
        mario.think-=dt;
        if(!controlled&&mario.moveDir<0&&mario.think<=0){mario.think=.04f;int dir=-1;
            if(danger(mario.x,mario.y))dir=escapeDirection(mario);
            else{float best=-1e9f;for(int d=0;d<4;++d){int x=mario.x+dx[d],y=mario.y+dy[d];if(!walkable(x,y)||danger(x,y))continue;
                float value=random01()*3+(d==mario.dir?.5f:0);if(loot[y][x])value+=5;
                if(bossHP>0)value-=.25f*(std::abs(x-bossMotion.px-1)+std::abs(y-bossMotion.py));
                for(auto&a:actors)if(a.hp>0){float dist=std::hypot((float)(x-a.x),(float)(y-a.y));if(dist<2)value-=2;else if(dist<5)value+=.3f;}
                if(value>best){best=value;dir=d;}}}
            if(dir>=0)startMove(mario,dir);
        }
        if(!(manual&&controlMario)||keys[VK_SPACE])throwApple();
    }
    dragon.inv=std::max(0.f,dragon.inv-dt);
    if(dragon.hp>0){
        dragon.px+=(dragon.x-dragon.px)*std::min(1.f,dt*8);dragon.py+=(dragon.y-dragon.py)*std::min(1.f,dt*8);
        for(auto&f:flames)if(f.x==dragon.x&&f.y==dragon.y)hurtDragon();
        dragon.think-=dt;if(dragon.think<=0){dragon.think=.55f;int offset=rng()%4;for(int k=0;k<4;++k){int d=(offset+k)%4,x=dragon.x+dx[d],y=dragon.y+dy[d];if(walkable(x,y)&&!danger(x,y)){dragon.x=x;dragon.y=y;break;}}}
        dragon.spit-=dt;if(dragon.spit<=0){dragon.spit=3.7f;float vx=mario.px-dragon.px,vy=mario.py-dragon.py,len=std::max(.1f,std::hypot(vx,vy));shots.push_back({dragon.px+.5f,dragon.py+.5f,vx/len*2.8f,vy/len*2.8f,4});}
    }
    for(auto&a:apples){a.age+=dt;if(a.age<a.duration)continue;
        int x=(int)a.tx,y=(int)a.ty;bool hit=false;
        if(board[y][x]==2){board[y][x]=0;loot[y][x]=rng()%3==0?1:0;score+=10;hit=true;}
        for(int i=0;i<4;++i)if(actors[i].hp>0&&std::hypot(a.tx-(actors[i].px+.5f),a.ty-(actors[i].py+.5f))<.95f){eliminateFighter(i,4);hit=true;}
        for(auto&m:minions)if(!m.dead&&std::hypot(a.tx-(m.px+.5f),a.ty-(m.py+.5f))<.9f){m.dead=true;score+=150;log(L"Sentinela eliminada pela maca!");hit=true;}
        if(dragon.hp>0&&std::hypot(a.tx-(dragon.px+.5f),a.ty-(dragon.py+.5f))<1){dragon.inv=0;dragon.hp=1;hurtDragon();hit=true;}
        for(auto&b:bombs)if(b.x==x&&b.y==y){b.fuse=0;hit=true;}
        if(bossTile(x,y)){damageBoss(3);hit=true;}
        burst(OX+a.tx*T,OY+a.ty*T,Color(255,80,66),hit?20:8);
        if(hit){++appleHits;score+=25;if(appleHits%5==1)log(L"Mario acertou uma macada!");}
        if(matchState!=MatchState::Playing)break;
    }
    apples.erase(std::remove_if(apples.begin(),apples.end(),[](auto&a){return a.age>=a.duration;}),apples.end());
}
void update(float dt){
    if(matchState!=MatchState::Playing)return;
    clockTime+=dt;
    bossHitTimer=std::max(0.f,bossHitTimer-dt);
    inputBuffer=std::max(0.f,inputBuffer-dt);
    updateBoss(dt);
    if(matchState!=MatchState::Playing)return;
    for(auto&f:flames){f.life-=dt;} flames.erase(std::remove_if(flames.begin(),flames.end(),[](auto&f){return f.life<=0;}),flames.end());
    for(auto&p:particles){p.life-=dt;p.x+=p.vx*dt;p.y+=p.vy*dt;p.vy+=200*dt;}
    particles.erase(std::remove_if(particles.begin(),particles.end(),[](auto&p){return p.life<=0;}),particles.end());
    for(auto&b:bombs)b.fuse-=dt;
    for(size_t i=0;i<bombs.size();)if(bombs[i].fuse<=0){Bomb b=bombs[i];bombs.erase(bombs.begin()+i);explode(b);}else ++i;
    if(matchState!=MatchState::Playing)return;
    for(int i=0;i<4;++i){auto&a=actors[i];a.inv=std::max(0.f,a.inv-dt);a.bombCD-=dt;
        if(a.hp<=0)continue;
        a.walking=false;
        const bool controlled=manual&&!controlMario&&i==0;
        if(controlled)moveManually(a,dt);else advanceActor(a,dt);
        for(auto&f:flames)if(f.x==a.x&&f.y==a.y)hurt(i,-1);
        if(loot[a.y][a.x]){int item=loot[a.y][a.x];loot[a.y][a.x]=0;if(item==1)a.range=std::min(5,a.range+1);else a.hp=std::min(3,a.hp+1);score+=100;log(std::wstring(names[i])+L" coletou uma melhoria.");burst(OX+(a.x+.5f)*T,OY+(a.y+.5f)*T,Color(100,255,210));}
        if(controlled){if(keys[VK_SPACE])place(i);continue;}
        a.think-=dt;if(a.think>0||a.moveDir>=0)continue;a.think=.04f;int dir=-1;
        if(danger(a.x,a.y))dir=escapeDirection(a);
        else {
            bool target=false;for(int d=0;d<4;++d){int x=a.x+dx[d],y=a.y+dy[d];if(inside(x,y)&&board[y][x]==2)target=true;}
            for(int j=0;j<4;++j)if(j!=i&&actors[j].hp>0&&std::abs(a.x-actors[j].x)+std::abs(a.y-actors[j].y)<=2)target=true;
            if(mario.hp>0&&std::abs(a.x-mario.x)+std::abs(a.y-mario.y)<=2)target=true;
            if(bossHP>0&&std::abs(a.x-bossMotion.px-1)<=2&&std::abs(a.y-bossMotion.py)<=3)target=true;
            if(target&&a.bombCD<=0){place(i);dir=escapeDirection(a);}else{
                float best=-1e9f;for(int d=0;d<4;++d){int x=a.x+dx[d],y=a.y+dy[d];if(!walkable(x,y)||danger(x,y))continue;
                    float value=random01()*3+(d==a.dir?.6f:0);if(loot[y][x])value+=8;
                    if(bossHP>0)value-=.12f*(std::abs(x-bossMotion.px-1)+std::abs(y-bossMotion.py));
                    for(int k=0;k<4;++k){int xx=x+dx[k],yy=y+dy[k];if(inside(xx,yy)&&board[yy][xx]==2)value+=.4f;}
                    if(value>best){best=value;dir=d;}}
            }
        }
        if(dir>=0)startMove(a,dir);
    }
    if(bossHP>0){bossCooldown-=dt;if(bossCooldown<=0){bossCooldown=4.5f;log(L"Chefe: chuva de fogo!");
        for(int i=0;i<5;++i){auto&a=i==4?mario:actors[i];if(a.hp<=0)continue;float x=bossMotion.px+1.5f,y=bossMotion.py+.8f,vx=a.px+.5f-x,vy=a.py+.5f-y,len=std::max(.1f,std::hypot(vx,vy));shots.push_back({x,y,vx/len*3.2f,vy/len*3.2f,4});}}
    }
    for(auto&s:shots){s.x+=s.vx*dt;s.y+=s.vy*dt;s.life-=dt;int x=(int)s.x,y=(int)s.y;
        if(!inside(x,y))s.life=0;else if(board[y][x]==1&&y>2)s.life=0;else if(board[y][x]==2){board[y][x]=0;s.life=0;burst(OX+s.x*T,OY+s.y*T,Color(255,120,45));}
        for(int i=0;i<4;++i)if(std::hypot(s.x-(actors[i].px+.5f),s.y-(actors[i].py+.5f))<.5f){hurt(i,-1);s.life=0;}
        if(mario.hp>0&&std::hypot(s.x-(mario.px+.5f),s.y-(mario.py+.5f))<.5f){hurtMario(-1);s.life=0;}
    }
    shots.erase(std::remove_if(shots.begin(),shots.end(),[](auto&s){return s.life<=0;}),shots.end());
    if(matchState!=MatchState::Playing)return;
    for(auto&m:minions){if(m.dead)continue;
        for(auto&f:flames)if(f.x==m.x&&f.y==m.y){m.dead=true;score+=150;}
        if(m.dead)continue;
        m.walking=false;advanceActor(m,dt*.6f);m.wait-=dt;
        if(m.wait<=0&&m.moveDir<0){m.wait=.05f;int offset=rng()%4;for(int k=0;k<4;++k){int d=(offset+k)%4;if(startMove(m,d))break;}}
        for(int i=0;i<4;++i)if(std::hypot(m.px-actors[i].px,m.py-actors[i].py)<.55f)hurt(i,-1);
        if(mario.hp>0&&std::hypot(m.px-mario.px,m.py-mario.py)<.55f)hurtMario(-1);
    }
    updateGuests(dt);
}
// Fixed simulation steps consume real elapsed time, independent of rendering cost.
void advanceRealTime(double seconds){
    if(paused||matchState!=MatchState::Playing){simulationRemainder=0;return;}
    constexpr double step=1.0/120.0;
    const double playPace=manual?(controlMario?.65:1.0):.8;
    simulationRemainder+=std::clamp(seconds,0.0,.25)*playPace*(slow?1.0/3.0:1.0);
    while(simulationRemainder+1e-10>=step){update((float)step);simulationRemainder-=step;}
}

std::unique_ptr<Bitmap> load(const wchar_t* name,bool clean=false){
    auto original=std::make_unique<Bitmap>((root+name).c_str());if(original->GetLastStatus()!=Ok)throw std::wstring(L"Falha ao abrir ")+name;
    auto result=std::unique_ptr<Bitmap>(original->Clone(0,0,original->GetWidth(),original->GetHeight(),PixelFormat32bppARGB));
    if(clean){Color key;original->GetPixel(0,40,&key);
        for(UINT y=0;y<result->GetHeight();++y)for(UINT x=0;x<result->GetWidth();++x){Color c;result->GetPixel(x,y,&c);int r=c.GetR(),g=c.GetG(),b=c.GetB();
            if(c.GetValue()==key.GetValue()||(r==26&&g==53&&b==29)||(r==63&&g==127&&b==127)||(r==31&&g==63&&b==63)||(r==64&&g==144&&b==56)||(r==40&&g==160&&b==192)||(r==23&&g==68&&b==68))result->SetPixel(x,y,Color(0,0,0,0));}}
    return result;
}
void sprite(Graphics&g,Bitmap* image,Rect src,float x,float y,float w,float h){g.DrawImage(image,RectF(x,y,w,h),(float)src.X,(float)src.Y,(float)src.Width,(float)src.Height,UnitPixel);}
void prepareCharacters(){
    // Each cell becomes its own RGBA texture. No neighboring atlas pixels can leak.
    for(int i=0;i<4;++i)for(int dir=0;dir<4;++dir)for(int step=0;step<3;++step){
        // Atlas rows are DOWN, RIGHT, UP, LEFT; world directions are DOWN, LEFT, UP, RIGHT.
        constexpr int atlasRow[]={0,3,2,1};
        int sx=(i%2?196:3)+step*17,sy=(i>=2?305:47)+atlasRow[dir]*25;
        auto cell=std::unique_ptr<Bitmap>(people->Clone(sx,sy,16,24,PixelFormat32bppARGB));
        const Color background=i==0?Color(64,144,56):Color(40,160,192);
        int visible=0;
        for(int y=0;y<24;++y)for(int x=0;x<16;++x){Color c;cell->GetPixel(x,y,&c);
            if(c.GetValue()==background.GetValue())cell->SetPixel(x,y,Color(0,0,0,0));
            else if(c.GetA()>0)++visible;
        }
        // Every animation frame must have a transparent perimeter at the corners.
        for(Point p:{Point(0,0),Point(15,0),Point(0,23),Point(15,23)}){
            Color c;cell->GetPixel(p.X,p.Y,&c);
            if(c.GetA()!=0)throw std::wstring(L"Fundo inesperado no recorte do personagem.");
        }
        if(!visible)throw std::wstring(L"Recorte de personagem vazio.");
        characterFrames[i][dir][step]=std::move(cell);
    }
}
std::unique_ptr<Bitmap> cutout(Bitmap* source,Rect region,const std::vector<Color>& backgrounds){
    auto cell=std::unique_ptr<Bitmap>(source->Clone(region.X,region.Y,region.Width,region.Height,PixelFormat32bppARGB));
    int visible=0;
    for(int y=0;y<region.Height;++y)for(int x=0;x<region.Width;++x){Color c;cell->GetPixel(x,y,&c);
        bool background=false;for(auto key:backgrounds)if(c.GetValue()==key.GetValue())background=true;
        if(background)cell->SetPixel(x,y,Color(0,0,0,0));else if(c.GetA())++visible;
    }
    if(!visible)throw std::wstring(L"Recorte de sprite vazio.");
    return cell;
}
void prepareGuests(){
    for(int dir=0;dir<4;++dir)for(int f=0;f<3;++f){
        // Top row groups are RIGHT, DOWN, LEFT; UP is on the second row.
        int x=(dir==0?60:dir==1?114:6)+f*17,y=dir==2?24:6;
        marioFrames[dir][f]=cutout(marioSheet.get(),Rect(x,y,16,16),{Color(56,135,0)});
    }
    for(int f=0;f<4;++f)dragonFrames[f]=cutout(minigame.get(),Rect(216+f*58,50,56,46),{Color(0,152,0),Color(0,136,0)});
    // Enemy frames have variable widths. Find each nontransparent column group.
    constexpr int rowTop[]={34,118,76,160},rowBottom[]={69,155,111,197};
    for(int dir=0;dir<4;++dir){int start=-1,count=0;
        for(int x=0;x<(int)enemy->GetWidth()&&count<3;++x){bool occupied=false;
            for(int y=rowTop[dir];y<rowBottom[dir]&&!occupied;++y){Color c;enemy->GetPixel(x,y,&c);occupied=c.GetA()>0;}
            if(occupied&&start<0)start=x;
            if(!occupied&&start>=0){minionFrames[dir][count++]=cutout(enemy.get(),Rect(start,rowTop[dir],x-start,rowBottom[dir]-rowTop[dir]),{});start=-1;}
        }
        if(count!=3)throw std::wstring(L"Nao foi possivel recortar as direcoes das sentinelas.");
    }
    // Small code-drawn pixel sprite: red apple, stem, leaf and a highlight.
    appleTexture=std::make_unique<Bitmap>(12,14,PixelFormat32bppARGB);
    Graphics g(appleTexture.get());g.Clear(Color(0,0,0,0));
    SolidBrush outline(Color(103,28,38)),red(Color(241,61,57)),light(Color(255,147,114)),leaf(Color(107,220,75)),stem(Color(114,73,42));
    g.FillRectangle(&stem,6,0,2,4);g.FillRectangle(&leaf,8,1,3,2);
    g.FillRectangle(&outline,2,4,8,9);g.FillRectangle(&outline,1,6,10,5);
    g.FillRectangle(&red,3,4,3,8);g.FillRectangle(&red,7,4,2,8);g.FillRectangle(&red,2,6,8,5);
    g.FillRectangle(&red,4,11,4,2);g.FillRectangle(&light,3,5,2,3);
}
struct Tiles { Bitmap* sheet;Rect floor,wall,box;Color tint; };
Tiles sourceTiles(int index){
    switch(index){
    case 1:return {stage2.get(),Rect(96,48,16,16),Rect(16,16,16,16),Rect(48,32,16,16),Color(20,5,17,23)};
    case 2:return {lobby.get(),Rect(34,30,64,64),Rect(122,24,32,32),Rect(343,403,32,24),Color(18,5,10,18)};
    case 3:return {minigame.get(),Rect(300,244,16,16),Rect(216,168,24,24),Rect(384,170,24,24),Color(15,45,24,5)};
    case 4:return {dungeon.get(),Rect(315,760,32,32),Rect(345,585,32,32),Rect(696,696,32,32),Color(24,15,9,4)};
    case 5:return {dungeon.get(),Rect(630,1900,32,32),Rect(630,1760,32,32),Rect(696,696,32,32),Color(15,2,22,4)};
    case 6:return {dungeon.get(),Rect(364,2506,32,32),Rect(625,2360,32,32),Rect(353,2410,32,32),Color(12,5,15,30)};
    case 7:return {dungeon.get(),Rect(1011,2156,24,16),Rect(350,2040,32,32),Rect(696,696,32,32),Color(12,9,5,30)};
    default:return {stage.get(),Rect(64,16,16,16),Rect(0,0,16,16),Rect(48,32,16,16),Color(35,5,17,23)};
    }
}
void rect(Graphics&g,float x,float y,float w,float h,Color c){SolidBrush b(c);g.FillRectangle(&b,x,y,w,h);}
void text(Graphics&g,const std::wstring&s,float x,float y,float size,Color c=Color(230,236,247),bool bold=false){Font f(L"Segoe UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush b(c);g.DrawString(s.c_str(),-1,&f,PointF(x,y),&b);}
std::array<std::array<std::unique_ptr<Bitmap>,3>,themeCount> arenaTiles;
std::wstring compactAtlasPath(){
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    std::wstring path=executable;auto slash=path.find_last_of(L"\\/");
    return path.substr(0,slash+1)+L"assets_otimizados\\arenas.png";
}
bool loadCompactAtlas(){
    Bitmap atlas(compactAtlasPath().c_str());
    if(atlas.GetLastStatus()!=Ok||atlas.GetWidth()!=themeCount*T||atlas.GetHeight()!=3*T)return false;
    for(int t=0;t<themeCount;++t)for(int kind=0;kind<3;++kind){
        arenaTiles[t][kind]=std::unique_ptr<Bitmap>(atlas.Clone(t*T,kind*T,T,T,PixelFormat32bppPARGB));
        if(!arenaTiles[t][kind]||arenaTiles[t][kind]->GetLastStatus()!=Ok)return false;
    }
    return true;
}
void prepareArenaTiles(){
    for(int t=0;t<themeCount;++t){const auto tiles=sourceTiles(t);
        // Clone only the tiny source regions, avoiding conversion of the whole atlas per draw.
        auto crop=[&](Rect r){return std::unique_ptr<Bitmap>(tiles.sheet->Clone(r.X,r.Y,r.Width,r.Height,PixelFormat32bppPARGB));};
        auto floor=crop(tiles.floor),wall=crop(tiles.wall),box=crop(tiles.box);
        for(int kind=0;kind<3;++kind){
            auto cell=std::make_unique<Bitmap>(T,T,PixelFormat32bppPARGB);
            {Graphics g(cell.get());g.Clear(Color(28,39,43));g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
                sprite(g,floor.get(),Rect(0,0,floor->GetWidth(),floor->GetHeight()),0,0,T,T);rect(g,0,0,T,T,tiles.tint);
                if(kind==1){sprite(g,wall.get(),Rect(0,0,wall->GetWidth(),wall->GetHeight()),0,0,T,T);rect(g,0,0,T,T,Color(25,10,24,40));}
                if(kind==2){rect(g,3,8,T-4,T-6,Color(100,0,0,0));sprite(g,box.get(),Rect(0,0,box->GetWidth(),box->GetHeight()),3,2,T-6,T-5);Pen edge(Color(85,230,196,125));g.DrawRectangle(&edge,3.f,2.f,T-6.f,T-5.f);}
            }
            arenaTiles[t][kind]=std::move(cell);
        }
    }
}
std::unique_ptr<Bitmap> terrainCache;
int cachedTheme=-1,cachedBoard[H][W]{};
void drawTerrain(Graphics&g){
    if(!terrainCache)terrainCache=std::make_unique<Bitmap>(W*T,H*T,PixelFormat32bppPARGB);
    if(cachedTheme!=theme||std::memcmp(cachedBoard,board,sizeof(board))!=0){
        Graphics terrain(terrainCache.get());terrain.SetCompositingMode(CompositingModeSourceCopy);
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            if(cachedTheme==theme&&cachedBoard[y][x]==board[y][x])continue;
            // Replace just the changed cell. Each cached tile is already opaque and 46x46.
            terrain.DrawImage(arenaTiles[theme][board[y][x]].get(),x*T,y*T);
        }
        cachedTheme=theme;std::memcpy(cachedBoard,board,sizeof(board));
    }
    g.DrawImage(terrainCache.get(),OX,OY);
}
void draw(Graphics&g){
    g.Clear(Color(11,16,28));g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
    rect(g,0,0,1080,5,Color(252,187,65));text(g,L"BOMBER / ARENA",36,24,35,Color(246,247,251),true);
    text(g,std::wstring(L"ARENA ")+std::to_wstring(theme+1)+L" / "+std::to_wstring(themeCount)+L"  -  "+themeNames[theme],40,70,13,Color(110,157,186),true);
    text(g,manual?(controlMario?L"VOCE CONTROLA MARIO":L"VOCE CONTROLA WHITE"):L"BATALHA AUTOMATICA",780,30,16,Color(98,235,199),true);
    text(g,L"ROUND 01   /   "+std::to_wstring((int)clockTime)+L"s",780,59,15,Color(144,164,190));
    if(manual&&controlMario)text(g,L"RITMO TRANQUILO / 65%",782,82,11,Color(255,169,123));
    else if(!manual)text(g,L"RITMO AUTOMATICO / 80%",782,82,11,Color(144,164,190));
    for(int i=0;i<5;++i){int x=38+i*140;Color color=i<4?colors[i]:Color(255,105,91);auto&a=i<4?actors[i]:mario;
        rect(g,(float)x,102,132,44,Color(25,34,50));rect(g,(float)x,102,3,44,color);text(g,i<4?names[i]:L"MARIO",(float)x+12,108,12,color,true);
        for(int h=0;h<3;++h){rect(g,(float)x+12+h*13,130,9,5,h<a.hp?color:Color(49,58,73));} text(g,a.hp>0?L"KO "+std::to_wstring(a.kills):L"MORTO",(float)x+80,117,11);}
    rect(g,OX-4,OY-4,W*T+8,H*T+8,Color(65,92,105));
    drawTerrain(g);
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){float xx=(float)(OX+x*T),yy=(float)(OY+y*T);
        if(loot[y][x]){float bob=std::sin(clockTime*5+x)*3;SolidBrush glow(Color(60,80,255,210));g.FillEllipse(&glow,xx+4,yy+4,(float)T-8,(float)T-8);
            sprite(g,items.get(),loot[y][x]==1?Rect(46,54,16,16):Rect(6,75,16,16),xx+10,yy+10+bob,26,26);}
    }
    for(auto&b:bombs){float x=OX+(b.x+.5f)*T,y=OY+(b.y+.5f)*T,pulse=1+std::sin(clockTime*(b.fuse<.8f?30:10))*.08f;
        SolidBrush shadow(Color(110,0,0,0));g.FillEllipse(&shadow,x-17,y+9,34.f,13.f);
        sprite(g,fx.get(),Rect(((int)(clockTime*8)%4)*16,0,16,16),x-18*pulse,y-20*pulse,36*pulse,36*pulse);
        Pen ring(b.fuse<.8f?Color(255,101,65):colors[b.owner],2);g.DrawArc(&ring,x-21,y-22,42.f,42.f,-90.f,360.f*b.fuse/2.5f);
    }
    if(bossHP>0){float x=OX+(bossMotion.px+1.5f)*T,y=OY+(bossMotion.py+.6f)*T+std::sin(clockTime*3)*5;SolidBrush shadow(Color(130,0,0,0));g.FillEllipse(&shadow,x-40,y+28,80.f,22.f);
        sprite(g,boss.get(),Rect(0,0,253,253),x-55,y-70,110,110);
        if(bossHitTimer>0){Pen hit(Color((BYTE)(bossHitTimer*400),255,177,85),2);g.DrawEllipse(&hit,x-52,y-60,104.f,100.f);text(g,L"DANO!",x-25,y-80-(.5f-bossHitTimer)*20,13,Color(255,194,98),true);}}
    for(auto&m:minions)if(!m.dead){float x=OX+(m.px+.5f)*T,y=OY+(m.py+1)*T;SolidBrush shadow(Color(100,0,0,0));g.FillEllipse(&shadow,x-17,y-7,34.f,12.f);
        auto*cell=minionFrames[m.dir][walkFrame(m)].get();float width=cell->GetWidth()*1.6f;
        sprite(g,cell,Rect(0,0,cell->GetWidth(),cell->GetHeight()),x-width/2,y-56,width,56);}
    if(dragon.hp>0){float x=OX+(dragon.px+.5f)*T,y=OY+(dragon.py+1)*T;
        SolidBrush shadow(Color(110,0,0,0));g.FillEllipse(&shadow,x-25,y-8,50.f,14.f);
        auto dragonState=g.Save();
        if(dragon.x>dragon.px+.01f){g.TranslateTransform(2*x,0);g.ScaleTransform(-1,1);}
        sprite(g,dragonFrames[(int)(clockTime*6)%4].get(),Rect(0,0,56,46),x-32,y-53,64,53);g.Restore(dragonState);
        text(g,L"CEEDRUN",x-28,y-65,9,Color(255,178,197),true);
    }
    std::array<int,4> order={0,1,2,3};std::sort(order.begin(),order.end(),[](int a,int b){return actors[a].py<actors[b].py;});
    for(int i:order){auto&a=actors[i];if(a.hp<=0)continue;
        float x=OX+(a.px+.5f)*T,y=OY+(a.py+1)*T-3;SolidBrush sh(Color(100,0,0,0));g.FillEllipse(&sh,x-17,y-8,34.f,12.f);
        Pen outline(colors[i],1.5f);g.DrawEllipse(&outline,x-20,y-8,40.f,14.f);
        if(a.inv>0){Pen shield(Color(110,155,225,255),2);g.DrawEllipse(&shield,x-23,y-58,46.f,60.f);}
        int step=walkFrame(a);
        sprite(g,characterFrames[i][a.dir][step].get(),Rect(0,0,16,24),x-20,y-55,40,60);
        text(g,std::to_wstring(i+1),x-4,y-69,11,colors[i],true);
    }
    if(mario.hp>0){float x=OX+(mario.px+.5f)*T,y=OY+(mario.py+1)*T-3;
        SolidBrush shadow(Color(100,0,0,0));g.FillEllipse(&shadow,x-19,y-8,38.f,12.f);
        if(mario.inv>0){Pen shield(Color(110,255,145,105),2);g.DrawEllipse(&shield,x-24,y-53,48.f,55.f);}
        sprite(g,marioFrames[mario.dir][walkFrame(mario)].get(),Rect(0,0,16,16),x-24,y-47,48,48);
        text(g,L"MARIO",x-22,y-63,10,Color(255,145,110),true);
        if(throwPose>0)sprite(g,appleTexture.get(),Rect(0,0,12,14),x+12,y-40,18,21);
    }
    for(auto&f:flames){float x=(float)(OX+f.x*T),y=(float)(OY+f.y*T);int row=16+((int)(clockTime*18)%4)*16;
        SolidBrush light(Color((BYTE)(f.life/.68f*55),255,163,45));g.FillEllipse(&light,x-18,y-18,T+36.f,T+36.f);
        sprite(g,fx.get(),Rect(112,row,16,16),x-3,y-3,T+6,T+6);
    }
    for(auto&s:shots){float x=OX+s.x*T,y=OY+s.y*T;SolidBrush halo(Color(65,255,115,20));g.FillEllipse(&halo,x-24,y-24,48.f,48.f);
        sprite(g,fire.get(),Rect(5+((int)(clockTime*12)%4)*40,4,31,55),x-16,y-29,32,56);}
    for(auto&a:apples){float u=std::clamp(a.age/a.duration,0.f,1.f),x=OX+(a.sx+(a.tx-a.sx)*u)*T,ground=OY+(a.sy+(a.ty-a.sy)*u)*T;
        float height=std::sin(u*3.1415926f)*T*1.7f+(1-u)*20;SolidBrush shadow(Color(80,0,0,0));g.FillEllipse(&shadow,x-9,ground-3,18.f,8.f);
        auto saved=g.Save();g.TranslateTransform(x,ground-height);g.RotateTransform(u*400);sprite(g,appleTexture.get(),Rect(0,0,12,14),-12,-14,24,28);g.Restore(saved);
        Pen target(Color(85,255,120,90),1);g.DrawEllipse(&target,OX+a.tx*T-12,OY+a.ty*T-5,24.f,10.f);
    }
    for(auto&p:particles){float alpha=std::clamp(p.life/p.total,0.f,1.f);rect(g,p.x,p.y,3+alpha*3,3+alpha*3,Color((BYTE)(alpha*255),p.color.GetR(),p.color.GetG(),p.color.GetB()));}
    rect(g,762,102,282,562,Color(19,27,42));
    text(g,L"THE STAR TYRANT",782,121,16,Color(255,197,89),true);text(g,bossHP>0?L"CHEFE / ATAQUE DE FOGO":L"CHEFE / DERROTADO",782,146,11,Color(134,151,176));
    sprite(g,minigame.get(),Rect(215,145,240,120),782,170,242,121);
    sprite(g,boss.get(),Rect(0,0,253,253),861,173,106,106);
    rect(g,784,298,238,7,Color(49,54,68));rect(g,784,298,238*bossHP/100,7,Color(254,151,64));
    text(g,std::to_wstring((int)bossHP)+L" / 100 HP",784,309,11,Color(255,194,116));
    sprite(g,marioFrames[0][(int)(clockTime*4)%3].get(),Rect(0,0,16,16),784,334,30,30);
    sprite(g,appleTexture.get(),Rect(0,0,12,14),822,336,20,24);
    text(g,L"MACADAS: "+std::to_wstring(appleHits)+L" / "+std::to_wstring(applesThrown),852,341,12,Color(255,146,124),true);
    rect(g,782,374,242,1,Color(48,61,80));text(g,L"PONTOS",782,391,11,Color(123,143,171),true);
    text(g,std::to_wstring(score),780,408,38,Color(244,248,255),true);
    text(g,L"ACONTECENDO AGORA",782,469,11,Color(98,235,199),true);
    int yy=495;for(auto&s:feed){text(g,s,782,(float)yy,11,Color(174,190,212));yy+=26;}
    text(g,L"ENTER  White / auto    M  Mario / auto    WASD  mover    ESPACO  bomba / maca",38,682,12,Color(164,185,211));
    text(g,L"T  8 arenas     P  pausa     R  reiniciar     L  camera lenta     ESC  sair",38,707,12,Color(107,132,163));
    if(paused&&matchState==MatchState::Playing){rect(g,OX,OY,W*T,H*T,Color(170,9,15,26));text(g,L"PAUSADO",OX+230,OY+210,40,Color(255,210,105),true);}
    if(matchState!=MatchState::Playing){
        bool victory=matchState==MatchState::Victory;
        rect(g,0,0,1080,744,Color(190,7,12,22));rect(g,245,220,590,280,Color(25,35,52));
        rect(g,245,220,590,4,victory?Color(91,232,181):Color(255,127,105));
        text(g,victory?L"MARIO VENCEU O CHEFE!":L"MARIO FOI DERROTADO",285,250,30,victory?Color(112,240,195):Color(255,169,136),true);
        text(g,victory?L"Todos os acertos valeram a pena!":L"Voce recebeu os tres acertos.",330,305,17);
        text(g,victory?L"Deseja jogar novamente?":L"Deseja voltar?",365,342,23,Color(235,243,255),true);
        rect(g,315,400,200,55,Color(40,145,113));rect(g,565,400,200,55,Color(79,88,105));
        text(g,L"SIM",390,413,22,Color(255,255,255),true);text(g,L"NAO",635,413,22,Color(255,255,255),true);
        text(g,L"SIM reinicia com todos os personagens. NAO fecha o jogo.",295,473,12,Color(171,190,213));
    }
}
int pngEncoder(CLSID*id){UINT count,size;GetImageEncodersSize(&count,&size);std::vector<BYTE> data(size);auto info=(ImageCodecInfo*)data.data();GetImageEncoders(count,size,info);for(UINT i=0;i<count;++i)if(wcscmp(info[i].MimeType,L"image/png")==0){*id=info[i].Clsid;return 1;}return 0;}
bool saveCompactAtlas(const wchar_t*path){
    Bitmap atlas(themeCount*T,3*T,PixelFormat32bppARGB);
    {Graphics g(&atlas);g.SetCompositingMode(CompositingModeSourceCopy);
        for(int t=0;t<themeCount;++t)for(int kind=0;kind<3;++kind)g.DrawImage(arenaTiles[t][kind].get(),t*T,kind*T);
    }
    CLSID id;return pngEncoder(&id)&&atlas.Save(path,&id,nullptr)==Ok;
}
bool benchmarkRendering(const wchar_t*path){
    auto report=_wfopen(path,L"w");if(!report)return false;
    fprintf(report,"theme,steady_ms,terrain_change_ms\n");
    Bitmap surface(1080,744,PixelFormat32bppARGB);
    for(int t=0;t<themeCount;++t){theme=t;reset();for(int i=0;i<8*120;++i)update(1.f/120);
        {Graphics g(&surface);draw(g);draw(g);}
        auto measure=[&](bool change){auto begin=SteadyClock::now();
            for(int i=0;i<12;++i){if(change)board[5][5]=i%2?2:0;Graphics g(&surface);draw(g);}
            return std::chrono::duration<double,std::milli>(SteadyClock::now()-begin).count()/12;
        };
        double steady=measure(false),changed=measure(true);
        fprintf(report,"%d,%.3f,%.3f\n",t+1,steady,changed);fflush(report);
    }
    fclose(report);return true;
}
bool saveDirectionPreview(const wchar_t*path){
    Bitmap preview(960,740,PixelFormat32bppARGB);
    {Graphics g(&preview);g.Clear(Color(17,24,37));g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
        const wchar_t*labels[]={L"BAIXO / S",L"ESQUERDA / A",L"CIMA / W",L"DIREITA / D"};
        for(int dir=0;dir<4;++dir){float x=dir*240.f;text(g,labels[dir],x+15,12,17,Color(111,238,201),true);
            for(int row=0;row<6;++row){float y=55+row*112.f;text(g,row<4?names[row]:row==4?L"MARIO":L"SENTINELA",x+15,y,12);
                for(int step=0;step<3;++step){Bitmap*cell=row<4?characterFrames[row][dir][step].get():row==4?marioFrames[dir][step].get():minionFrames[dir][step].get();
                    float h=row==4?48.f:60.f,w=cell->GetWidth()*h/cell->GetHeight();
                    sprite(g,cell,Rect(0,0,cell->GetWidth(),cell->GetHeight()),x+18+step*68,y+24,w,h);}
            }
        }
    }
    CLSID id;return pngEncoder(&id)&&preview.Save(path,&id,nullptr)==Ok;
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){case WM_TIMER:{auto now=SteadyClock::now();double elapsed=std::chrono::duration<double>(now-lastTick).count();lastTick=now;
        if(IsIconic(hwnd)){simulationRemainder=0;return 0;}
        advanceRealTime(elapsed);InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_KEYDOWN:if(wp<256)pressKey((int)wp);if(lp&(1LL<<30))return 0;
        if(wp==VK_ESCAPE){DestroyWindow(hwnd);return 0;}
        if(matchState!=MatchState::Playing&&wp!='R'&&wp!='T')return 0;
        if(wp==VK_RETURN){manual=!(manual&&!controlMario);controlMario=false;clearInput();log(manual?L"Voce controla WHITE.":L"Batalha automatica.");}
        if(wp=='M'){manual=!(manual&&controlMario);controlMario=true;clearInput();log(manual?L"Voce controla MARIO. Espaco: maca!":L"Batalha automatica.");}
        if(wp=='P')paused=!paused;
        if(wp=='R')reset();
        if(wp=='T'){theme=(theme+1)%themeCount;log(std::wstring(L"Arena: ")+themeNames[theme]);}
        if(wp=='L')slow=!slow;
        InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_KEYUP:if(wp<256)keys[wp]=false;return 0;
    case WM_LBUTTONUP:{
        if(matchState==MatchState::Playing)return 0;
        RECT r;GetClientRect(hwnd,&r);float scale=std::min(r.right/1080.f,r.bottom/744.f);
        if(scale<=0)return 0;
        float x=((short)LOWORD(lp)-(r.right-1080*scale)/2)/scale,y=((short)HIWORD(lp)-(r.bottom-744*scale)/2)/scale;
        if(y>=400&&y<=455){if(x>=315&&x<=515){reset();lastTick=SteadyClock::now();InvalidateRect(hwnd,nullptr,FALSE);}
            else if(x>=565&&x<=765)DestroyWindow(hwnd);}
        return 0;}
    case WM_KILLFOCUS:clearInput();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);
        RECT r;GetClientRect(hwnd,&r);
        if(r.right>0&&r.bottom>0&&ensureBackBuffer(dc,r.right,r.bottom)){
            {Graphics offscreen(backDC);
                float scale=std::min(r.right/1080.f,r.bottom/744.f);
                offscreen.TranslateTransform((r.right-1080*scale)/2,(r.bottom-744*scale)/2);
                offscreen.ScaleTransform(scale,scale);draw(offscreen);
            }
            BitBlt(dc,0,0,r.right,r.bottom,backDC,0,0,SRCCOPY);
        }
        EndPaint(hwnd,&ps);return 0;}
    case WM_DESTROY:KillTimer(hwnd,1);releaseBackBuffer();PostQuitMessage(0);return 0;}return DefWindowProc(hwnd,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    GdiplusStartupInput input;ULONG_PTR token;if(GdiplusStartup(&token,&input,nullptr)!=Ok)return 1;int result=0;
    try{people=load(L"novos_perssonagens.png");prepareCharacters();enemy=load(L"inimigos_BombMan.png",true);fx=load(L"BombsExplosionsv2.png",true);boss=load(L"boss.png");items=load(L"icons_blocos.png");fire=load(L"fire_2.png");
        minigame=load(L"Game Boy Advance - Bomberman Tournament - Miscellaneous - Bomberman and Ceedrun Minigame.png");
        marioSheet=load(L"mario.png");prepareGuests();
        if(!loadCompactAtlas()){
            stage=load(L"SNES - Super Bomberman - Story Mode Stages - Stage 6-3.png");stage2=load(L"SNES - Super Bomberman - Story Mode Stages - Stage 2-5.png");
            lobby=load(L"GameCube - Bomberman Land 2 - Tilesets - Survival Bomberman Tiles (Lobby, General).png");
            dungeon=load(L"GameCube - Bomberman Land 2 - Tilesets - Survival Bomberman Tiles (Dungeon).png");
            prepareArenaTiles();
        }
        // No large terrain sheets are needed during gameplay.
        stage.reset();stage2.reset();lobby.reset();dungeon.reset();
        // Remove both shades of the VFX cell background, retaining the flame colors.
        for(UINT y=0;y<fire->GetHeight();++y)for(UINT x=0;x<fire->GetWidth();++x){Color c;fire->GetPixel(x,y,&c);if((c.GetR()==84&&c.GetG()==109&&c.GetB()==142)||(c.GetR()==112&&c.GetG()==154&&c.GetB()==209))fire->SetPixel(x,y,Color(0,0,0,0));}
        reset();int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        bool snapshot=argc>=3&&std::wstring(argv[1])==L"--snapshot";
        if(argc>=3&&std::wstring(argv[1])==L"--export-tiles"){if(!saveCompactAtlas(argv[2]))result=1;}
        else if(argc>=3&&std::wstring(argv[1])==L"--benchmark"){if(!benchmarkRendering(argv[2]))result=1;}
        else if(argc>=3&&std::wstring(argv[1])==L"--directions"){if(!saveDirectionPreview(argv[2]))result=1;}
        else if(snapshot){if(argc>4)theme=std::clamp(_wtoi(argv[4]),0,themeCount-1);
            float seconds=argc>3?(float)_wtof(argv[3]):8.f;seconds=std::clamp(seconds,0.f,300.f);for(int i=0;i<(int)(seconds*60);++i)update(1.f/60);
            Bitmap b(1080,744,PixelFormat32bppARGB);{Graphics g(&b);draw(g);}CLSID id;if(!pngEncoder(&id)||b.Save(argv[2],&id,nullptr)!=Ok)result=1;
            if(auto report=_wfopen((std::wstring(argv[2])+L".txt").c_str(),L"w")){
                fprintf(report,"theme=%d\nseconds=%.2f\napples_thrown=%d\napple_hits=%d\nmario_hp=%d\nmario_kills=%d\ndragon_hp=%d\nboss_hp=%.0f\nscore=%d\n",theme,clockTime,applesThrown,appleHits,mario.hp,mario.kills,dragon.hp,bossHP,score);fclose(report);
            }
        }else{WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"BomberArena";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
            RECT r{0,0,1080,744};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);HWND hwnd=CreateWindowW(wc.lpszClassName,L"Bomber Arena - batalha de sprites",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
            if(!hwnd){throw std::wstring(L"Nao foi possivel criar a janela.");} ShowWindow(hwnd,show);lastTick=SteadyClock::now();SetTimer(hwnd,1,16,nullptr);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        LocalFree(argv);
    }catch(const std::wstring&e){MessageBoxW(nullptr,e.c_str(),L"Bomber Arena",MB_ICONERROR);result=1;}
    for(auto&person:characterFrames)for(auto&direction:person)for(auto&cell:direction)cell.reset();
    for(auto&dir:marioFrames)for(auto&cell:dir)cell.reset();
    for(auto&cell:dragonFrames)cell.reset();
    for(auto&dir:minionFrames)for(auto&cell:dir)cell.reset();
    for(auto&set:arenaTiles)for(auto&tile:set)tile.reset();
    terrainCache.reset();lobby.reset();dungeon.reset();minigame.reset();marioSheet.reset();appleTexture.reset();
    releaseBackBuffer();people.reset();enemy.reset();fx.reset();boss.reset();stage.reset();stage2.reset();items.reset();fire.reset();GdiplusShutdown(token);return result;
}

