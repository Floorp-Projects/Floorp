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

#include "config.h"
#include "art_rect_svp.h"

#include "art_misc.h"
#include "art_svp.h"
#include "art_rect.h"

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif /* MAX */

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif /* MIN */

/**
 * art_drect_svp: Find the bounding box of a sorted vector path.
 * @bbox: Where to store the bounding box.
 * @svp: The SVP.
 *
 * Finds the bounding box of the SVP.
 **/
void
art_drect_svp (ArtDRect *bbox, const ArtSVP *svp)
{
  int i;

  if (svp->n_segs == 0)
    {
  bbox->x0 = 0;
  bbox->y0 = 0;
  bbox->x1 = 0;
  bbox->y1 = 0;
      return;
    }

  art_drect_copy (bbox, &svp->segs[0].bbox);

  for (i = 1; i < svp->n_segs; i++)
    {
      bbox->x0 = MIN (bbox->x0, svp->segs[i].bbox.x0);
      bbox->y0 = MIN (bbox->y0, svp->segs[i].bbox.y0);
      bbox->x1 = MAX (bbox->x1, svp->segs[i].bbox.x1);
      bbox->y1 = MAX (bbox->y1, svp->segs[i].bbox.y1);
    }
}

/**
 * art_drect_svp_union: Compute the bounding box of the svp and union it in to the existing bounding box.
 * @bbox: Initial boundin box and where to store the bounding box.
 * @svp: The SVP.
 *
 * Finds the bounding box of the SVP, computing its union with an
 * existing bbox.
 **/
void
art_drect_svp_union (ArtDRect *bbox, const ArtSVP *svp)
{
  ArtDRect svp_bbox;

  art_drect_svp (&svp_bbox, svp);
  art_drect_union (bbox, bbox, &svp_bbox);
}
