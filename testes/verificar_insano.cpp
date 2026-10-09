// Exercise combat rules without opening a window or loading artwork.
#define wWinMain arenaEntryForTests
#include "../modo_insano/batalha_referencia.cpp"
#undef wWinMain
#include <iostream>
#include <stdexcept>

void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void emptyArena(){
    reset();
    for(int y=1;y<H-1;++y)for(int x=1;x<W-1;++x){board[y][x]=0;loot[y][x]=0;}
    appleCooldown=1000;mario.think=1000;dragon.think=1000;dragon.spit=1000;
    mario.x=1;mario.y=1;mario.px=1;mario.py=1;
    for(int i=0;i<4;++i){auto&a=actors[i];a.x=12;a.y=8;a.px=12;a.py=8;a.inv=0;}
    std::fill(std::begin(keys),std::end(keys),false);manual=false;controlMario=false;
}
void landApple(int x,int y){apples.push_back({1.5f,1.5f,x+.5f,y+.5f,0,.1f});updateGuests(.11f);}
int main(){
    try{
        emptyArena();actors[0].x=4;actors[0].y=5;actors[0].px=4;actors[0].py=5;
        actors[0].inv=5;landApple(4,5);check(actors[0].hp==0&&appleHits==1&&mario.kills==1,"Apple must permanently eliminate a fighter, even during damage immunity");
        landApple(4,5);check(actors[0].hp==0&&appleHits==1,"Dead enemy must not receive another kill award");
        for(int i=0;i<1200;++i)update(1.f/120);
        check(actors[0].hp==0,"Eliminated fighter must never respawn");

        emptyArena();landApple(7,5);check(minions[0].dead,"Apple must permanently eliminate a sentinel");
        emptyArena();dragon.inv=5;landApple(7,7);check(dragon.hp==0,"Apple must permanently eliminate Ceedrun");

        emptyArena();board[5][5]=2;landApple(5,5);
        check(board[5][5]==0,"Apple must break a crate");
        emptyArena();board[5][5]=1;landApple(5,5);
        check(board[5][5]==1,"Apple must preserve solid walls");

        emptyArena();bombs.push_back({5,5,0,2,2.5f});landApple(5,5);
        check(bombs[0].fuse==0,"Apple must trigger a bomb");update(1.f/60);
        check(bombs.empty()&&!flames.empty(),"Triggered bomb must explode");

        emptyArena();mario.inv=0;hurtMario(0);
        check(mario.hp==2&&matchState==MatchState::Playing,"Mario must survive the first hit");
        hurtMario(0);check(mario.hp==2,"One sustained contact must not count as multiple hits");
        mario.inv=0;hurtMario(0);check(mario.hp==1,"Mario must survive the second hit");
        mario.inv=0;hurtMario(0);
        check(mario.hp==0&&actors[0].kills==1&&matchState==MatchState::Defeat,"Third hit must defeat Mario and request a new match");
        updateGuests(10);check(mario.hp==0,"Mario must not respawn without choosing Yes");
        reset();check(mario.hp==3&&dragon.hp==3&&!minions[0].dead&&actors[0].hp==3&&bossHP==100&&matchState==MatchState::Playing,"Restart must restore the whole cast and the boss");

        emptyArena();bossHP=2;damageBoss(3);int awarded=score;damageBoss(3);
        check(bossHP==0&&score==awarded,"Boss defeat must award points only once");

        emptyArena();mario.x=5;mario.y=5;mario.px=5;mario.py=5;mario.think=0;
        manual=true;controlMario=true;appleCooldown=0;keys['D']=true;keys[VK_SPACE]=true;
        updateGuests(.1f);check(mario.x==6&&mario.dir==3&&applesThrown==1,"Manual Mario must move and throw right");
        check(!apples.empty()&&apples[0].tx>mario.px,"Manual apple must follow facing direction");

        emptyArena();mario.x=5;mario.y=5;mario.px=5;mario.py=5;mario.think=0;
        board[5][6]=1;manual=true;controlMario=true;keys['D']=true;
        updateGuests(.1f);check(mario.x==5,"Mario must not walk through walls");

        // Manual movement ignores AI think delay and turns back during a step.
        emptyArena();mario.x=5;mario.y=5;mario.px=5;mario.py=5;
        manual=true;controlMario=true;pressKey('D');updateGuests(.05f);
        float beforeReverse=mario.px;
        check(beforeReverse>5&&mario.dir==3,"Mario must respond immediately, even with AI delay pending");
        pressKey('A');updateGuests(.02f);
        check(mario.px<beforeReverse&&mario.dir==1,"Latest key must reverse movement and facing immediately");
        keys['A']=false;check(requestedDirection()==3,"Releasing newest key must restore the older held key");

        emptyArena();mario.x=5;mario.y=5;mario.px=5;mario.py=5;
        manual=true;controlMario=true;pressKey('D');updateGuests(.05f);keys['D']=false;inputBuffer=0;
        for(int i=0;i<60;++i)updateGuests(1.f/120);
        float stopped=mario.px;updateGuests(.1f);
        check(std::abs(mario.px-stopped)<.001f&&!mario.walking,"Mario must settle at the next tile and stay still after release");

        for(int d=0;d<4;++d){emptyArena();Actor a{};a.x=5;a.y=5;a.px=5;a.py=5;
            check(startMove(a,d),"Every direction must be available in an empty arena");advanceActor(a,.1f);
            check(a.dir==d&&(a.px-5)*dx[d]+(a.py-5)*dy[d]>0,"Sprite direction must match actual movement");}

        // Physics must consume the same time regardless of presentation frequency or theme.
        float baselineX=0,baselineClock=0;
        for(int fps:{120,60,30,15}){emptyArena();paused=false;slow=false;manual=true;controlMario=true;
            mario.x=3;mario.y=5;mario.px=3;mario.py=5;pressKey('D');
            for(int i=0;i<fps;++i){theme=i%themeCount;advanceRealTime(1.0/fps);}
            if(fps==120){baselineX=mario.px;baselineClock=clockTime;}
            check(std::abs(mario.px-baselineX)<.001f&&std::abs(clockTime-baselineClock)<.001f,"FPS and theme must not change game speed");
        }
        float timeBeforePause=clockTime;paused=true;advanceRealTime(.2);
        check(clockTime==timeBeforePause,"Paused game must not advance");paused=false;

        emptyArena();slow=false;manual=true;controlMario=true;
        for(int i=0;i<60;++i)advanceRealTime(1.0/60);
        check(std::abs(clockTime-.65f)<.001f,"Manual Mario mode must slow the entire match by 35 percent");
        emptyArena();manual=false;controlMario=false;
        for(int i=0;i<60;++i)advanceRealTime(1.0/60);
        check(std::abs(clockTime-.8f)<.001f,"Automatic mode must slow the entire match by 20 percent");
        emptyArena();manual=true;controlMario=false;
        for(int i=0;i<60;++i)advanceRealTime(1.0/60);
        check(std::abs(clockTime-1.f)<.001f,"White manual mode must keep its existing pace");

        emptyArena();check(!walkable(7,1)&&!walkable(7,2),"Living boss must block its entire footprint");
        Actor approach{};approach.x=7;approach.y=3;approach.px=7;approach.py=3;
        check(!startMove(approach,2)&&approach.y==3,"A fighter must not walk into the boss");
        float hpBefore=bossHP;explode({7,3,0,3,0});
        check(bossHP==hpBefore-4&&bossHitTimer>0,"Bomb flame touching boss must cause damage and feedback");
        check(std::none_of(flames.begin(),flames.end(),[](auto&f){return f.x==7&&f.y==1;}),"Flame must stop on the boss surface");
        damageBoss(100);check(walkable(7,2),"Defeated boss must release its occupied area");
        check(matchState==MatchState::Victory,"Defeating the boss must end the match with Mario victory");
        float finishedTime=clockTime;update(10);check(clockTime==finishedTime,"Match must stop after victory");

        emptyArena();board[1][9]=2;board[2][10]=1;
        check(startBossMove(3,2),"Boss must be able to start a horizontal move");
        check(board[1][9]==0&&board[2][10]==0,"Boss must clear interior obstacles along its path");
        check(!walkable(10,1),"Boss movement path must be reserved against fighters");
        for(int i=0;i<90;++i)updateBoss(1.f/60);
        check(!bossMotion.moving&&bossMotion.x==8&&bossTile(10,1)&&!bossTile(6,1),"Collision footprint must move with boss graphics");
        hpBefore=bossHP;explode({11,1,0,3,0});check(bossHP==hpBefore-4,"Bombs must damage the boss at its new position");
        emptyArena();actors[0].x=10;actors[0].y=1;actors[0].px=10;actors[0].py=1;
        check(!startBossMove(3,2),"Boss must not move through a live character");
        check(!startBossMove(1,20),"Boss must stay inside the arena borders");
        emptyArena();bossMotion.wait=.1f;updateBoss(.11f);
        check(bossMotion.moving,"Boss must start moving when its interval expires");

        // Exercise the actual clickable Yes/No handlers on a hidden window.
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BattleRestartTest";
        RegisterClassW(&wc);HWND hwnd=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,1080,744,nullptr,nullptr,wc.hInstance,nullptr);
        check(hwnd!=nullptr,"Restart test window must be created");
        auto click=[&](float x,float y){RECT r;GetClientRect(hwnd,&r);float scale=std::min(r.right/1080.f,r.bottom/744.f);
            int px=(int)(x*scale+(r.right-1080*scale)/2),py=(int)(y*scale+(r.bottom-744*scale)/2);
            SendMessageW(hwnd,WM_LBUTTONUP,0,MAKELPARAM(px,py));};
        emptyArena();mario.hp=0;actors[0].hp=0;minions[0].dead=true;matchState=MatchState::Defeat;
        click(415,427);check(matchState==MatchState::Playing&&mario.hp==3&&actors[0].hp==3&&!minions[0].dead,"Clicking Yes must restart with every character alive");
        matchState=MatchState::Defeat;click(665,427);check(!IsWindow(hwnd),"Clicking No must close the game window");

        manual=false;controlMario=false;reset();std::array<bool,4> eliminated{};
        for(int i=0;i<180*60;++i){update(1.f/60);for(int j=0;j<4;++j){if(eliminated[j])check(actors[j].hp==0,"Enemy must remain dead throughout the match");if(actors[j].hp==0)eliminated[j]=true;}}
        check(applesThrown>0&&appleHits>0,"Battle must include apple throws and hits");
        check(mario.hp>=0&&mario.hp<=3&&bossHP>=0,"Health must remain within bounds");
        std::cout<<"OK: permanent apple kills, Mario three hits, clickable restart, boss movement/collision/damage, pacing and combat\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
