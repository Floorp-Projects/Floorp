/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998, 1999 Raph Levien
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
#include <string.h>
#include <math.h>
#include "art_misc.h"
#include "art_vpath.h"
#include "art_svp.h"
#include "art_svp_vpath.h"
#include "art_gray_svp.h"
#include "art_rgb_svp.h"
#include "art_svp_vpath_stroke.h"
#include "art_svp_ops.h"
#include "art_affine.h"
#include "art_rgb_affine.h"
#include "art_rgb_bitmap_affine.h"
#include "art_rgb_rgba_affine.h"
#include "art_alphagamma.h"
#include "art_svp_point.h"
#include "art_vpath_dash.h"
#include "art_render.h"
#include "art_render_gradient.h"
#include "art_render_svp.h"
#include "art_svp_intersect.h"

#ifdef DEAD_CODE
static void
test_affine (void) {
  double src[6];
  double dst[6];
  double src2[6];
  char str[128];
  int i;
  ArtPoint ps, pd, ptmp;

  for (i = 0; i < 6; i++)
    {
      src[i] = (rand () * 2.0 / RAND_MAX) - 1.0;
      src2[i] = (rand () * 2.0 / RAND_MAX) - 1.0;
    }
#if 0
  src[0] = 0.9999999;
  src[1] = -0.000001;
  src[2] = 0.000001;
  src[3] = 0.9999999;
  src[4] = 0;
  src[5] = 0;
#if 1
  src[0] = 0.98480775;
  src[1] = -0.17364818;
  src[2] = 0.17364818;
  src[3] = 0.98480775;
#endif

  src2[0] = 0.98480775;
  src2[1] = -0.17364818;
  src2[2] = 0.17364818;
  src2[3] = 0.98480775;
#endif


  ps.x = rand() * 100.0 / RAND_MAX;
  ps.y = rand() * 100.0 / RAND_MAX;

  art_affine_point (&pd, &ps, src);
  art_affine_invert (dst, src);
  art_affine_point (&ptmp, &pd, dst);
  art_affine_to_string (str, src);
  printf ("src = %s\n", str);
  art_affine_to_string (str, dst);
  printf ("dst = %s\n", str);
  printf ("point (%g, %g) -> (%g, %g) -> (%g, %g)\n",
	  ps.x, ps.y, pd.x, pd.y, ptmp.x, ptmp.y);

  art_affine_point (&ptmp, &ps, src);
  art_affine_point (&pd, &ptmp, src2);
  art_affine_to_string (str, src2);
  printf ("src2 = %s\n", str);
  printf ("point (%g, %g) -> (%g, %g) -> (%g, %g)\n",
	  ps.x, ps.y, ptmp.x, ptmp.y, pd.x, pd.y);
  art_affine_multiply (dst, src, src2);
  art_affine_to_string (str, dst);
  printf ("dst = %s\n", str);
  art_affine_point (&pd, &ps, dst);
  printf ("point (%g, %g) -> (%g, %g)\n",
	  ps.x, ps.y, pd.x, pd.y);

}
#endif

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
#if 0
      r = r + 0.9 * (250 - r);
#endif
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

#define TILE_SIZE 512
#define NUM_ITERS 1
#define COLOR

#ifdef COLOR
#define BYTES_PP 3
#else
#define BYTES_PP 1
#endif

#ifndef nDEAD_CODE
static void
print_svp (ArtSVP *vp)
{
  int i, j;

  for (i = 0; i < vp->n_segs; i++)
    {
      printf ("segment %d, dir = %s (%f, %f) - (%f, %f)\n",
	      i, vp->segs[i].dir ? "down" : "up",
	      vp->segs[i].bbox.x0,
	      vp->segs[i].bbox.y0,
	      vp->segs[i].bbox.x1,
	      vp->segs[i].bbox.y1);
      for (j = 0; j < vp->segs[i].n_points; j++)
        printf ("  (%g, %g)\n",
                vp->segs[i].points[j].x,
                vp->segs[i].points[j].y);
    }
}
#endif

static void
print_vpath (ArtVpath *vpath)
{
  int i;

  for (i = 0; vpath[i].code != ART_END; i++)
    printf ("%g %g %s\n",
	    vpath[i].x, vpath[i].y,
	    vpath[i].code == ART_MOVETO_OPEN ? "moveto %open" :
	    vpath[i].code == ART_MOVETO ? "moveto" :
	    vpath[i].code == ART_LINETO ? "lineto" :
	    "?");

  printf ("stroke\n");
}

static void
make_testpat (void)
{
  ArtVpath *vpath, *vpath2, *vpath3;
  ArtSVP *svp, *svp2;
  ArtSVP *svp3;
  art_u8 buf[512 * 512 * BYTES_PP];
  int i, j;
  int iter;
  art_u8 colorimg[256][256][3];
  art_u8 rgbaimg[256][256][4];
  art_u8 bitimg[16][2];
  int x, y;
  double affine[6];
  double affine2[6];
  double affine3[6];
  ArtAlphaGamma *alphagamma;
  double dash_data[] = { 20 };
  ArtVpathDash dash;

  dash.offset = 0;
  dash.n_dash = 1;
  dash.dash = dash_data;

#ifdef TEST_AFFINE
  test_affine ();
  exit (0);
#endif

  vpath = randstar (50);
  svp = art_svp_from_vpath (vpath);
  art_free (vpath);

  vpath2 = randstar (50);
#if 1
  vpath3 = art_vpath_dash (vpath2, &dash);
  art_free (vpath2);
  svp2 = art_svp_vpath_stroke (vpath3,
			       ART_PATH_STROKE_JOIN_MITER,
			       ART_PATH_STROKE_CAP_BUTT,
			       15,
			       4,
			       0.5);
  art_free (vpath3);
#else
  svp2 = art_svp_from_vpath (vpath2);
#endif

#if 1
  svp3 = art_svp_intersect (svp, svp2);
#else
  svp3 = svp2;
#endif

#if 0
  print_svp (svp);
#endif

  for (y = 0; y < 256; y++)
    for (x = 0; x < 256; x++)
      {
	colorimg[y][x][0] = (x + y) >> 1;
	colorimg[y][x][1] = (x + (255 - y)) >> 1;
	colorimg[y][x][2] = ((255 - x) + y) >> 1;

	rgbaimg[y][x][0] = (x + y) >> 1;
	rgbaimg[y][x][1] = (x + (255 - y)) >> 1;
	rgbaimg[y][x][2] = ((255 - x) + y) >> 1;
	rgbaimg[y][x][3] = y;
      }

  for (y = 0; y < 16; y++)
    for (x = 0; x < 2; x++)
      bitimg[y][x] = (x << 4) | y;

  affine[0] = 0.5;
  affine[1] = .2;
  affine[2] = -.2;
  affine[3] = 0.5;
  affine[4] = 64;
  affine[5] = 64;
  
  affine2[0] = 1;
  affine2[1] = -.2;
  affine2[2] = .2;
  affine2[3] = 1;
  affine2[4] = 128;
  affine2[5] = 128;

  affine3[0] = 5;
  affine3[1] = -.2;
  affine3[2] = .2;
  affine3[3] = 5;
  affine3[4] = 384;
  affine3[5] = 32;

#if 0
  alphagamma = art_alphagamma_new (1.8);
#else
  alphagamma = NULL;
#endif

#ifdef COLOR
  printf ("P6\n512 512\n255\n");
#else
  printf ("P5\n512 512\n255\n");
#endif
  for (iter = 0; iter < NUM_ITERS; iter++)
    for (j = 0; j < 512; j += TILE_SIZE)
      for (i = 0; i < 512; i += TILE_SIZE)
	{
#ifdef COLOR
	  art_rgb_svp_aa (svp, i, j, i + TILE_SIZE, j + TILE_SIZE,
			  0xffe0a0, 0x100040,
			  buf + (j * 512 + i) * BYTES_PP, 512 * BYTES_PP,
			  alphagamma);
	  art_rgb_svp_alpha (svp2, i, j, i + TILE_SIZE, j + TILE_SIZE,
			     0xff000080,
			     buf + (j * 512 + i) * BYTES_PP, 512 * BYTES_PP,
			     alphagamma);
	  art_rgb_svp_alpha (svp3, i, j, i + TILE_SIZE, j + TILE_SIZE,
			     0x00ff0080,
			     buf + (j * 512 + i) * BYTES_PP, 512 * BYTES_PP,
			     alphagamma);
	  art_rgb_affine (buf + (j * 512 + i) * BYTES_PP,
			  i, j, i + TILE_SIZE, j + TILE_SIZE, 512 * BYTES_PP,
			  (art_u8 *)colorimg, 256, 256, 256 * 3,
			  affine,
			  ART_FILTER_NEAREST, alphagamma);
	  art_rgb_rgba_affine (buf + (j * 512 + i) * BYTES_PP,
			       i, j, i + TILE_SIZE, j + TILE_SIZE,
			       512 * BYTES_PP,
			       (art_u8 *)rgbaimg, 256, 256, 256 * 4,
			       affine2,
			       ART_FILTER_NEAREST, alphagamma);
	  art_rgb_bitmap_affine (buf + (j * 512 + i) * BYTES_PP,
				 i, j, i + TILE_SIZE, j + TILE_SIZE,
				 512 * BYTES_PP,
				 (art_u8 *)bitimg, 16, 16, 2,
				 0xffff00ff,
				 affine3,
				 ART_FILTER_NEAREST, alphagamma);
#else
	  art_gray_svp_aa (svp, i, j, i + TILE_SIZE, j + TILE_SIZE,
			   buf + (j * 512 + i) * BYTES_PP, 512 * BYTES_PP);
#endif
	}

  art_svp_free (svp2);
  art_svp_free (svp3);
  art_svp_free (svp);

#if 1
  fwrite (buf, 1, 512 * 512 * BYTES_PP, stdout);
#endif
}

static void
test_dist (void)
{
  ArtVpath *vpath;
  ArtSVP *svp;
  art_u8 buf[512 * 512 * BYTES_PP];
  int x, y;
  int ix;
  double dist;
  int wind;

  vpath = randstar (20);
#ifdef NO_STROKE
  svp = art_svp_from_vpath (vpath);
#else
  svp = art_svp_vpath_stroke (vpath,
			       ART_PATH_STROKE_JOIN_MITER,
			       ART_PATH_STROKE_CAP_BUTT,
			       15,
			       4,
			       0.5);
#endif

  art_rgb_svp_aa (svp, 0, 0, 512, 512,
		  0xffe0a0, 0x100040,
		  buf, 512 * BYTES_PP,
		  NULL);

  ix = 0;
  for (y = 0; y < 512; y++)
    {
      for (x = 0; x < 512; x++)
	{
	  wind = art_svp_point_wind (svp, x, y);
	  buf[ix] = 204 - wind * 51;
	  dist = art_svp_point_dist (svp, x, y);
	  if (((x | y) & 0x3f) == 0)
	    {
	      fprintf (stderr, "%d,%d: %f\n", x, y, dist);
	    }
	  buf[ix + 1] = 255 - dist;
	  ix += 3;
	}
    }

  printf ("P6\n512 512\n255\n");
  fwrite (buf, 1, 512 * 512 * BYTES_PP, stdout);

}

static void
test_dash (void)
{
  ArtVpath *vpath, *vpath2;
  double dash_data[] = { 10, 4, 1, 4};
  ArtVpathDash dash;
	  
  dash.offset = 0;
  dash.n_dash = 3;
  dash.dash = dash_data;
  
  vpath = randstar (50);
  vpath2 = art_vpath_dash (vpath, &dash);
  printf ("%%!\n");
  print_vpath (vpath2);
  printf ("showpage\n");
  art_free (vpath);
  art_free (vpath2);
}

static void
test_render_gradient (art_u8 *buf)
{
  ArtGradientLinear gradient;
  ArtGradientStop stops[3] = {
    { 0.0, { 0x7fff, 0x0000, 0x0000, 0x7fff }},
    { 0.5, { 0x0000, 0x0000, 0x0000, 0x1000 }},
    { 1.0, { 0x0000, 0x7fff, 0x0000, 0x7fff }}
  };
  ArtVpath *vpath;
  ArtSVP *svp;
  ArtRender *render;

  gradient.a = 0.003;
  gradient.b = -0.0015;
  gradient.c = 0.1;
  gradient.spread = ART_GRADIENT_PAD;
  gradient.n_stops = sizeof(stops) / sizeof(stops[0]);
  gradient.stops = stops;

  vpath = randstar (50);
  svp = art_svp_from_vpath (vpath);

  render = art_render_new (0, 0, 512, 512, buf, 512 * 3, 3, 8, ART_ALPHA_NONE,
			   NULL);
  art_render_svp (render, svp);
  art_render_gradient_linear (render, &gradient, ART_FILTER_NEAREST);
  art_render_invoke (render);

}

static void
test_render_rad_gradient (art_u8 *buf)
{
  ArtGradientRadial gradient;
  ArtGradientStop stops[3] = {
    { 0.0, { 0xffff, 0x0000, 0x0000, 0xffff }},
    { 0.5, { 0xe000, 0xe000, 0x0000, 0xe000 }},
    { 1.0, { 0x0000, 0x0000, 0x0000, 0x0000 }}
  };
  ArtVpath *vpath;
  ArtSVP *svp;
  ArtRender *render;

  gradient.affine[0] = 3.0 / 512;
  gradient.affine[1] = 0;
  gradient.affine[2] = 0;
  gradient.affine[3] = 3.0 / 512;
  gradient.affine[4] = -1.5;
  gradient.affine[5] = -1.5;
  gradient.fx = 0.9;
  gradient.fy = 0.1;
  
  gradient.n_stops = sizeof(stops) / sizeof(stops[0]);
  gradient.stops = stops;

  vpath = randstar (50);
  svp = art_svp_from_vpath (vpath);

  render = art_render_new (0, 0, 512, 512, buf, 512 * 3, 3, 8, ART_ALPHA_NONE,
			   NULL);
  art_render_svp (render, svp);
  art_render_gradient_radial (render, &gradient, ART_FILTER_NEAREST);
  art_render_invoke (render);

}

static void
test_gradient (void)
{
  ArtVpath *vpath;
  ArtSVP *svp;
  art_u8 buf[512 * 512 * 3];
  ArtRender *render;
  ArtPixMaxDepth color[3] = {0x0000, 0x0000, 0x8000 };
  int i;
  const int n_iter = 1;

  vpath = randstar (50);
  svp = art_svp_from_vpath (vpath);

  for (i = 0; i < n_iter; i++)
    {
#define USE_RENDER
#ifdef USE_RENDER
      render = art_render_new (0, 0, 512, 512, buf, 512 * 3, 3, 8, ART_ALPHA_NONE,
			       NULL);
      art_render_clear_rgb (render, 0xfff0c0);
      art_render_svp (render, svp);
      art_render_image_solid (render, color);
      art_render_invoke (render);
#else
      art_rgb_svp_aa (svp, 0, 0, 512, 512, 0xfff0c0, 0x000080,
		      buf, 512 * 3, NULL);
#endif
    }

#if 1
  test_render_gradient (buf);
#endif
  test_render_rad_gradient (buf);

  printf ("P6\n512 512\n255\n");
  fwrite (buf, 1, 512 * 512 * 3, stdout);
}

static void
output_svp_ppm (const ArtSVP *svp)
{
  art_u8 buf[512 * 512 * 3];
  art_rgb_svp_aa (svp, 0, 0, 512, 512, 0xfff0c0, 0x000080,
		  buf, 512 * 3, NULL);
  printf ("P6\n512 512\n255\n");
  fwrite (buf, 1, 512 * 512 * 3, stdout);
}

static void
test_intersect (void)
{
  ArtVpath vpath[] = {

#if 0
    /* two triangles */
    { ART_MOVETO, 100, 100 },
    { ART_LINETO, 300, 400 },
    { ART_LINETO, 400, 200 },
    { ART_LINETO, 100, 100 },
    { ART_MOVETO, 110, 110 },
    { ART_LINETO, 310, 410 },
    { ART_LINETO, 410, 210 },
    { ART_LINETO, 110, 110 },
#endif

#if 0
    /* a bowtie */
    { ART_MOVETO, 100, 100 },
    { ART_LINETO, 400, 400 },
    { ART_LINETO, 400, 100 },
    { ART_LINETO, 100, 400 },
    { ART_LINETO, 100, 100 },
#endif

#if 1
    /* a square */
    { ART_MOVETO, 100, 100 },
    { ART_LINETO, 100, 400 },
    { ART_LINETO, 400, 400 },
    { ART_LINETO, 400, 100 },
    { ART_LINETO, 100, 100 },
#endif

#if 1
    /* another square */
#define XOFF 10
#define YOFF 10
    { ART_MOVETO, 100 + XOFF, 100 + YOFF },
    { ART_LINETO, 100 + XOFF, 400 + YOFF },
    { ART_LINETO, 400 + XOFF, 400 + YOFF },
    { ART_LINETO, 400 + XOFF, 100 + YOFF },
    { ART_LINETO, 100 + XOFF, 100 + YOFF },
#endif

    { ART_END, 0, 0}
  };
  ArtSVP *svp, *svp2;
  ArtSvpWriter *swr;

  svp = art_svp_from_vpath (vpath);

#define RUN_INTERSECT
#ifdef RUN_INTERSECT
  swr = art_svp_writer_rewind_new (ART_WIND_RULE_ODDEVEN);
  art_svp_intersector (svp, swr);

  svp2 = art_svp_writer_rewind_reap (swr);
#endif

#if 0
  output_svp_ppm (svp2);
#else
  print_svp (svp2);
#endif

  art_svp_free (svp);

#ifdef RUN_INTERSECT
  art_svp_free (svp2);
#endif
}

static void
usage (void)
{
  fprintf (stderr, "usage: testart <test>\n"
"  where <test> is one of:\n"
"  testpat    -- make random star + gradients test pattern\n"
"  gradient   -- test pattern for rendered gradients\n"
"  dash       -- dash test (output is valid PostScript)\n"
"  dist       -- distance test\n"
"  intersect  -- softball test for intersector\n");
  exit (1);
}

int
main (int argc, char **argv)
{
  if (argc < 2)
    usage ();

  if (!strcmp (argv[1], "testpat"))
    make_testpat ();
  else if (!strcmp (argv[1], "gradient"))
    test_gradient ();
  else if (!strcmp (argv[1], "dist"))
    test_dist ();
  else if (!strcmp (argv[1], "dash"))
    test_dash ();
  else if (!strcmp (argv[1], "intersect"))
    test_intersect ();
  else
    usage ();
  return 0;
}
