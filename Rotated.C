
/* ********************************************************************** */

/* xvertext, Copyright (c) 1992 Alan Richardson (mppa3@uk.ac.sussex.syma)
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both the
 * copyright notice and this permission notice appear in supporting
 * documentation.  All work developed as a consequence of the use of
 * this program should duly acknowledge such use. No representations are
 * made about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/* ********************************************************************** */

// Minor modifications by Chris Cannam for wm2/wmx
// Major modifications by Kazushi (Jam) Marukawa for wm2/wmx i18n patch
// Minor modifications by Sven Oliver (SvOlli) Moll for wmx multihead support

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Config.h"
#include "Rotated.h"

#ifndef CONFIG_USE_XFT

/* ---------------------------------------------------------------------- */


int xv_errno;

static char *my_strdup(const char *);
static char *my_strtok(char *, const char *);


/* ---------------------------------------------------------------------- */  


/* *** Routine to mimic `strdup()' (some machines don't have it) *** */

static char *my_strdup(const char *str)
{
  char *s;

  if (str == NULL) return NULL;

  s = (char *)malloc((unsigned)(strlen(str)+1));
  /* this error is highly unlikely ... */
  if (s == NULL) {
    fprintf(stderr, "Fatal error: my_strdup(): Couldn't do malloc (gulp!)\n");
    exit(1); 
  }

  strcpy(s, str);
  return s;
}


/* ---------------------------------------------------------------------- */


/* *** Routine to replace `strtok' : this one returns a zero
       length string if it encounters two consecutive delimiters *** */

static char *my_strtok(char *str1, const char *str2)
{
  char *ret;
  int i, j, stop;
  static int start, len;
  static char *stext;

  /* this error should never occur ... */
  if (str2 == NULL) {
    fprintf(stderr,
	    "Fatal error: my_strtok(): recieved null delimiter string\n");
    exit(1);
  }

  /* initialise if str1 not NULL ... */
  if (str1 != NULL) {
    start = 0;
    stext = str1;
    len = strlen(str1);
  }

  /* run out of tokens ? ... */
  if (start >= len) return NULL;

  /* loop through characters ... */
  for (i = start; i < len; i++) {

    /* loop through delimiters ... */
    stop = 0;
    for (j = 0; j < strlen(str2); j++)
      if (stext[i] == str2[j]) stop = 1;
 
    if (stop) break;
  }

  stext[i] = '\0';
  ret = stext + start;
  start = i+1;

  return ret;
}


/* ---------------------------------------------------------------------- */
  

/* *** Routine to return version/copyright information *** */

float XRotVersion(char *str, int n)
{
  if (str != NULL) strncpy(str, XV_COPYRIGHT, n);
  return XV_VERSION;
}


/* ---------------------------------------------------------------------- */


/* *** Load the rotated version of a given font *** */
 
XRotFontStruct *XRotLoadFont(Display *dpy, int screen, 
			     char *fontname, float angle)
{
  char val;
  XImage *I1, *I2;
  Pixmap canvas;
  Window root;
  GC font_gc;
  char text[3];/*, errstr[300];*/
  XFontStruct *fontstruct;
  XRotFontStruct *rotfont;
  int ichar, i, j, index, boxlen = 60, dir;
  int vert_w, vert_h, vert_len, bit_w, bit_h, bit_len;
  int min_char, max_char;
  unsigned char *vertdata, *bitdata;
  int ascent, descent, lbearing, rbearing;
  int on = 1, off = 0;

  /* make angle positive ... */
  if (angle < 0) do angle += 360; while (angle < 0);

  /* get nearest vertical or horizontal direction ... */
  dir = (int)((angle+45.)/90.)%4;

  /* useful macros ... */
  root = RootWindow(dpy,screen);
    
  /* create the depth 1 canvas bitmap ... */
  canvas = XCreatePixmap(dpy, root, boxlen, boxlen, 1);
 
  /* create a GC ... */
  font_gc = XCreateGC(dpy, canvas, 0, 0);
  XSetBackground(dpy, font_gc, off);

  /* load the font ... */
  char **ml;
  int mc;
  char *ds;

  XFontSet fontset;
  fontset = XCreateFontSet(dpy, fontname, &ml, &mc, &ds);
  if (fontset) {
    XFontStruct **fs_list;
    XFontsOfFontSet(fontset, &fs_list, &ml);
    fontstruct = fs_list[0];
  } else {
    fontstruct = NULL;
  }
#define XTextWidth(x,y,z)     XmbTextEscapement(rotfont->xfontset,y,z)
#define XDrawString(t,u,v,w,x,y,z) XmbDrawString(t,u,rotfont->xfontset,v,w,x,y,z)
#define XDrawImageString(t,u,v,w,x,y,z) XmbDrawString(t,u,rotfont->xfontset,v,w,x,y,z)
  if (fontstruct == NULL) {
    xv_errno = XV_NOFONT;
    return NULL;
  }
 
  XSetFont(dpy, font_gc, fontstruct->fid);

  /* allocate space for rotated font ... */
  rotfont = (XRotFontStruct *)malloc((unsigned)sizeof(XRotFontStruct));
  if (rotfont == NULL) {
    xv_errno = XV_NOMEM;
    return NULL;
  }
   
  /* determine which characters are defined in font ... */
  min_char = fontstruct->min_char_or_byte2; 
  max_char = fontstruct->max_char_or_byte2;
 
  /* we only want printing characters ... */
  if (min_char<32)  min_char = 32;
  //  if (max_char>126) max_char = 126; // bad for non-ascii iso
  if (max_char>255) max_char = 255;
     
  /* some overall font data ... */
  rotfont->name = my_strdup(fontname);
  rotfont->dir = dir;
  rotfont->min_char = min_char;
  rotfont->max_char = max_char;
  rotfont->max_ascent = fontstruct->max_bounds.ascent;
  rotfont->max_descent = fontstruct->max_bounds.descent;   
  rotfont->height = rotfont->max_ascent+rotfont->max_descent;

  /* remember xfontstruct for `normal' text ... */
  rotfont->xfontset = fontset;
  rotfont->xfontstruct = fontstruct;

  /* free pixmap and GC ... */
  XFreePixmap(dpy, canvas);
  XFreeGC(dpy, font_gc);

  return rotfont;
}


/* ---------------------------------------------------------------------- */


/* *** Free the resources associated with a rotated font *** */

void XRotUnloadFont(Display *dpy, int screen, XRotFontStruct *rotfont)
{
  int ichar;

  XFreeFontSet(dpy, rotfont->xfontset);

  /* rotfont should never be referenced again ... */
  free((char *)rotfont->name);
  free((char *)rotfont);
}


/* ---------------------------------------------------------------------- */
   

/* *** Return the width of a string *** */

int XRotTextWidth(XRotFontStruct *rotfont, char *str, int len)
{
  int i, width = 0, ichar;

  if (str == NULL) return 0;

  width = XTextWidth(rotfont->xfontstruct, str, strlen(str));

  return width;
}


/* ---------------------------------------------------------------------- */


/* *** A front end to XRotPaintString : mimics XDrawString *** */

void XRotDrawString(Display *dpy, int screen, XRotFontStruct *rotfont, 
		    Drawable drawable,
		    GC gc, int x, int y, char *str, int len)
{            
  static GC my_gc = 0;
  static int lastscreen = -1;
    
  int i, xp, yp, dir, ichar;

  if (str == NULL || len<1) return;

  dir = rotfont->dir;

  if((lastscreen != screen) && (my_gc != 0))
  {
    XFreeGC(dpy,my_gc);
    my_gc = 0;
  }
    
  if(my_gc == 0) my_gc = XCreateGC(dpy, drawable, 0, 0);

  XCopyGC(dpy, gc, GCForeground|GCBackground, my_gc);

  /* a horizontal string is easy ... */
  if (dir == 0) {
    XSetFillStyle(dpy, my_gc, FillSolid);
    XSetFont(dpy, my_gc, rotfont->xfontstruct->fid);
    XDrawString(dpy, drawable, my_gc, x, y, str, len);

    return;
  }

  /* vertical or upside down ... */

  XImage *I1, *I2;
  unsigned char *vertdata, *bitdata;
  int vert_w, vert_h, vert_len, bit_w, bit_h, bit_len;
  char val;

  /* useful macros ... */
  Window root = RootWindow(dpy,screen);

  int ascent = rotfont->max_ascent;
  int descent = rotfont->max_descent;
  int width = XRotTextWidth(rotfont, str, len);
  int height = rotfont->height;
  if (width < 1) width = 1;
  if (height < 1) height = 1;

  /* glyph width and height when vertical ... */
  vert_w = width;
  vert_h = height;

  /* width in bytes ... */
  vert_len = (vert_w-1)/8+1;   
 
  /* create the depth 1 canvas bitmap ... */
  Pixmap canvas = XCreatePixmap(dpy, root, width, height, 1);

  /* create a GC ... */
  GC font_gc = XCreateGC(dpy, canvas, 0, 0);
  XSetBackground(dpy, font_gc, 0);

  /* clear canvas */
  XSetForeground(dpy, font_gc, 0);
  XFillRectangle(dpy, canvas, font_gc, 0, 0, width, height);

  /* draw the character centre top right on canvas ... */
  XSetForeground(dpy, font_gc, 1);
  XSetFont(dpy, font_gc, rotfont->xfontstruct->fid);
  XDrawImageString(dpy, canvas, font_gc, 0, height-descent, str, len);

  /* reserve memory for first XImage ... */
  vertdata = (unsigned char *) malloc((unsigned)(vert_len*vert_h));
  if (vertdata == NULL) {
    xv_errno = XV_NOMEM;
    return;
  }

  /* create the XImage ... */
  I1 = XCreateImage(dpy, DefaultVisual(dpy, screen), 1, XYBitmap,
		    0, (char *)vertdata, vert_w, vert_h, 8, 0);

  if (I1 == NULL) {
    xv_errno = XV_NOXIMAGE;
    return;
  }

  I1->byte_order = I1->bitmap_bit_order = MSBFirst;

  /* extract character from canvas ... */
  XGetSubImage(dpy, canvas, 0, 0,
	       vert_w, vert_h, 1, XYPixmap, I1, 0, 0);
  I1->format = XYBitmap; 

  /* width, height of rotated character ... */
  if (dir == 2) { 
    bit_w = vert_w;
    bit_h = vert_h; 
  } else {
    bit_w = vert_h;
    bit_h = vert_w; 
  }

  /* width in bytes ... */
  bit_len = (bit_w-1)/8 + 1;

  /* reserve memory for the rotated image ... */
  bitdata = (unsigned char *)calloc((unsigned)(bit_h*bit_len), 1);
  if (bitdata == NULL) {
    xv_errno = XV_NOMEM;
    return;
  }

  /* create the image ... */
  I2 = XCreateImage(dpy, DefaultVisual(dpy, screen), 1, XYBitmap, 0,
		    (char *)bitdata, bit_w, bit_h, 8, 0); 

  if (I2 == NULL) {
    xv_errno = XV_NOXIMAGE;
    return;
  }

  I2->byte_order = I2->bitmap_bit_order = MSBFirst;

  /* map vertical data to rotated character ... */
  int j;
  for (j = 0; j < bit_h; j++) {
    for (i = 0; i < bit_w; i++) {
      /* map bits ... */
      if (dir == 1)
	val = vertdata[i*vert_len + (vert_w-j-1)/8] &
	  (128>>((vert_w-j-1)%8));

      else if (dir == 2)
	val = vertdata[(vert_h-j-1)*vert_len + (vert_w-i-1)/8] &
	  (128>>((vert_w-i-1)%8));
		
      else 
	val = vertdata[(vert_h-i-1)*vert_len + j/8] & 
	  (128>>(j%8));
    
      if (val) 
	bitdata[j*bit_len + i/8] = bitdata[j*bit_len + i/8] |
	  (128>>(i%8));
    }
  }

  /* create this character's bitmap ... */
  Pixmap image = XCreatePixmap(dpy, root, bit_w, bit_h, 1);
     
  /* put the image into the bitmap ... */
  XPutImage(dpy, image, font_gc, I2, 0, 0, 0, 0, bit_w, bit_h);
  
  /* free the image and data ... */
  XDestroyImage(I1);
  XDestroyImage(I2);
  /*      free((char *)bitdata);  -- XDestroyImage does this
	  free((char *)vertdata);*/

  /* free pixmap and GC ... */
  XFreePixmap(dpy, canvas);
  XFreeGC(dpy, font_gc);

  /* suitable offset ... */
  if (dir == 1) {
    xp = x-rotfont->max_ascent;
    yp = y-width; //rotfont->rbearing; 
  }
  else if (dir == 2) {
    xp = x-width; //rotfont->rbearing;
    yp = y-rotfont->max_descent+1;
  }
  else {
    xp = x-rotfont->max_descent+1;
    yp = y+0; //rotfont->lbearing; 
  }

  XSetFillStyle(dpy, my_gc, FillStippled);
  XSetStipple(dpy, my_gc, image);
  XSetTSOrigin(dpy, my_gc, xp, yp);
  XFillRectangle(dpy, drawable, my_gc, xp, yp,
		 bit_w, bit_h);

  XFreePixmap(dpy, image);
}

  
    
/* ---------------------------------------------------------------------- */


/* *** A front end to XRotPaintAlignedString : uses XRotDrawString *** */

void XRotDrawAlignedString(Display *dpy, int screen, XRotFontStruct *rotfont,
			   Drawable drawable, GC gc, int x, int y,
			   char *text, int align)
{  
  int xp = 0, yp = 0, dir;
  int i, nl = 1, max_width = 0, this_width;
  char *str1, *str2 = "\n\0", *str3;

  if (text == NULL) 
    return;
  
  dir = rotfont->dir;

  /* count number of sections in string ... */
  for (i = 0; i<strlen(text); i++) 
    if (text[i] == '\n') 
      nl++;

  /* find width of longest section ... */
  str1 = my_strdup(text);
  str3 = my_strtok(str1, str2);
  max_width = XRotTextWidth(rotfont, str3, strlen(str3));

  do {
    str3 = my_strtok((char *)NULL, str2);
    if (str3 != NULL)
      if (XRotTextWidth(rotfont, str3, strlen(str3))>max_width)
	max_width = XRotTextWidth(rotfont, str3, strlen(str3));
  }
  while (str3 != NULL);
 
  /* calculate vertical starting point according to alignment policy and
     rotation angle ... */
  if (dir == 0) {
    if (align == TLEFT || align == TCENTRE || align == TRIGHT)
      yp = y+rotfont->max_ascent;

    else if (align == BLEFT || align == BCENTRE || align == BRIGHT)
      yp = y-(nl-1)*rotfont->height - rotfont->max_descent;

    else 
      yp = y-(nl-1)/2*rotfont->height + rotfont->max_ascent -
	rotfont->height/2 - ((nl%2 == 0)?rotfont->height/2:0); 
  }

  else if (dir == 1) {
    if (align == TLEFT || align == TCENTRE || align == TRIGHT)
      xp = x+rotfont->max_ascent;

    else if (align == BLEFT || align == BCENTRE || align == BRIGHT)
      xp = x-(nl-1)*rotfont->height - rotfont->max_descent;

    else 
      xp = x-(nl-1)/2*rotfont->height + rotfont->max_ascent -
	rotfont->height/2 - ((nl%2 == 0)?rotfont->height/2:0); 
  }

  else if (dir == 2) {
    if (align == TLEFT || align == TCENTRE || align == TRIGHT)
      yp = y-rotfont->max_ascent;
     
    else if (align == BLEFT || align == BCENTRE || align == BRIGHT)
      yp = y+(nl-1)*rotfont->height + rotfont->max_descent;
     
    else 
      yp = y+(nl-1)/2*rotfont->height - rotfont->max_ascent +
	rotfont->height/2 + ((nl%2 == 0)?rotfont->height/2:0); 
  }

  else {
    if (align == TLEFT || align == TCENTRE || align == TRIGHT)
      xp = x-rotfont->max_ascent;
    
    else if (align == BLEFT || align == BCENTRE || align == BRIGHT)
      xp = x+(nl-1)*rotfont->height + rotfont->max_descent;
  
    else 
      xp = x+(nl-1)/2*rotfont->height - rotfont->max_ascent +
	rotfont->height/2 + ((nl%2 == 0)?rotfont->height/2:0); 
  }

  free(str1);
  str1 = my_strdup(text);
  str3 = my_strtok(str1, str2);
  
  /* loop through each section in the string ... */
  do {
    /* width of this section ... */
    this_width = XRotTextWidth(rotfont, str3, strlen(str3));

    /* horizontal alignment ... */
    if (dir == 0) {
      if (align == TLEFT || align == MLEFT || align == BLEFT)
	xp = x;
  
      else if (align == TCENTRE || align == MCENTRE || align == BCENTRE)
	xp = x-this_width/2;
 
      else 
	xp = x-max_width; 
    }   

    else if (dir == 1) {
      if (align == TLEFT || align == MLEFT || align == BLEFT)
	yp = y;

      else if (align == TCENTRE || align == MCENTRE || align == BCENTRE)
	yp = y+this_width/2;

      else 
	yp = y+max_width; 
    }

    else if (dir == 2) {
      if (align == TLEFT || align == MLEFT || align == BLEFT)
	xp = x;
  
      else if (align == TCENTRE || align == MCENTRE || align == BCENTRE)
	xp = x+this_width/2;
 
      else 
	xp = x+max_width; 
    }

    else {
      if (align == TLEFT || align == MLEFT || align == BLEFT)  
	yp = y;
     
      else if (align == TCENTRE || align == MCENTRE || align == BCENTRE)
	yp = y-this_width/2;
     
      else 
	yp = y-max_width; 
    }

    /* draw the section ... */
    XRotDrawString(dpy, screen, rotfont, drawable, gc, xp, yp,
		   str3, strlen(str3));

    str3 = my_strtok((char *)NULL, str2);

    /* advance position ... */
    if (dir == 0)
      yp += rotfont->height;
    else if (dir == 1)
      xp += rotfont->height;
    else if (dir == 2)
      yp -= rotfont->height;
    else 
      xp -= rotfont->height;
  }
  while (str3 != NULL);

  free(str1);
}

#endif
