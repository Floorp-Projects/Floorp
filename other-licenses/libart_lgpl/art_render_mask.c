/*
 * art_render_mask.c: Alpha mask source for modular rendering.
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
 */

#include "config.h"
#include "art_render_mask.h"

#include <string.h>


typedef struct _ArtMaskSourceMask ArtMaskSourceMask;

struct _ArtMaskSourceMask {
  ArtMaskSource super;
  ArtRender *render;
  art_boolean first;
  int x0;
  int y0;
  int x1;
  int y1;
  const art_u8 *mask_buf;
  int rowstride;
};

static void
art_render_mask_done (ArtRenderCallback *self, ArtRender *render)
{
  art_free (self);
}

static int
art_render_mask_can_drive (ArtMaskSource *self, ArtRender *render)
{
  return 0;
}

static void
art_render_mask_render (ArtRenderCallback *self, ArtRender *render,
			art_u8 *dest, int y)
{
  ArtMaskSourceMask *z = (ArtMaskSourceMask *)self;
  int x0 = render->x0, x1 = render->x1;
  int z_x0 = z->x0, z_x1 = z->x1;
  int width = x1 - x0;
  int z_width = z_x1 - z_x0;
  art_u8 *alpha_buf = render->alpha_buf;

  if (y < z->y0 || y >= z->y1 || z_width <= 0)
    memset (alpha_buf, 0, width);
  else
    {
      const art_u8 *src_line = z->mask_buf + (y - z->y0) * z->rowstride;
      art_u8 *dst_line = alpha_buf + z_x0 - x0;

      if (z_x0 > x0)
	memset (alpha_buf, 0, z_x0 - x0);
      
      if (z->first)
	memcpy (dst_line, src_line, z_width);
      else
	{
	  int x;
	  
	  for (x = 0; x < z_width; x++)
	    {
	      int v;
	      v = src_line[x];
	      if (v)
		{
		  v = v * dst_line[x] + 0x80;
		  v = (v + (v >> 8)) >> 8;
		  dst_line[x] = v;
		}
	      else
		{
		  dst_line[x] = 0;
		}
	    }
	}
      
      if (z_x1 < x1)
	memset (alpha_buf + z_x1 - x0, 0, x1 - z_x1);
    }
}

static void
art_render_mask_prepare (ArtMaskSource *self, ArtRender *render,
			 art_boolean first)
{
  ArtMaskSourceMask *z = (ArtMaskSourceMask *)self;
  self->super.render = art_render_mask_render;
  z->first = first;
}

/**
 * art_render_mask: Use an alpha buffer as a render mask source.
 * @render: Render object.
 * @x0: Left coordinate of mask rect.
 * @y0: Top coordinate of mask rect.
 * @x1: Right coordinate of mask rect.
 * @y1: Bottom coordinate of mask rect.
 * @mask_buf: Buffer containing 8bpp alpha mask data.
 * @rowstride: Rowstride of @mask_buf.
 *
 * Adds @mask_buf to the render object as a mask. Note: @mask_buf must
 * remain allocated until art_render_invoke() is called on @render.
 **/
void
art_render_mask (ArtRender *render,
		 int x0, int y0, int x1, int y1,
		 const art_u8 *mask_buf, int rowstride)
{
  ArtMaskSourceMask *mask_source;

  if (x0 < render->x0)
    {
      mask_buf += render->x0 - x0;
      x0 = render->x0;
    }
  if (x1 > render->x1)
    x1 = render->x1;

  if (y0 < render->y0)
    {
      mask_buf += (render->y0 - y0) * rowstride;
      y0 = render->y0;
    }
  if (y1 > render->y1)
    y1 = render->y1;

  mask_source = art_new (ArtMaskSourceMask, 1);

  mask_source->super.super.render = NULL;
  mask_source->super.super.done = art_render_mask_done;
  mask_source->super.can_drive = art_render_mask_can_drive;
  mask_source->super.invoke_driver = NULL;
  mask_source->super.prepare = art_render_mask_prepare;
  mask_source->render = render;
  mask_source->x0 = x0;
  mask_source->y0 = y0;
  mask_source->x1 = x1;
  mask_source->y1 = y1;
  mask_source->mask_buf = mask_buf;
  mask_source->rowstride = rowstride;

  art_render_add_mask_source (render, &mask_source->super);

}
