/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998-2000 Raph Levien
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

/* The spiffy antialiased renderer for sorted vector paths. */

#include "config.h"
#include "art_svp_render_aa.h"

#include <math.h>
#include <string.h> /* for memmove */
#include "art_misc.h"

#include "art_rect.h"
#include "art_svp.h"

#include <stdio.h>

typedef double artfloat;

struct _ArtSVPRenderAAIter {
  const ArtSVP *svp;
  int x0, x1;
  int y;
  int seg_ix;

  int *active_segs;
  int n_active_segs;
  int *cursor;
  artfloat *seg_x;
  artfloat *seg_dx;

  ArtSVPRenderAAStep *steps;
};

static void
art_svp_render_insert_active (int i, int *active_segs, int n_active_segs,
			      artfloat *seg_x, artfloat *seg_dx)
{
  int j;
  artfloat x;
  int tmp1, tmp2;

  /* this is a cheap hack to get ^'s sorted correctly */
  x = seg_x[i] + 0.001 * seg_dx[i];
  for (j = 0; j < n_active_segs && seg_x[active_segs[j]] < x; j++);

  tmp1 = i;
  while (j < n_active_segs)
    {
      tmp2 = active_segs[j];
      active_segs[j] = tmp1;
      tmp1 = tmp2;
      j++;
    }
  active_segs[j] = tmp1;
}

static void
art_svp_render_delete_active (int *active_segs, int j, int n_active_segs)
{
  int k;

  for (k = j; k < n_active_segs; k++)
    active_segs[k] = active_segs[k + 1];
}

#define EPSILON 1e-6

/* Render the sorted vector path in the given rectangle, antialiased.

   This interface uses a callback for the actual pixel rendering. The
   callback is called y1 - y0 times (once for each scan line). The y
   coordinate is given as an argument for convenience (it could be
   stored in the callback's private data and incremented on each
   call).

   The rendered polygon is represented in a semi-runlength format: a
   start value and a sequence of "steps". Each step has an x
   coordinate and a value delta. The resulting value at position x is
   equal to the sum of the start value and all step delta values for
   which the step x coordinate is less than or equal to x. An
   efficient algorithm will traverse the steps left to right, keeping
   a running sum.

   All x coordinates in the steps are guaranteed to be x0 <= x < x1.
   (This guarantee is a change from the gfonted vpaar renderer, and is
   designed to simplify the callback).

   There is now a further guarantee that no two steps will have the
   same x value. This may allow for further speedup and simplification
   of renderers.

   The value 0x8000 represents 0% coverage by the polygon, while
   0xff8000 represents 100% coverage. This format is designed so that
   >> 16 results in a standard 0x00..0xff value range, with nice
   rounding.

   Status of this routine:

   Basic correctness: OK

   Numerical stability: pretty good, although probably not
   bulletproof.

   Speed: Needs more aggressive culling of bounding boxes.  Can
   probably speed up the [x0,x1) clipping of step values.  Can do more
   of the step calculation in fixed point.

   Precision: No known problems, although it should be tested
   thoroughly, especially for symmetry.

*/

ArtSVPRenderAAIter *
art_svp_render_aa_iter (const ArtSVP *svp,
			int x0, int y0, int x1, int y1)
{
  ArtSVPRenderAAIter *iter = art_new (ArtSVPRenderAAIter, 1);

  iter->svp = svp;
  iter->y = y0;
  iter->x0 = x0;
  iter->x1 = x1;
  iter->seg_ix = 0;

  iter->active_segs = art_new (int, svp->n_segs);
  iter->cursor = art_new (int, svp->n_segs);
  iter->seg_x = art_new (artfloat, svp->n_segs);
  iter->seg_dx = art_new (artfloat, svp->n_segs);
  iter->steps = art_new (ArtSVPRenderAAStep, x1 - x0);
  iter->n_active_segs = 0;

  return iter;
}

#define ADD_STEP(xpos, xdelta)                          \
  /* stereotype code fragment for adding a step */      \
  if (n_steps == 0 || steps[n_steps - 1].x < xpos)      \
    {                                                   \
      sx = n_steps;                                     \
      steps[sx].x = xpos;                               \
      steps[sx].delta = xdelta;                         \
      n_steps++;                                        \
    }                                                   \
  else                                                  \
    {                                                   \
      for (sx = n_steps; sx > 0; sx--)                  \
	{                                               \
	  if (steps[sx - 1].x == xpos)                  \
	    {                                           \
	      steps[sx - 1].delta += xdelta;            \
	      sx = n_steps;                             \
	      break;                                    \
	    }                                           \
	  else if (steps[sx - 1].x < xpos)              \
	    {                                           \
	      break;                                    \
	    }                                           \
	}                                               \
      if (sx < n_steps)                                 \
	{                                               \
	  memmove (&steps[sx + 1], &steps[sx],          \
		   (n_steps - sx) * sizeof(steps[0]));  \
	  steps[sx].x = xpos;                           \
	  steps[sx].delta = xdelta;                     \
	  n_steps++;                                    \
	}                                               \
    }

void
art_svp_render_aa_iter_step (ArtSVPRenderAAIter *iter, int *p_start,
			     ArtSVPRenderAAStep **p_steps, int *p_n_steps)
{
  const ArtSVP *svp = iter->svp;
  int *active_segs = iter->active_segs;
  int n_active_segs = iter->n_active_segs;
  int *cursor = iter->cursor;
  artfloat *seg_x = iter->seg_x;
  artfloat *seg_dx = iter->seg_dx;
  int i = iter->seg_ix;
  int j;
  int x0 = iter->x0;
  int x1 = iter->x1;
  int y = iter->y;
  int seg_index;

  int x;
  ArtSVPRenderAAStep *steps = iter->steps;
  int n_steps;
  artfloat y_top, y_bot;
  artfloat x_top, x_bot;
  artfloat x_min, x_max;
  int ix_min, ix_max;
  artfloat delta; /* delta should be int too? */
  int last, this;
  int xdelta;
  artfloat rslope, drslope;
  int start;
  const ArtSVPSeg *seg;
  int curs;
  artfloat dy;

  int sx;
  
  /* insert new active segments */
  for (; i < svp->n_segs && svp->segs[i].bbox.y0 < y + 1; i++)
    {
      if (svp->segs[i].bbox.y1 > y &&
	  svp->segs[i].bbox.x0 < x1)
	{
	  seg = &svp->segs[i];
	  /* move cursor to topmost vector which overlaps [y,y+1) */
	  for (curs = 0; seg->points[curs + 1].y < y; curs++);
	  cursor[i] = curs;
	  dy = seg->points[curs + 1].y - seg->points[curs].y;
	  if (fabs (dy) >= EPSILON)
	    seg_dx[i] = (seg->points[curs + 1].x - seg->points[curs].x) /
	      dy;
	  else
	    seg_dx[i] = 1e12;
	  seg_x[i] = seg->points[curs].x +
	    (y - seg->points[curs].y) * seg_dx[i];
	  art_svp_render_insert_active (i, active_segs, n_active_segs++,
					seg_x, seg_dx);
	}
    }

  n_steps = 0;

  /* render the runlengths, advancing and deleting as we go */
  start = 0x8000;

  for (j = 0; j < n_active_segs; j++)
    {
      seg_index = active_segs[j];
      seg = &svp->segs[seg_index];
      curs = cursor[seg_index];
      while (curs != seg->n_points - 1 &&
	     seg->points[curs].y < y + 1)
	{
	  y_top = y;
	  if (y_top < seg->points[curs].y)
	    y_top = seg->points[curs].y;
	  y_bot = y + 1;
	  if (y_bot > seg->points[curs + 1].y)
	    y_bot = seg->points[curs + 1].y;
	  if (y_top != y_bot) {
	    delta = (seg->dir ? 16711680.0 : -16711680.0) *
	      (y_bot - y_top);
	    x_top = seg_x[seg_index] + (y_top - y) * seg_dx[seg_index];
	    x_bot = seg_x[seg_index] + (y_bot - y) * seg_dx[seg_index];
	    if (x_top < x_bot)
	      {
		x_min = x_top;
		x_max = x_bot;
	      }
	    else
	      {
		x_min = x_bot;
		x_max = x_top;
	      }
	    ix_min = floor (x_min);
	    ix_max = floor (x_max);
	    if (ix_min >= x1)
	      {
		/* skip; it starts to the right of the render region */
	      }
	    else if (ix_max < x0)
	      /* it ends to the left of the render region */
	      start += delta;
	    else if (ix_min == ix_max)
	      {
		/* case 1, antialias a single pixel */
		xdelta = (ix_min + 1 - (x_min + x_max) * 0.5) * delta;

		ADD_STEP(ix_min, xdelta)

		if (ix_min + 1 < x1)
		  {
		    xdelta = delta - xdelta;

		    ADD_STEP(ix_min + 1, xdelta)
		  }
	      }
	    else
	      {
		/* case 2, antialias a run */
		rslope = 1.0 / fabs (seg_dx[seg_index]);
		drslope = delta * rslope;
		last =
		  drslope * 0.5 *
		  (ix_min + 1 - x_min) * (ix_min + 1 - x_min);
		xdelta = last;
		if (ix_min >= x0)
		  {
		    ADD_STEP(ix_min, xdelta)
		    
		    x = ix_min + 1;
		  }
		else
		  {
		    start += last;
		    x = x0;
		  }
		if (ix_max > x1)
		  ix_max = x1;
		for (; x < ix_max; x++)
		  {
		    this = (seg->dir ? 16711680.0 : -16711680.0) * rslope *
		      (x + 0.5 - x_min);
		    xdelta = this - last;
		    last = this;

		    ADD_STEP(x, xdelta)
		  }
		if (x < x1)
		  {
		    this =
		      delta * (1 - 0.5 *
			       (x_max - ix_max) * (x_max - ix_max) *
			       rslope);
		    xdelta = this - last;
		    last = this;

		    ADD_STEP(x, xdelta)
		    
		    if (x + 1 < x1)
		      {
			xdelta = delta - last;

			ADD_STEP(x + 1, xdelta)
		      }
		  }
	      }
	  }
	  curs++;
	  if (curs != seg->n_points - 1 &&
	      seg->points[curs].y < y + 1)
	    {
	      dy = seg->points[curs + 1].y - seg->points[curs].y;
	      if (fabs (dy) >= EPSILON)
		seg_dx[seg_index] = (seg->points[curs + 1].x -
				     seg->points[curs].x) / dy;
	      else
		seg_dx[seg_index] = 1e12;
	      seg_x[seg_index] = seg->points[curs].x +
		(y - seg->points[curs].y) * seg_dx[seg_index];
	    }
	  /* break here, instead of duplicating predicate in while? */
	}
      if (seg->points[curs].y >= y + 1)
	{
	  curs--;
	  cursor[seg_index] = curs;
	  seg_x[seg_index] += seg_dx[seg_index];
	}
      else
	{
	  art_svp_render_delete_active (active_segs, j--,
					--n_active_segs);
	}
    }

  *p_start = start;
  *p_steps = steps;
  *p_n_steps = n_steps;

  iter->seg_ix = i;
  iter->n_active_segs = n_active_segs;
  iter->y++;
}

void
art_svp_render_aa_iter_done (ArtSVPRenderAAIter *iter)
{
  art_free (iter->steps);

  art_free (iter->seg_dx);
  art_free (iter->seg_x);
  art_free (iter->cursor);
  art_free (iter->active_segs);
  art_free (iter);
}

/**
 * art_svp_render_aa: Render SVP antialiased.
 * @svp: The #ArtSVP to render.
 * @x0: Left coordinate of destination rectangle.
 * @y0: Top coordinate of destination rectangle.
 * @x1: Right coordinate of destination rectangle.
 * @y1: Bottom coordinate of destination rectangle.
 * @callback: The callback which actually paints the pixels.
 * @callback_data: Private data for @callback.
 *
 * Renders the sorted vector path in the given rectangle, antialiased.
 *
 * This interface uses a callback for the actual pixel rendering. The
 * callback is called @y1 - @y0 times (once for each scan line). The y
 * coordinate is given as an argument for convenience (it could be
 * stored in the callback's private data and incremented on each
 * call).
 *
 * The rendered polygon is represented in a semi-runlength format: a
 * start value and a sequence of "steps". Each step has an x
 * coordinate and a value delta. The resulting value at position x is
 * equal to the sum of the start value and all step delta values for
 * which the step x coordinate is less than or equal to x. An
 * efficient algorithm will traverse the steps left to right, keeping
 * a running sum.
 *
 * All x coordinates in the steps are guaranteed to be @x0 <= x < @x1.
 * (This guarantee is a change from the gfonted vpaar renderer from
 * which this routine is derived, and is designed to simplify the
 * callback).
 *
 * The value 0x8000 represents 0% coverage by the polygon, while
 * 0xff8000 represents 100% coverage. This format is designed so that
 * >> 16 results in a standard 0x00..0xff value range, with nice
 * rounding.
 * 
 **/
void
art_svp_render_aa (const ArtSVP *svp,
		   int x0, int y0, int x1, int y1,
		   void (*callback) (void *callback_data,
				     int y,
				     int start,
				     ArtSVPRenderAAStep *steps, int n_steps),
		   void *callback_data)
{
  ArtSVPRenderAAIter *iter;
  int y;
  int start;
  ArtSVPRenderAAStep *steps;
  int n_steps;

  iter = art_svp_render_aa_iter (svp, x0, y0, x1, y1);


  for (y = y0; y < y1; y++)
    {
      art_svp_render_aa_iter_step (iter, &start, &steps, &n_steps);
      (*callback) (callback_data, y, start, steps, n_steps);
    }

  art_svp_render_aa_iter_done (iter);
}
