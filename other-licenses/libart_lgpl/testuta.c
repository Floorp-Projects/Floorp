/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "art_misc.h"
#include "art_uta.h"
#include "art_vpath.h"
#include "art_uta_vpath.h"
#include "art_rect.h"
#include "art_rect_uta.h"
#include "art_uta_rect.h"

#define TEST_UTA
#define noTEST_UTA_SPEED

#define XOFF 50
#define YOFF 700

static void
print_uta_ps (ArtUta *uta)
{
  int x, y;
  int x0, y0, x1, y1;
  int width = uta->width;
  ArtUtaBbox ub;

  for (y = 0; y < uta->height; y++)
    for (x = 0; x < width; x++)
      {
	ub = uta->utiles[y * width + x];
	if (ub != 0)
	  {
	    x0 = (uta->x0 + x) * ART_UTILE_SIZE + ART_UTA_BBOX_X0(ub);
	    x1 = (uta->x0 + x) * ART_UTILE_SIZE + ART_UTA_BBOX_X1(ub);
	    y0 = (uta->y0 + y) * ART_UTILE_SIZE + ART_UTA_BBOX_Y0(ub);
	    y1 = (uta->y0 + y) * ART_UTILE_SIZE + ART_UTA_BBOX_Y1(ub);
	    printf ("%% tile %d, %d: %d %d %d %d\n",
		    x, y,
		    ART_UTA_BBOX_X0(ub),
		    ART_UTA_BBOX_Y0(ub),
		    ART_UTA_BBOX_X1(ub),
		    ART_UTA_BBOX_Y1(ub));
	    printf ("%d %d moveto %d %d lineto %d %d lineto %d %d lineto closepath fill\n",
		    XOFF + x0, YOFF - y0, XOFF + x1, YOFF - y0,
		    XOFF + x1, YOFF - y1, XOFF + x0, YOFF - y1);
	  }
      }
}

static void
print_rbuf_ps (int *rbuf, int width, int height)
{
  int x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      if (1 && rbuf[y * width + x] != 0)
	printf ("%d %d moveto (%d) show\n", x * ART_UTILE_SIZE, y * ART_UTILE_SIZE,
		rbuf[y * width + x]);	
}

#if 0
void
randline (ArtUta *uta, int *rbuf, int rbuf_rowstride)
{
  double x0, y0, x1, y1;

  x0 = rand () * (500.0 / RAND_MAX);
  y0 = rand () * (500.0 / RAND_MAX);
  x1 = rand () * (500.0 / RAND_MAX);
  y1 = rand () * (500.0 / RAND_MAX);

  printf ("%g %g moveto %g %g lineto stroke\n", x0, y0, x1, y1);
  art_uta_add_line (uta, x0, y0, x1, y1, rbuf, rbuf_rowstride);
}
#endif

static void
print_ps_vpath (ArtVpath *vpath)
{
  int i;

  for (i = 0; vpath[i].code != ART_END; i++)
    {
      switch (vpath[i].code)
	{
	case ART_MOVETO:
	  printf ("%g %g moveto\n", XOFF + vpath[i].x, YOFF - vpath[i].y);
	  break;
	case ART_LINETO:
	  printf ("%g %g lineto\n", XOFF + vpath[i].x, YOFF - vpath[i].y);
	  break;
	default:
	  break;
	}
    }
  printf ("stroke\n");
}

static ArtVpath *
randstar (int n)
{
  ArtVpath *vec;
  int i;
  double r, th;

  vec = art_new (ArtVpath, n + 2);
  for (i = 0; i < n; i++)
    {
      vec[i].code = i ? ART_LINETO : ART_MOVETO;
      r = rand () * (250.0 / RAND_MAX);
      th = i * 2 * M_PI / n;
      vec[i].x = 250 + r * cos (th);
      vec[i].y = 250 - r * sin (th);
    }
  vec[i].code = ART_LINETO;
  vec[i].x = vec[0].x;
  vec[i].y = vec[0].y;
  i++;
  vec[i].code = ART_END;
  vec[i].x = 0;
  vec[i].y = 0;
  return vec;
}

int
main (int argc, char **argv)
{
  ArtUta *uta;
  int i;
  int *rbuf;
  ArtVpath *vec;
  ArtIRect *rects;
  int n_rects;

  if (argc == 2)
    srand (atoi (argv[1]));

#ifdef TEST_UTA
  printf ("%%!PS-Adobe\n");
  printf ("/Helvetica findfont 12 scalefont setfont\n");
  printf ("0.5 setlinewidth\n");

  printf ("0.5 setgray\n");
  for (i = 0; i < 500; i += ART_UTILE_SIZE)
    {
      printf ("%d %d moveto %d %d lineto stroke\n",
	      XOFF, YOFF - i, XOFF + 500, YOFF - i);
      printf ("%d %d moveto %d %d lineto stroke\n",
	      XOFF + i, YOFF, XOFF + i, YOFF - 500);
    }

  printf ("/a {\n");

#if 1
  vec = randstar (50);
  print_ps_vpath (vec);
  uta = art_uta_from_vpath (vec);
#ifdef TEST_UTA_RECT
  {
    ArtIRect bbox = {5, 5, 450, 450};
    uta = art_uta_from_irect (&bbox);
  }
#endif
  rbuf = 0;
#else
  uta = art_uta_new_coords (0, 0, 500, 500);

  rbuf = malloc (sizeof(int) * (500 >> ART_UTILE_SHIFT) * (500 >> ART_UTILE_SHIFT));
  for (i = 0; i < 10; i++)
    randline (uta, rbuf, 500 >> ART_UTILE_SHIFT);
#endif

  printf ("} def 1 0.5 0.5 setrgbcolor\n");

  print_uta_ps (uta);

  printf ("0 0 0.5 setrgbcolor\n");

  if (rbuf)
    print_rbuf_ps (rbuf, 500 >> ART_UTILE_SHIFT, 500 >> ART_UTILE_SHIFT);

  printf ("0 setgray a\n");

  rects = art_rect_list_from_uta (uta, 256, 64, &n_rects);

  printf ("%% %d rectangles:\n0 0 1 setrgbcolor\n", n_rects);

  for (i = 0; i < n_rects; i++)
    printf ("%d %d moveto %d %d lineto %d %d lineto %d %d lineto closepath stroke\n",
	    XOFF + rects[i].x0, YOFF - rects[i].y0,
	    XOFF + rects[i].x1, YOFF - rects[i].y0,
	    XOFF + rects[i].x1, YOFF - rects[i].y1,
	    XOFF + rects[i].x0, YOFF - rects[i].y1);

  printf ("showpage\n");
#endif

#ifdef TEST_UTA_SPEED
  for (i = 0; i < 1000; i++)
    {
      vec = randstar (50);
      uta = art_uta_from_vpath (vec);
      art_free (vec);
      art_uta_free (uta);
    }
#endif

  return 0;
}
