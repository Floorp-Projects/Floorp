/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 2001 Raph Levien
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

#ifndef __ART_SVP_INTERSECT_H__
#define __ART_SVP_INTERSECT_H__

/* The funky new SVP intersector. */

#ifdef LIBART_COMPILATION
#include "art_svp.h"
#else
#include <libart_lgpl/art_svp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef ART_WIND_RULE_DEFINED
#define ART_WIND_RULE_DEFINED
typedef enum {
  ART_WIND_RULE_NONZERO,
  ART_WIND_RULE_INTERSECT,
  ART_WIND_RULE_ODDEVEN,
  ART_WIND_RULE_POSITIVE
} ArtWindRule;
#endif

typedef struct _ArtSvpWriter ArtSvpWriter;

struct _ArtSvpWriter {
  int (*add_segment) (ArtSvpWriter *self, int wind_left, int delta_wind,
		      double x, double y);
  void (*add_point) (ArtSvpWriter *self, int seg_id, double x, double y);
  void (*close_segment) (ArtSvpWriter *self, int seg_id);
};

ArtSvpWriter *
art_svp_writer_rewind_new (ArtWindRule rule);

ArtSVP *
art_svp_writer_rewind_reap (ArtSvpWriter *self);

int
art_svp_seg_compare (const void *s1, const void *s2);

void
art_svp_intersector (const ArtSVP *in, ArtSvpWriter *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_SVP_INTERSECT_H__ */
