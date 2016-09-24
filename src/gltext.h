
#ifndef GLTEXT_H
#define GLTEXT_H

#include "sdl.h"

#include <stdio.h>
#include <stdlib.h>

// Handles loading and rendering fonts under OpenGL
// Uses FNT files which are created using Lev Povalahev's program located at:
// http://www.levp.de/3d/fonts.html
// (I use none of his code, just his utility to generate the font files)

struct FontHeader
{
  unsigned int    id ;
  unsigned int    width, height;
  unsigned int    fheight;
} __attribute__((packed, aligned(1)));

struct CharInfo // character metrics
{
  unsigned int      top, left, right, bottom;
  unsigned int      available ;
  unsigned short    width; // of character
} __attribute__((packed, aligned(1)));



struct Font
{
  char              filename[128];
  struct CharInfo   info[256];
  SDL_Surface*      source;
  unsigned short    spacing ;
  unsigned int      fontHeight ;
  float             ratio ;
  char              error[256];
};
typedef struct Font   Font;




bool ReadFNT( FILE* f, Font* dest );

Font*     LoadFont( const char* fnt_file )
{
  Font*   result ;

  if ( strlen(fnt_file)>127 ) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Filename is too long:%s", fnt_file)  ;
    return NULL;
  }

  FILE * f = fopen( fnt_file, "rb");
  if ( f==NULL ) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Could not open file:%s", fnt_file);
      return NULL;
  }

  result= (Font*)calloc(sizeof(Font), 1);
  strncpy( result->filename, fnt_file, 127 );
  result->ratio = 1.0;

  if ( false==ReadFNT( f, result ) ) {
    free( result );
    result = NULL;
  }

  fclose( f );
  return result;
}


void FreeFont( Font* f ) {
  if ( f!=NULL ) {
    if ( f->source!=NULL ) SDL_FreeSurface(f->source);
    f->source=NULL;
    free(f);
  }
}


void  TextSize( Font* f, const char*txt, int* w, int* h ) {

  if (f==NULL) return ;

  if ( h!=NULL ) {
      (*h) = f->fontHeight;
  }

  if ( w!=NULL ) {

    int len = strlen( txt );
    if ( len> 512 ) len = 512;

    int total =0 ;
    for( int n =0; n < len; n++) {
      short c = (short)txt[n];
      total += f->info[c].width;
    }
    (*w)= total + (f->spacing * (len-1) );
  }
}

bool ReadFNT(FILE* f, Font* dest) {

  FontHeader header;
  fread( &header, sizeof(FontHeader), 1, f);

  CharInfo*  info;

  for( int n=0; n<256; n++ )
  {
    unsigned int coords[4];
    float   aspect;

    info = dest->info+n;

    fread( coords, sizeof(unsigned int), 4 , f);
    fread( &info->available, sizeof(unsigned int), 1 , f);
    fread( &aspect, sizeof(float), 1 , f);
    info->top = coords[0];
    info->left= coords[1];
    info->bottom = coords[2];
    info->right = coords[3];
    info->width = (unsigned short)(aspect * header.fheight) ;
  }

  unsigned int size = header.width * header.height;

  if ( size == 0 )
  {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Font bitmap size is zero! Aborting...load of %s", dest->filename );
    return NULL;
  }


  // read and convert the mono luminance textels
  unsigned char*  mono = (unsigned char*)calloc( sizeof(unsigned char), size );
  fread( mono, 1, size, f);
  dest->source = SDL_CreateRGBSurface(0, header.width, header.height, 32, 0xff0000, 0x00ff00, 0xff, 0xff000000);
  //SDL_SetSurfaceBlendMode(dest->source, SDL_BLENDMODE_BLEND);
  SDL_FillRect(dest->source, NULL, SDL_MapRGBA(dest->source->format, 0, 0, 0,0) );

  int color = SDL_MapRGBA(dest->source->format, 0xff, 0xff, 0xff, 0xff );
  unsigned char* ptr = (unsigned char*)dest->source->pixels;
  int p = dest->source->pitch;
  int pos = 0;
  for( unsigned int y = 0; y < header.height; y++) {
    int* row = (int*)(&ptr[y*p]);
    for( unsigned int x = 0; x < header.width; x++ ) {
        unsigned char b = mono[pos];
        pos++;
        row[x] = (b>0) ? color : 0 ;
    }
  }

  free( mono );
  dest->fontHeight = header.fheight;
  dest->spacing= 1;
  return true;
}




// FONT Targets are what you render to, keyed to the renderer and dimensions


struct FontTarget
{
  Font*           font;
  SDL_Surface*    surface;
  SDL_Texture*    texture;
  int             width, height ;
};
typedef struct FontTarget FontTarget;



FontTarget*     CreateFontTarget(Font* f,  SDL_Renderer* renderer, int w, int h) {
  if ( f==NULL ) return NULL;

  FontTarget*  target = (FontTarget*)calloc(sizeof(FontTarget), 1);
  if ( target==NULL ) {
      SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Cannot allocate font target!" );
      exit(-2);
  }

  target->font = f;
  target->width = w;
  target->height = h;
  target->surface = SDL_CreateRGBSurface(0, w, h, 32, 0xff0000,0xff00,0xff,0xff000000 );
  target->texture = SDL_CreateTextureFromSurface(renderer, target->surface);
  //SDL_SetTextureAlphaMod( target->texture, SDL_BLENDMODE_BLEND );
  return target;
}



void FreeFontTarget(FontTarget* target) {
  if ( target==NULL ) return ;

  if ( target->surface!=NULL ) SDL_FreeSurface(target->surface);

  if ( target->texture!=NULL ) SDL_DestroyTexture(target->texture);
  memset( target, 0, sizeof(FontTarget));
  free( target );
}


void  BlitFontTarget(FontTarget* target, SDL_Renderer* renderer ) {
  SDL_UpdateTexture(target->texture, NULL, target->surface->pixels, target->surface->pitch ) ;
  SDL_RenderCopy(renderer, target->texture, NULL, NULL ) ;
}
// use this to clear a font target texture between frames
void ClearFont( FontTarget* target ) {
  SDL_FillRect(target->surface,NULL, SDL_MapRGBA(target->surface->format, 0, 0, 0,0) );
}


void WriteText(FontTarget* target, const char* txt, int x, int y) {

  if ( target==NULL ) return;

  Font*  font = target->font;
  SDL_Surface* surface= target->surface;

  SDL_Rect  srcRect, destRect;

  destRect.x = x;
  destRect.y = y;

  int width =0 ;
  int len = strlen(txt);

  for( int n =0; n < len; n++ )
  {
    unsigned char c = txt[n];
    if ( c > 128 ) continue;
    CharInfo info = font->info[c];
    if ( info.available==0 ) continue;

      width = (unsigned int)(info.width * font->ratio);

      srcRect.x = info.left;
      srcRect.y = info.top;
      destRect.w =  srcRect.w = info.right - info.left;
      destRect.h  = srcRect.h = info.bottom - info.top;

      SDL_BlitSurface(font->source, &srcRect, surface, &destRect );
      destRect.x += width + font->spacing;
  }
}

#endif // GLTEXT_H
