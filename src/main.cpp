#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "gltext.h"

typedef unsigned char byte;
typedef unsigned int uint;

const int ScreenWidth = 1024, ScreenHeight = 768;


SDL_Texture* offscreen=NULL;

bool running = true;
bool playSounds = true;

enum  GameState { RESTART, RUNNING, GAMEOVER } ;

GameState gameState = GAMEOVER;

int pixels[240*240];
int lightmap[240 * 240];
int brightness[512];  // used for ambient brightness by distance

uint map[1024*1024];

uint sprites[18*4*16*12*12];

struct Entity {
    int x, y, dir, health, under, dead;
    int timer ; // was data[9]  i think this is a timer
    int d8, d3 ;    // data[8],data[3] ... no clue what these are
};

Entity   entities[320] ;

int score = 0;
int hurtTime = 0;
int bonusTime = 0;
int xWin0 = 0;
int yWin0 = 0;
int xWin1 = 0;
int yWin1 = 0;
int xCam = 0, yCam = 0;
int tick = 0;
int level = 0;
int shootDelay = 0;
int rushTime = 150;
int damage = 20;
int ammo = 20;
int clips = 20;

int closestHitDist = 0;
int closestHit =0;


SDL_Window *mainWindow;
SDL_Renderer *renderer;



int mouseX, mouseY ;
bool clicked = false;

bool moveUp, moveDown, moveLeft, moveRight, doReload  ;

void Setup();
void UserInput();
void UpdateGame();
void Shutdown();
void drawString( const char* txt, int x, int y );
void fireGun(double playerDir, double C, double S);


uint getMap( int x, int y ) {  return map[ (x&1023) | (y&1023)<<10 ]; }
void setMap( int x, int y, int v ) { map[ (x&1023) | (y&1023)<<10 ] = v; }


int min( int val, int mv  ) { return val < mv ? val : mv; }
int max( int val, int mv  ) { return val > mv ? val : mv; }

Mix_Chunk*  shoot_fx, *reload_fx;

//SDL_Surface*  overlay;
//SDL_Texture*  overlayTexture;

Font*         myfont;
FontTarget*   fontTarget;


void drawString( const char* txt, int x, int y ) {
  WriteText( fontTarget, txt, x * ScreenWidth/240, y * ScreenHeight / 240 );
}


int main(int argc, char* args[])
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	IMG_Init(IMG_INIT_PNG); //initialize SDL_image

  Mix_Init(MIX_INIT_FLAC);

  if ( 0!= Mix_OpenAudio(MIX_DEFAULT_FREQUENCY,MIX_DEFAULT_FORMAT, 2, 1024) ) {
    fprintf(stderr,"Error initializing audio channels!\n");
    exit(-3);
  };


  shoot_fx = Mix_LoadWAV("shoot.wav");
  reload_fx = Mix_LoadWAV("reload.wav");

	mainWindow = SDL_CreateWindow("Left4kDead", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ScreenWidth, ScreenHeight,
                                SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_GRABBED  );
	renderer = SDL_CreateRenderer(mainWindow, -1, 0);

  offscreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 240, 240 );

  myfont = LoadFont("myfont.fnt");
  fontTarget = CreateFontTarget(myfont, renderer, ScreenWidth, ScreenHeight);


  Setup();

  //overlay = SDL_CreateRGBSurface(0, 240, 240, 32, 0xff0000,0xff00,0xff,0xff000000 );
  //overlayTexture= SDL_CreateTextureFromSurface(renderer, overlay);

  //SDL_SetSurfaceBlendMode(overlay, SDL_BLENDMODE_BLEND);
  //SDL_FillRect(overlay, NULL, SDL_MapRGBA(overlay->format, 0xff, 0, 0, 0x30) );
  //SDL_UpdateTexture(overlayTexture, NULL, overlay->pixels, overlay->pitch);


  uint frames     = 0;
  uint  start     = SDL_GetTicks();
  uint frameTicks = 1000 / 30; // 30 fps

  uint nextFrame = SDL_GetTicks() + frameTicks;

	while (running)
	{
	  uint t = SDL_GetTicks();
	  if ( t < nextFrame ) {
      SDL_Delay( nextFrame - t );
      continue;
	  }

	  if ( nextFrame + frameTicks < t ) {  // framerate is dropping below 30 fps
	      nextFrame+= frameTicks;          // skip a frame
	      UpdateGame();
	      continue;
	  }

	  nextFrame += frameTicks;

	  UserInput();
    UpdateGame();

    SDL_UpdateTexture( offscreen,NULL, pixels, 240*sizeof(uint) );
    SDL_RenderCopy(renderer, offscreen, NULL, NULL);
    BlitFontTarget(fontTarget,renderer);

    //SDL_RenderCopy(renderer, overlayTexture, NULL, NULL);

		SDL_RenderPresent(renderer);
    frames ++ ;

    ClearFont(fontTarget);
	}

  float gameTime = (double)( SDL_GetTicks() - start ) / 1000.0;

  printf("Elapsed time:%f\n", gameTime );
  printf("Frames:%d\n", frames );
  printf("FPS:%f\n", (frames / gameTime ));

  FreeFontTarget(fontTarget);
  FreeFont(myfont);

  Mix_FreeChunk(shoot_fx);
  Mix_FreeChunk(reload_fx);
  Mix_CloseAudio();
  Mix_Quit();

  SDL_DestroyTexture(offscreen);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(mainWindow);
	SDL_Quit();
	return 0;
}

const double PI = M_PI;
const double TWOPI = 2.0 * PI;

int randInt( int mx )
{
  return rand() % mx;
}


void Setup()
{
  memset( pixels, 0, sizeof pixels);
  memset( sprites, 0, sizeof sprites);

  double offs = 30.0;
  for (int i = 0; i < 512; i++)
  {
    brightness[i] = (int) (255.0 * offs / (i + offs));
    if (i < 4) brightness[i] = brightness[i] * i / 4;
  }

  int pix = 0;
  for (int i = 0; i < 18; i++)
  {
      int skin = 0xFF9993;      // flesh color
      int clothes = 0xffffff ;  // white clothes

      if (i > 0)
      {
          skin = 0xa0ff90;      // green skin
          clothes = (randInt(0x1000000) & 0x7f7f7f);  // random clothes
      }
      for (int t = 0; t < 4; t++) // frames of animation?
      {
          for (int d = 0; d < 16; d++)  // 16 directions
          {
              double dir = d * PI * 2 / 16.0;

              if (t == 1) dir += 0.5 * PI * 2 / 16.0;
              if (t == 3) dir -= 0.5 * PI * 2 / 16.0;

              double C = cos(dir);
              double S = sin(dir);

              for (int y = 0; y < 12; y++)
              {
                  int col = 0x000000;
                  for (int x = 0; x < 12; x++)
                  {
                      int xPix = (int) (C * (x - 6) + S * (y - 6) + 6.5);
                      int yPix = (int) (C * (y - 6) - S * (x - 6) + 6.5);

                      if (i == 17)
                      {
                          if (xPix > 3 && xPix < 9 && yPix > 3 && yPix < 9)
                          {
                             col = 0xff0000 + (t & 1) * 0xff00;
                          }
                      }
                      else
                      {
                          if ((t == 1) && (xPix > 1) && (xPix < 4) && (yPix > 3) && (yPix < 8)) col = skin;
                          if ((t == 3) && (xPix > 8) && (xPix < 11) && (yPix > 3) && (yPix < 8)) col = skin;

                          if ((xPix > 1) && (xPix < 11) && (yPix > 5) && (yPix < 8))
                          {
                              col = clothes;
                          }
                          if ((xPix > 4) && (xPix < 8) && (yPix > 4) && (yPix < 8))
                          {
                              col = skin;
                          }
                      }
                      sprites[pix++] = col;
                      col = ( col> 1) ? 1 : 0;
                  }
              }
          }
      }
  }
}


void Shutdown()
{

}





void RestartGame();
void RunGame();
void WinLevel();



uint Red( uint pixel )    { return (pixel >>16 ) & 0xff; }
uint Green( uint pixel )  { return (pixel >> 8) & 0xff ; }
uint Blue( uint pixel )   { return pixel & 0xff; }

uint Pixel(int r, int g, int b ) {
    return  min(b,0xff) |
            ( min(g,0xff)<<8) |
            (min(r,0xff)<<16);
}



// draw hud, apply lightmap, red flash and noise ambiance
void UpdateScreen()
{
  bonusTime = bonusTime * 8 / 9;
  hurtTime /= 2;

  for (int y = 0; y < 240; y++)
  {
      for (int x = 0; x < 240; x++)
      {
        int noise = 0; //randInt(16) * randInt(16) / 16;
        if (gameState!=RUNNING) noise = randInt(16) * 4;

        int c = pixels[x + y * 240];
        int lum = lightmap[x + y * 240]  ;

        lightmap[x + y * 240] = 0;
        int r = Red(c) * lum  / 255 + noise;
        int g = Green(c) * lum /255  + noise;
        int b = Blue(c) * lum /255 + noise;

        r = (r * (255 - hurtTime) / 255 ) + hurtTime;
        g = (g * (255 - bonusTime) / 255) + bonusTime;
        pixels[x + y * 240] = Pixel(r,g,b) ;
      }
      if ((y % 2 == 0) && (y >= damage) && (y < 220))
      {
        for (int x = 232; x < 238; x++)
        {
            pixels[y * 240 + x] = (0x800000);  // health bar
        }
      }
      if (y % 2 == 0 && (y >= ammo && y < 220))
      {
        for (int x = 224; x < 230; x++)
        {
          pixels[y * 240 + x] = (0x808000);  // ammo bar
        }
      }
      if (y % 10 < 9 && (y >= clips && y < 220))
      {
        for (int x = 221; x < 222; x++)
        {
          pixels[y * 240 + 221] = (0xffff00); // clips bar
        }
      }
   }
}

void UpdateGame()
{
  static char txt[200];

  if ( gameState==GAMEOVER ) {
    tick++;
    drawString("Left 4k Dead", 10, 50 );
    if ( tick>60 && clicked ) {
      RestartGame();
    }
  }
  else if ( gameState==RUNNING ) {
    RunGame();

    if (tick < 60) {
        sprintf( txt, "Level %d", level );
        drawString(txt, 90, 70);
        drawString( "Press F1 to disable sounds", 150, 190 );
        drawString( "Press F10 to quit", 150, 200  );
    } else {
      sprintf( txt, "%d", score );
      drawString(txt, 4,228);
    }
  }

  UpdateScreen();

}

void RestartGame()
{
  level = 0;
  shootDelay = 0;
  rushTime=150;
  damage=20;
  ammo=20;
  clips=20;
  gameState = RUNNING;
  WinLevel();
}


void nextMonster(int m, double playerDir, double C, double S)
{
  Entity* e = &entities[m];
  int xPos = e->x;
  int yPos = e->y;
  if ( e->health == 0) //respawn
  {
      xPos = (randInt(62) + 1) * 16 + 8;
      yPos = (randInt(62) + 1) * 16 + 8;

      int xd = xCam - xPos;
      int yd = yCam - yPos;

      if ((xd * xd + yd * yd) < (180 * 180))
      {
          xPos = 1;
          yPos = 1;
      }

      if ( (getMap(xPos ,yPos) < 0xfffffe ) &&
          ((m <= 128) || (rushTime > 0) || ((m > 255) && (tick == 1))))
      {
        e->x = xPos;
        e->y = yPos;
        e->under = getMap( xPos, yPos );
        setMap(xPos , yPos , 0xfffffe );
        e->timer = (rushTime > 0) || (randInt(3) == 0) ? 127 : 0;
        e->health = 1;
        e->dir = m & 15;
      }
      else
      {
          return;
      }
  }
  else
  {
      int xd = xPos - xCam;
      int yd = yPos - yCam;

      if (m >= 255)
      {
          if (xd * xd + yd * yd < 8 * 8)
          {
              setMap( xPos, yPos, entities[m].under );
              entities[m].health = 0;

              bonusTime = 120;
              if ((m & 1) == 0)
              {
                  damage = 20; // health
              }
              else
              {
                  clips = 20; // add clips
              }
              return;
          }
      }
      else if (xd * xd + yd * yd > 340 * 340)
      {
          setMap(xPos , yPos , entities[m].under);
          entities[m].health = 0;
          return;
      }
  }


  int xm = xPos - xCam + 120;
  int ym = entities[m].y - yCam + 120;

  int d = entities[m].dir;
  if (m == 0)
  {
      d = (((int) (playerDir / (PI * 2) * 16 + 4.5 + 16)) & 15);
  }

  d += ((entities[m].d3 / 4) & 3) * 16;

  int p = (0 * 16 + d) * 144;
  if (m > 0)
  {
      p += ((m & 15) + 1) * 144 * 16 * 4;
  }

  if (m > 255)
  {
      p = (17 * 4 * 16 + ((m & 1) * 16 + (tick & 15))) * 144;
  }

  // draw 12x12 sprite around xm, ym
  for (int y = ym - 6; y < ym + 6; y++)
  {
      for (int x = xm - 6; x < xm + 6; x++)
      {
          int c = sprites[p++];
          if ( (c > 0) && (x >= 0) && (y >= 0) && (x < 240) && (y < 240))
          {
            pixels[x + y * 240] = (c);
          }
      }
  }

  bool moved = false;

  if (entities[m].dead > 0)
  {
    entities[m].health += randInt(3) + 1;
    entities[m].dead = 0;

    double rot = 0.25;
    int amount = 8;
    double poww = 32;


    if (entities[m].health >= 2+level)
    {
        rot = PI * 2;
        amount = 60;
        poww = 16;
        setMap(xPos, yPos, 0xa00000 );
        entities[m].health = 0;
        score += level;
    }
    for (int i = 0; i < amount; i++)
    {
        double pow = (randInt(100) * randInt(100)) * poww / 10000+4;
        double dir = (randInt(100) - randInt(100)) / 100.0 * rot;
        double xdd = (cos(playerDir + dir) * pow) + randInt(4) - randInt(4);
        double ydd = (sin(playerDir + dir) * pow) + randInt(4) - randInt(4);
        int col = (randInt(128) + 120);
        for (int j = 2; j < pow; j++)
        {
            int xd = (int) (xPos + xdd * j / pow);
            int yd = (int) (yPos + ydd * j / pow);
            if ( getMap(xd,yd) >= 0xff0000) break;
            if (randInt(2) != 0)
            {
              setMap(xd,yd, col <<16  );  //blood splatters
            }
        }
    }
    return;
  }

  int xPlayerDist = xCam - xPos;
  int yPlayerDist = yCam - yPos;

  if (m <= 255)
  {
      double rx = -(C * xPlayerDist - S * yPlayerDist);
      double ry = C * yPlayerDist + S * xPlayerDist;

      if ((rx > -6) && (rx < 6) && (ry > -6) && (ry < 6) && (m > 0) )
      {
          damage++;
          hurtTime += 20;
      }
      if ((rx > -32) && (rx < 220) && (ry > -32) && (ry < 32) && (randInt(10) == 0))
      {
          entities[m].timer+= 1;
      }
      if ((rx > 0) && (rx < closestHitDist) && (ry > -8) && (ry < 8))
      {
          closestHitDist = (int) (rx);
          closestHit = m;
      }

      for (int i = 0; i < 2; i++)
      {
          int xa = 0;
          int ya = 0;
          xPos = entities[m].x;
          yPos = entities[m].y;

          if (m == 0)
          {
              if (moveLeft) xa--;
              if (moveRight) xa++;
              if (moveUp) ya--;
              if (moveDown) ya++;
          }
          else
          {
              if (entities[m].timer < 8) return;

              if (entities[m].d8 != 12)
              {
                  xPlayerDist = (entities[m].d8) % 5 - 2;
                  yPlayerDist = (entities[m].d8) / 5 - 2;
                  if (randInt(10) == 0)
                  {
                      entities[m].d8 = 12;
                  }
              }

              double xxd = sqrt(xPlayerDist * xPlayerDist);
              double yyd = sqrt(yPlayerDist * yPlayerDist);
              if ((randInt(1024) / 1024.0) < (yyd / xxd))
              {
                  if (yPlayerDist < 0) ya--;
                  if (yPlayerDist > 0) ya++;
              }
              if ((randInt(1024) / 1024.0) < (xxd / yyd))
              {
                  if (xPlayerDist < 0) xa--;
                  if (xPlayerDist > 0) xa++;
              }

              moved = true;
              double dir = atan2(yPlayerDist, xPlayerDist);
              entities[m].dir = (((int) (dir / (((PI * 2) * 16) + 4.5 + 16))) & 15);
          }

          ya *= i;
          xa *= 1 - i;

          if ((xa != 0 ) || (ya != 0))
          {
            setMap(xPos,yPos, entities[m].under );
              for (int xx = xPos + xa - 3; xx <= xPos + xa + 3; xx++) {
                  for (int yy = yPos + ya - 3; yy <= yPos + ya + 3; yy++) {
                      if (getMap(xx , yy) >= 0xfffffe)
                      {
                        setMap(xPos ,yPos, 0xfffffe);
                        entities[m].d8 = randInt(25);
                        goto dirLoop;
                      }
                  }
              }

              moved = true;
              entities[m].x += xa;
              entities[m].y += ya;
              entities[m ].under = getMap(xPos + xa, yPos + ya );
              setMap(xPos+xa,yPos + ya,  0xfffffe);
          }
          dirLoop:continue;
      }

      if (moved)
      {
          entities[m].d3+=1;
      }
  }

} // end nextmonster

void updateLightmap( double playerDir )
{
  for (int i = 0; i < 960; i++) // update visibility in 960 directions around player (240 pixels * 4 sides)
  {
      int xt = i % 240 - 120;
      int yt = (i / 240 % 2) * 239 - 120;

      if (i >= 480)
      {
          int tmp = xt;
          xt = yt;
          yt = tmp;
      }

      double dd = atan2(yt, xt) - playerDir; // get angle of raycast
      if (dd < - PI) dd += TWOPI ;
      if (dd >=  PI) dd -= TWOPI ;

      int brr = (int) ((1 - dd * dd) * 255);

      int dist = 120;
      if (brr < 0)
      {
          brr = 0;
          dist = 32;
      }
      if (tick < 60) brr = brr * tick / 60;  // this makes light slowly increase first 2 seconds

      int j = 0;
      for (; j < dist; j++)
      {
          int xx = xt * j / 120 + 120;
          int yy = yt * j / 120 + 120;
          int xm = xx + xCam - 120;
          int ym = yy + yCam - 120;

          if (getMap(xm,ym) == 0xffffff) break; // hit a wall, stop expanding light

          int xd = (xx - 120) * 256 / 120;
          int yd = (yy - 120) * 256 / 120;

          int ddd = (xd * xd + yd * yd) / 256;
          int br = brightness[ddd] * brr / 255;

          if (ddd < 16)
          {
              int tmp = 128 * (16 - ddd) / 16;
              br = br + tmp * (255 - br) / 255;
          }

          lightmap[xx + yy * 240] = br;
      }
  }
}


void RunGame()
{
    tick++;
    rushTime++;

    if (rushTime >= 150)
    {
        rushTime = -randInt(2000);
    }
    // Move player:
    double playerDir = atan2(mouseY - 120, mouseX - 120);

    double shootDir = playerDir + (randInt(100) - randInt(100)) / 100.0 * 0.1;
    double C = cos(-shootDir);
    double S = sin(-shootDir);

    xCam = entities[0].x;
    yCam = entities[0].y;


    updateLightmap(playerDir);

    // draw map/background into pixels from map[]?
    for (int y = 0; y < 240; y++)
    {
        int xm = xCam - 120;
        int ym = y + yCam - 120;
        for (int x = 0; x < 240; x++)
        {
            pixels[x + y * 240] = getMap(xm + x ,  ym ) ;
        }
    }


    // closest Hit of raycast
    closestHitDist = 0;
    for (int j = 0; j < 250; j++)
    {
        int xm = xCam + (int) (C * j / 2);
        int ym = yCam - (int) (S * j / 2);
        if ( getMap(xm , ym ) == 0xffffff) break; // hit wall, stop cast
        closestHitDist = j / 2;
    }

    // closestHit = monster number closest to player, updated in nextMonster
    closestHit = 0;

    for (int m = 0; m < 256 + 16; m++) {
      nextMonster(m, playerDir,C, S);
    }

    bool shoot = ((shootDelay--) < 0) && clicked;

    if (shoot)
    {
      fireGun(playerDir, C, S);
      if (playSounds)
      {
        int ch = Mix_PlayChannel(-1,shoot_fx, 0);
        if ( ch<0 )printf("Error playing shoot sound!\n");
        else Mix_Volume(ch, 16);
      }
    }


    if (damage >= 220)
    {
        clicked = false;
        hurtTime = 255;
        tick =0;
        gameState=GAMEOVER;
    }
    else if (doReload && ammo > 20 && clips < 220)
    {
        shootDelay = 30;
        ammo = 20;
        clips += 10;
        if (playSounds )
        {
          int ch = Mix_PlayChannel(-1, reload_fx, 0);
          if ( ch<0 )printf("Error playing reload sound!\n");
          else Mix_Volume(ch, 16);
        }
    }
    else if ((xCam > xWin0 ) && (xCam < xWin1) && (yCam > yWin0) && (yCam < yWin1))
    {
       WinLevel();
    }
}


void fireGun(double playerDir, double C, double S)
{
  if (ammo >= 220)
    {
      shootDelay = 2;
      clicked = false;
    }
    else
    {
      shootDelay = 1;
      ammo += 4;
    }

    // monster hit then kill it
    if (closestHit > 0)
    {
      entities[closestHit].dead = 1;
      entities[closestHit].timer = 127;
    }

    // this section draws the bullets ?
    int glow = 0;
    for (int j = closestHitDist; j >= 0; j--)
    {
      int xm = +(int) (C * j) + 120;
      int ym = -(int) (S * j) + 120;
      if ((xm > 0) && (ym > 0) && (xm < 240) && (ym < 240))
      {
        if ((randInt(20) == 0) || (j == closestHitDist))
        {
            pixels[xm + ym * 240] = 0xffffff;  // 'tracer' type bullets
            glow = 200; // glow near impact
        }
        lightmap[xm + ym * 240] += glow * (255 - lightmap[xm + ym * 240]) / 255;
      }
      glow = glow * 20 / 21;
    }

    if (closestHitDist < 120)  // is the closest hit within the viewable area?
    {
      closestHitDist -= 3;
      int xx = (int) (120 + C * closestHitDist);
      int yy = (int) (120 - S * closestHitDist);


      for (int x = -12; x <= 12; x++)  // sparks/light near bullet impact
      {
        for (int y = -12; y <= 12; y++)
        {
          int xd = xx + x;
          int yd = yy + y;
          if (xd >= 0 && yd >= 0 && xd < 240 && yd < 240)
          {
             lightmap[xd + yd * 240] += 2000 / (x * x + y * y + 10) * (255 - lightmap[xd + yd * 240]) / 255;
          }
        }
      }


      for (int i = 0; i < 10; i++)
      {
        double pow = randInt(100) * randInt(100) * 8.0 / 10000;
        double dir = (randInt(100) - randInt(100)) / 100.0;
        int xd = (int) (xx - cos(playerDir + dir) * pow) + randInt(4) - randInt(4);
        int yd = (int) (yy - sin(playerDir + dir) * pow) + randInt(4) - randInt(4);
        if ((xd >= 0) && (yd >= 0) && (xd < 240) && (yd < 240))
        {
            if (closestHit > 0)
            {
                lightmap[xd + yd * 240] = (0xff0000); // red
            }
            else
            {
                //lightmap[xd + yd * 240] = 0xcacaca;  // light gray
                lightmap[xd + yd * 240] = 0xeaeaea;  // light gray
            }
        }
      }
  }
}


// rebuilds the level on startup or level change
void WinLevel()
{
  memset( entities, 0, sizeof entities);
  tick = 0;
  level++;
  //srand( SDL_GetTicks() );
  srand(4329+level);

    int i = 0;
    for (int y = 0; y < 1024; y++)
    {
        for (int x = 0; x < 1024; x++)
        {
            int br = randInt(32) + 112;
            map[i] = ((br / 3) << 16)  | ((br) << 8);
            if ((x < 4) || (y < 4) || (x >= 1020) || (y >= 1020))
            {
                map[i] = 0xFFFEFE;
            }
            i++;
        }
    }

    // this makes 70 rooms in the level ?
    for (i = 0; i < 70; i++)
    {
        int w = randInt(8) + 2;  //(2-9)
        int h = randInt(8) + 2;
        int xm = randInt(64 - w - 2) + 1;
        int ym = randInt(64 - h - 2) + 1;

        w *= 16;
        h *= 16;

        w += 5;
        h += 5;
        xm *= 16;
        ym *= 16;

        if (i==68)  // number 68 is starting room
        {
          entities[0].x = xm+w/2;
          entities[0].y = ym+h/2;
          entities[0].under = 0x808080;
          entities[0].health = 1;
        }

        xWin0 = xm+5;
        yWin0 = ym+5;
        xWin1 = xm + w-5;
        yWin1 = ym + h-5;
        for (int y = ym; y < ym + h; y++)
            for (int x = xm; x < xm + w; x++)
            {
                int d = x - xm;
                if (xm + w - x - 1 < d) d = xm + w - x - 1;
                if (y - ym < d) d = y - ym;
                if (ym + h - y - 1 < d) d = ym + h - y - 1;

                setMap(x, y, 0xFF8052);  // this is the border of the walls
                if (d > 4)
                {
                    int br = randInt(16) + 112;
                    if (((x + y) & 3) == 0)
                    {
                        br += 16;
                    }
                    setMap(x,y, Pixel( (br * 3 / 3) , (br * 4 / 4) ,(br * 4 / 4)) );
                }
                if (i == 69)
                {
                    setMap(x, y , getMap(x,y) & 0xff0000 );  // win room has a red floor
                }
            }

        // doorways
        for (int j = 0; j < 2; j++)
        {
            int xGap = randInt(w - 24) + xm + 5;
            int yGap = randInt(h - 24) + ym + 5;
            int ww = 5;
            int hh = 5;

            xGap = xGap / 16 * 16 + 5;
            yGap = yGap / 16 * 16 + 5;
            if (randInt(2) == 0)
            {
                xGap = xm + (w - 5) * randInt(2);
                hh = 11;
            }
            else
            {
                ww = 11;
                yGap = ym + (h - 5) * randInt(2);
            }
            for (int y = yGap; y < yGap + hh; y++)
                for (int x = xGap; x < xGap + ww; x++)
                {
                    int br = randInt(32) + 112 - 64;
                    if ( (x+y)%3 ==0 )br += 16;
                    setMap(x,y, Pixel( (br * 3 / 3) , (br * 4 / 4) , (br * 4 / 4) ) );
                }
        }
    }


    // set all walls to 0xFFFFFF
    for (int y = 1; y < 1024 - 1; y++)
    {
      for (int x = 1; x < 1024 - 1; x++)
      {
        for (int xx = x - 1; xx <= x + 1; xx++) {
            for (int yy = y - 1; yy <= y + 1; yy++) {
                if (getMap(xx ,yy ) < 0xff0000) goto inloop;
            }
        }

        setMap( x , y , 0xffffff );
        inloop: continue;
      }
    }


}



void UserInput()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT)
    {
      running = false;
    }
    if ( event.type==SDL_KEYDOWN || event.type==SDL_KEYUP )
    {
      bool pressed = (event.type==SDL_KEYDOWN);
      switch( event.key.keysym.sym)
      {
        case SDLK_a: moveLeft = pressed; break;
        case SDLK_d: moveRight = pressed; break;
        case SDLK_w: moveUp = pressed; break;
        case SDLK_s: moveDown = pressed; break;
        case SDLK_r: doReload = pressed; break;
        case SDLK_F1: if ( pressed ) playSounds= !playSounds; break;
        case SDLK_F10: running = false; break;
        default: break;
      }
    }
    else if ( event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP )
    {
      clicked = event.button.button==SDL_BUTTON_LEFT && event.button.state==SDL_PRESSED;
    }
    else if ( event.type==SDL_MOUSEMOTION)
    {
      mouseX = event.motion.x * 240 / ScreenWidth ;
      mouseY = event.motion.y * 240 / ScreenHeight;
    }

  }
}

