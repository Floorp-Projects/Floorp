/*
 * art_render_gradient.c: Gradient image source for modular rendering.
 *
 * Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 2000 Raph Levien
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
 *
 * Authors: Raph Levien <raph@acm.org>
 *          Alexander Larsson <alla@lysator.liu.se>
 */

#include "config.h"
#include "art_render_gradient.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Hack to find out how to define alloca on different platforms.
 * Modified version of glib/galloca.h.
 */

#ifdef  __GNUC__
/* GCC does the right thing */
# undef alloca
# define alloca(size)   __builtin_alloca (size)
#elif defined (HAVE_ALLOCA_H)
/* a native and working alloca.h is there */ 
# include <alloca.h>
#else /* !__GNUC__ && !HAVE_ALLOCA_H */
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else /* !_MSC_VER */
#  ifdef _AIX
 #pragma alloca
#  else /* !_AIX */
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif /* !alloca */
#  endif /* !_AIX */
# endif /* !_MSC_VER */
#endif /* !__GNUC__ && !HAVE_ALLOCA_H */

#undef DEBUG_SPEW

typedef struct _ArtImageSourceGradLin ArtImageSourceGradLin;
typedef struct _ArtImageSourceGradRad ArtImageSourceGradRad;

struct _ArtImageSourceGradLin {
  ArtImageSource super;
  const ArtGradientLinear *gradient;
};

struct _ArtImageSourceGradRad {
  ArtImageSource super;
  const ArtGradientRadial *gradient;
  double a;
};

#define EPSILON 1e-6

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif /* MAX */

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif /* MIN */

static void
art_rgba_gradient_run (art_u8 *buf,
		       art_u8 *color1,
		       art_u8 *color2,
		       int len)
{
  int i;
  int r, g, b, a;
  int dr, dg, db, da;

#ifdef DEBUG_SPEW
  printf ("gradient run from %3d %3d %3d %3d to %3d %3d %3d %3d in %d pixels\n",
	  color1[0], color1[1], color1[2], color1[3], 
	  color2[0], color2[1], color2[2], color2[3],
	  len);
#endif
  
  r = (color1[0] << 16) + 0x8000;
  g = (color1[1] << 16) + 0x8000;
  b = (color1[2] << 16) + 0x8000;
  a = (color1[3] << 16) + 0x8000;
  dr = ((color2[0] - color1[0]) << 16) / len;
  dg = ((color2[1] - color1[1]) << 16) / len;
  db = ((color2[2] - color1[2]) << 16) / len;
  da = ((color2[3] - color1[3]) << 16) / len;

  for (i = 0; i < len; i++)
    {
      *buf++ = (r>>16);
      *buf++ = (g>>16);
      *buf++ = (b>>16);
      *buf++ = (a>>16);

      r += dr;
      g += dg;
      b += db;
      a += da;
    }
}

static void
calc_color_at (ArtGradientStop *stops,
	       int n_stops,
	       ArtGradientSpread spread,
	       double offset,
	       double offset_fraction,
	       int favor_start,
	       int ix,
	       art_u8 *color)
{
  double off0, off1;
  int j;
  
  if (spread == ART_GRADIENT_PAD)
    {
      if (offset < 0.0)
	{
	  color[0] = ART_PIX_8_FROM_MAX (stops[0].color[0]);
	  color[1] = ART_PIX_8_FROM_MAX (stops[0].color[1]);
	  color[2] = ART_PIX_8_FROM_MAX (stops[0].color[2]);
	  color[3] = ART_PIX_8_FROM_MAX (stops[0].color[3]);
	  return;
	}
      if (offset >= 1.0)
	{
	  color[0] = ART_PIX_8_FROM_MAX (stops[n_stops-1].color[0]);
	  color[1] = ART_PIX_8_FROM_MAX (stops[n_stops-1].color[1]);
	  color[2] = ART_PIX_8_FROM_MAX (stops[n_stops-1].color[2]);
	  color[3] = ART_PIX_8_FROM_MAX (stops[n_stops-1].color[3]);
	  return;
	}
    }

  if (ix > 0 && ix < n_stops)
    {
      off0 = stops[ix - 1].offset;
      off1 = stops[ix].offset;
      if (fabs (off1 - off0) > EPSILON)
	{
	  double interp;
	  double o;
	  o = offset_fraction;
	  
	  if ((fabs (o) < EPSILON) && (!favor_start))
	    o = 1.0;
	  else if ((fabs (o-1.0) < EPSILON) && (favor_start))
	    o = 0.0;
	  
	  /*
	  if (offset_fraction == 0.0  && !favor_start)
	    offset_fraction = 1.0;
	  */
	  
	  interp = (o - off0) / (off1 - off0);
	  for (j = 0; j < 4; j++)
	    {
	      int z0, z1;
	      int z;
	      z0 = stops[ix - 1].color[j];
	      z1 = stops[ix].color[j];
	      z = floor (z0 + (z1 - z0) * interp + 0.5);
	      color[j] = ART_PIX_8_FROM_MAX (z);
	    }
	  return;
	}
      /* If offsets are too close to safely do the division, just
	 pick the ix color. */
      color[0] = ART_PIX_8_FROM_MAX (stops[ix].color[0]);
      color[1] = ART_PIX_8_FROM_MAX (stops[ix].color[1]);
      color[2] = ART_PIX_8_FROM_MAX (stops[ix].color[2]);
      color[3] = ART_PIX_8_FROM_MAX (stops[ix].color[3]);
      return;
    }

  printf ("WARNING! bad ix %d in calc_color_at() [internal error]\n", ix);
  assert (0);
}

static void
art_render_gradient_linear_render_8 (ArtRenderCallback *self,
				     ArtRender *render,
				     art_u8 *dest, int y)
{
  ArtImageSourceGradLin *z = (ArtImageSourceGradLin *)self;
  const ArtGradientLinear *gradient = z->gradient;
  int i;
  int width = render->x1 - render->x0;
  int len;
  double offset, d_offset;
  double offset_fraction;
  int next_stop;
  int ix;
  art_u8 color1[4], color2[4];
  int n_stops = gradient->n_stops;
  int extra_stops;
  ArtGradientStop *stops = gradient->stops;
  ArtGradientStop *tmp_stops;
  art_u8 *bufp = render->image_buf;
  ArtGradientSpread spread = gradient->spread;

#ifdef DEBUG_SPEW
  printf ("x1: %d, x2: %d, y: %d\n", render->x0, render->x1, y);
  printf ("spread: %d, stops:", gradient->spread);
  for (i=0;i<n_stops;i++)
    {
      printf ("%f, ", gradient->stops[i].offset);
    }
  printf ("\n");
#endif
  
  offset = render->x0 * gradient->a + y * gradient->b + gradient->c;
  d_offset = gradient->a;

  /* We need to force the gradient to extend the whole 0..1 segment,
     because the rest of the code doesn't handle partial gradients
     correctly */
  if ((gradient->stops[0].offset != 0.0) ||
      (gradient->stops[n_stops-1].offset != 1.0))
  {
    extra_stops = 0;
    tmp_stops = stops = alloca (sizeof (ArtGradientStop) * (n_stops + 2));
    if (gradient->stops[0].offset != 0.0)
      {
	memcpy (tmp_stops, gradient->stops, sizeof (ArtGradientStop));
	tmp_stops[0].offset = 0.0;
	tmp_stops += 1;
	extra_stops++;
      }
    memcpy (tmp_stops, gradient->stops, sizeof (ArtGradientStop) * n_stops);
    if (gradient->stops[n_stops-1].offset != 1.0)
      {
	tmp_stops += n_stops;
	memcpy (tmp_stops, &gradient->stops[n_stops-1], sizeof (ArtGradientStop));
	tmp_stops[0].offset = 1.0;
	extra_stops++;
      }
    n_stops += extra_stops;


#ifdef DEBUG_SPEW
    printf ("start/stop modified stops:");
    for (i=0;i<n_stops;i++)
      {
	printf ("%f, ", stops[i].offset);
      }
    printf ("\n");
#endif

  }
  
  if (spread == ART_GRADIENT_REFLECT)
    {
      tmp_stops = stops;
      stops = alloca (sizeof (ArtGradientStop) * n_stops * 2);
      memcpy (stops, tmp_stops, sizeof (ArtGradientStop) * n_stops);

      for (i = 0; i< n_stops; i++)
	{
	  stops[n_stops * 2 - 1 - i].offset = (1.0 - stops[i].offset / 2.0);
	  memcpy (stops[n_stops * 2 - 1 - i].color, stops[i].color, sizeof (stops[i].color));
	  stops[i].offset = stops[i].offset / 2.0;
	}
      
      spread = ART_GRADIENT_REPEAT;
      offset = offset / 2.0;
      d_offset = d_offset / 2.0;
      
      n_stops = 2 * n_stops;

#ifdef DEBUG_SPEW
      printf ("reflect modified stops:");
      for (i=0;i<n_stops;i++)
	{
	  printf ("%f, ", stops[i].offset);
	}
      printf ("\n");
#endif
    }

  offset_fraction = offset - floor (offset);
#ifdef DEBUG_SPEW
  printf ("inital offset: %f, fraction: %f d_offset: %f\n", offset, offset_fraction, d_offset);
#endif
  /* ix is selected so that offset_fraction is
     stops[ix-1] <= offset_fraction <= stops[x1]
     If offset_fraction is equal to one of the edges, ix
     is selected so the the section of the line extending
     in the same direction as d_offset is between ix-1 and ix.
  */
  for (ix = 0; ix < n_stops; ix++)
    if (stops[ix].offset > offset_fraction ||
	(d_offset < 0.0 && stops[ix].offset == offset_fraction))
      break;
  if (ix == 0)
    ix = n_stops - 1;

#ifdef DEBUG_SPEW
  printf ("Initial ix: %d\n", ix);
#endif
  
  assert (ix > 0);
  assert (ix < n_stops);
  assert ((stops[ix-1].offset <= offset_fraction) ||
	  ((stops[ix].offset == 1.0) && (offset_fraction == 0.0)));
  assert (offset_fraction <= stops[ix].offset);
  assert ((offset_fraction != stops[ix-1].offset) ||
	  (d_offset >= 0.0));
  assert ((offset_fraction != stops[ix].offset) ||
	  (d_offset <= 0.0));
  
  while (width > 0)
    {
#ifdef DEBUG_SPEW
      printf ("ix: %d\n", ix);
      printf ("start offset: %f\n", offset);
#endif
      calc_color_at (stops, n_stops,
		     spread,
		     offset,
		     offset_fraction,
		     (d_offset > 0.0),
		     ix,
		     color1);

      if (d_offset > 0)
	next_stop = ix;
      else
	next_stop = ix-1;

#ifdef DEBUG_SPEW
      printf ("next_stop: %d\n", next_stop);
#endif
      if (fabs (d_offset) > EPSILON)
	{
	  double o;
	  o = offset_fraction;
	  
	  if ((fabs (o) <= EPSILON) && (ix == n_stops - 1))
	    o = 1.0;
	  else if ((fabs (o-1.0) <= EPSILON) && (ix == 1))
	    o = 0.0;

#ifdef DEBUG_SPEW
	  printf ("o: %f\n", o);
#endif
	  len = (int)floor (fabs ((stops[next_stop].offset - o) / d_offset)) + 1;
	  len = MAX (len, 0);
	  len = MIN (len, width);
	}
      else
	{
	  len = width;
	}
#ifdef DEBUG_SPEW
      printf ("len: %d\n", len);
#endif
      if (len > 0)
	{
	  offset = offset + (len-1) * d_offset;
	  offset_fraction = offset - floor (offset);
#ifdef DEBUG_SPEW
	  printf ("end offset: %f, fraction: %f\n", offset, offset_fraction);
#endif	  
	  calc_color_at (stops, n_stops,
			 spread,
			 offset,
			 offset_fraction,
			 (d_offset < 0.0),
			 ix,
			 color2);
	  
	  art_rgba_gradient_run (bufp,
				 color1,
				 color2,
				 len);
	  offset += d_offset;
	  offset_fraction = offset - floor (offset);
	}

      if (d_offset > 0)
	{
	  do
	    {
	      ix++;
	      if (ix == n_stops)
		ix = 1;
	      /* Note: offset_fraction can actually be one here on x86 machines that
		 does calculations with extanded precision, but later rounds to 64bit.
		 This happens if the 80bit offset_fraction is larger than the
		 largest 64bit double that is less than one.
	      */
	    }
	  while (!((stops[ix-1].offset <= offset_fraction &&
		   offset_fraction < stops[ix].offset) ||
		   (ix == 1 && offset_fraction == 1.0))); 
	}
      else
	{
	  do
	    {
	      ix--;
	      if (ix == 0)
		ix = n_stops - 1;
	    }
	  while (!((stops[ix-1].offset < offset_fraction &&
		    offset_fraction <= stops[ix].offset) ||
		   (ix == n_stops - 1 && offset_fraction == 0.0)));
	}
      
      bufp += 4*len;
      width -= len;
    }
}


/**
 * art_render_gradient_setpix: Set a gradient pixel.
 * @render: The render object.
 * @dst: Pointer to destination (where to store pixel).
 * @n_stops: Number of stops in @stops.
 * @stops: The stops for the gradient.
 * @offset: The offset.
 *
 * @n_stops must be > 0.
 *
 * Sets a gradient pixel, storing it at @dst.
 **/
static void
art_render_gradient_setpix (ArtRender *render,
			    art_u8 *dst,
			    int n_stops, ArtGradientStop *stops,
			    double offset)
{
  int ix;
  int j;
  double off0, off1;
  int n_ch = render->n_chan + 1;

  for (ix = 0; ix < n_stops; ix++)
    if (stops[ix].offset > offset)
      break;
  /* stops[ix - 1].offset < offset < stops[ix].offset */
  if (ix > 0 && ix < n_stops)
    {
      off0 = stops[ix - 1].offset;
      off1 = stops[ix].offset;
      if (fabs (off1 - off0) > EPSILON)
	{
	  double interp;

	  interp = (offset - off0) / (off1 - off0);
	  for (j = 0; j < n_ch; j++)
	    {
	      int z0, z1;
	      int z;
	      z0 = stops[ix - 1].color[j];
	      z1 = stops[ix].color[j];
	      z = floor (z0 + (z1 - z0) * interp + 0.5);
	      if (render->buf_depth == 8)
		dst[j] = ART_PIX_8_FROM_MAX (z);
	      else /* (render->buf_depth == 16) */
		((art_u16 *)dst)[j] = z;
	    }
	  return;
	}
    }
  else if (ix == n_stops)
    ix--;

  for (j = 0; j < n_ch; j++)
    {
      int z;
      z = stops[ix].color[j];
      if (render->buf_depth == 8)
	dst[j] = ART_PIX_8_FROM_MAX (z);
      else /* (render->buf_depth == 16) */
	((art_u16 *)dst)[j] = z;
    }
}

static void
art_render_gradient_linear_done (ArtRenderCallback *self, ArtRender *render)
{
  art_free (self);
}

static void
art_render_gradient_linear_render (ArtRenderCallback *self, ArtRender *render,
				   art_u8 *dest, int y)
{
  ArtImageSourceGradLin *z = (ArtImageSourceGradLin *)self;
  const ArtGradientLinear *gradient = z->gradient;
  int pixstride = (render->n_chan + 1) * (render->depth >> 3);
  int x;
  int width = render->x1 - render->x0;
  double offset, d_offset;
  double actual_offset;
  int n_stops = gradient->n_stops;
  ArtGradientStop *stops = gradient->stops;
  art_u8 *bufp = render->image_buf;
  ArtGradientSpread spread = gradient->spread;

  offset = render->x0 * gradient->a + y * gradient->b + gradient->c;
  d_offset = gradient->a;

  for (x = 0; x < width; x++)
    {
      if (spread == ART_GRADIENT_PAD)
	actual_offset = offset;
      else if (spread == ART_GRADIENT_REPEAT)
	actual_offset = offset - floor (offset);
      else /* (spread == ART_GRADIENT_REFLECT) */
	{
	  double tmp;

	  tmp = offset - 2 * floor (0.5 * offset);
	  actual_offset = tmp > 1 ? 2 - tmp : tmp;
	}
      art_render_gradient_setpix (render, bufp, n_stops, stops, actual_offset);
      offset += d_offset;
      bufp += pixstride;
    }
}

static void
art_render_gradient_linear_negotiate (ArtImageSource *self, ArtRender *render,
				      ArtImageSourceFlags *p_flags,
				      int *p_buf_depth, ArtAlphaType *p_alpha)
{
  if (render->depth == 8 &&
      render->n_chan == 3)
    {
      self->super.render = art_render_gradient_linear_render_8;
      *p_flags = 0;
      *p_buf_depth = 8;
      *p_alpha = ART_ALPHA_PREMUL;
      return;
    }
  
  self->super.render = art_render_gradient_linear_render;
  *p_flags = 0;
  *p_buf_depth = render->depth;
  *p_alpha = ART_ALPHA_PREMUL;
}

/**
 * art_render_gradient_linear: Add a linear gradient image source.
 * @render: The render object.
 * @gradient: The linear gradient.
 *
 * Adds the linear gradient @gradient as the image source for rendering
 * in the render object @render.
 **/
void
art_render_gradient_linear (ArtRender *render,
			    const ArtGradientLinear *gradient,
			    ArtFilterLevel level)
{
  ArtImageSourceGradLin *image_source = art_new (ArtImageSourceGradLin, 1);

  image_source->super.super.render = NULL;
  image_source->super.super.done = art_render_gradient_linear_done;
  image_source->super.negotiate = art_render_gradient_linear_negotiate;

  image_source->gradient = gradient;

  art_render_add_image_source (render, &image_source->super);
}

static void
art_render_gradient_radial_done (ArtRenderCallback *self, ArtRender *render)
{
  art_free (self);
}

static void
art_render_gradient_radial_render (ArtRenderCallback *self, ArtRender *render,
				   art_u8 *dest, int y)
{
  ArtImageSourceGradRad *z = (ArtImageSourceGradRad *)self;
  const ArtGradientRadial *gradient = z->gradient;
  int pixstride = (render->n_chan + 1) * (render->depth >> 3);
  int x;
  int x0 = render->x0;
  int width = render->x1 - x0;
  int n_stops = gradient->n_stops;
  ArtGradientStop *stops = gradient->stops;
  art_u8 *bufp = render->image_buf;
  double fx = gradient->fx;
  double fy = gradient->fy;
  double dx, dy;
  double *affine = gradient->affine;
  double aff0 = affine[0];
  double aff1 = affine[1];
  const double a = z->a;
  const double arecip = 1.0 / a;
  double b, db;
  double c, dc, ddc;
  double b_a, db_a;
  double rad, drad, ddrad;

  dx = x0 * aff0 + y * affine[2] + affine[4] - fx;
  dy = x0 * aff1 + y * affine[3] + affine[5] - fy;
  b = dx * fx + dy * fy;
  db = aff0 * fx + aff1 * fy;
  c = dx * dx + dy * dy;
  dc = 2 * aff0 * dx + aff0 * aff0 + 2 * aff1 * dy + aff1 * aff1;
  ddc = 2 * aff0 * aff0 + 2 * aff1 * aff1;

  b_a = b * arecip;
  db_a = db * arecip;

  rad = b_a * b_a + c * arecip;
  drad = 2 * b_a * db_a + db_a * db_a + dc * arecip;
  ddrad = 2 * db_a * db_a + ddc * arecip;

  for (x = 0; x < width; x++)
    {
      double z;

      if (rad > 0)
	z = b_a + sqrt (rad);
      else
	z = b_a;
      art_render_gradient_setpix (render, bufp, n_stops, stops, z);
      bufp += pixstride;
      b_a += db_a;
      rad += drad;
      drad += ddrad;
    }
}

static void
art_render_gradient_radial_negotiate (ArtImageSource *self, ArtRender *render,
				      ArtImageSourceFlags *p_flags,
				      int *p_buf_depth, ArtAlphaType *p_alpha)
{
  self->super.render = art_render_gradient_radial_render;
  *p_flags = 0;
  *p_buf_depth = render->depth;
  *p_alpha = ART_ALPHA_PREMUL;
}

/**
 * art_render_gradient_radial: Add a radial gradient image source.
 * @render: The render object.
 * @gradient: The radial gradient.
 *
 * Adds the radial gradient @gradient as the image source for rendering
 * in the render object @render.
 **/
void
art_render_gradient_radial (ArtRender *render,
			    const ArtGradientRadial *gradient,
			    ArtFilterLevel level)
{
  ArtImageSourceGradRad *image_source = art_new (ArtImageSourceGradRad, 1);
  double fx = gradient->fx;
  double fy = gradient->fy;

  image_source->super.super.render = NULL;
  image_source->super.super.done = art_render_gradient_radial_done;
  image_source->super.negotiate = art_render_gradient_radial_negotiate;

  image_source->gradient = gradient;
  /* todo: sanitycheck fx, fy? */
  image_source->a = 1 - fx * fx - fy * fy;

  art_render_add_image_source (render, &image_source->super);
}
