/* 
 * art_vpath_filters.h: Filters for Libart_LGPL ArtVpaths
 *
 * Copyright (c)2002 Crocodile Clips Ltd.
 *
 * Author: Alex Fritze <alex@croczilla.com>
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

#ifndef __ART_VPATH_FILTERS_H__
#define __ART_VPATH_FILTERS_H__

/*
  This code defines a stack-based filter architecture for vpaths. The
  collection includes sources, sinks, clipping filters and dashing
  filters. The filters are defined as subclasses of an abstract
  ArtVpathIterator and can be pipelined without the need for
  intermediate storage.

  Usage:

  To set up a pipeline that clips a aVpath to aRect and dashes it
  using aDashDescription,

  +------+   +-------------+   +----------+   +----------+   +-------+
  |aVpath|-->|arrayiterator|-->|clipfilter|-->|dashfilter|-->|aResult|
  +------+   +-------------+   +----------+   +----------+   +-------+

  the following code can be used:

   
    ArtVpathArrayIterator src_iter;
    ArtVpathClipFilter clip_filter;
    ArtVpathDashFilter dash_filter;
      
    art_vpath_array_iterator_init(aVpath, &src_iter);
    art_vpath_clip_filter_init((ArtVpathIterator*)&src_iter,
                               &aRect,
                               &clip_filter);
    art_vpath_dash_filter_init((ArtVpathIterator*)&clip_filter,
                               &aDashDescription,
                               &dash_filter);
      
    aResult = art_vpath_new_vpath_array((ArtVpathIterator*)&dash_filter);
    
*/


#ifdef LIBART_COMPILATION
#include "art_vpath.h"
#include "art_rect.h"
#include "art_pathcode.h"
#include "art_vpath_dash.h"
#else
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_pathcode.h>
#include <libart_lgpl/art_vpath_dash.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ArtVpathIterator: abstract base class implemented by all filters
 */

typedef struct _ArtVpathIterator ArtVpathIterator;

struct _ArtVpathIterator {
  ArtVpath *(*current)(ArtVpathIterator *instance);
  void (*next)(ArtVpathIterator *instance);
};

  
/* art_vpath_new_vpath_array: generate a heap-based vpath array from
 * the given pipeline
 */  

ArtVpath *art_vpath_new_vpath_array(ArtVpathIterator *src);
  

/* ArtVpathArrayIterator: an iterator for vpath arrays
 */

typedef struct _ArtVpathArrayIterator ArtVpathArrayIterator;
  
struct _ArtVpathArrayIterator {
  ArtVpathIterator super;
  ArtVpath *cursor;
};

void art_vpath_array_iterator_init (ArtVpath *arr,
                                    ArtVpathArrayIterator *instance);


  
/* ArtVpathClipFilter: a filter that clips src_iter to drect. Clipped
 * line-segments get the pathcode ART_LINE_CLIPPED.
 */
  
typedef struct _ArtVpathClipFilter ArtVpathClipFilter;

struct _ArtVpathClipFilter {
  ArtVpathIterator super;  
  ArtVpathIterator *src;
  const ArtDRect *cliprect;
  ArtVpath segment_stack[3];
  int segment_stack_pointer;
};

void art_vpath_clip_filter_init (ArtVpathIterator *src_iter,
                                 const ArtDRect *drect,
                                 ArtVpathClipFilter *instance);


/* ArtVpathPolyRectClipFilter: a filter that clips the polygon
 * described by src_iter to the upright rect given by drect
 */

/* helper filter to clip a polygon to one upright edge */
typedef struct _ArtVpathPolyUpEdgeClipFilter ArtVpathPolyUpEdgeClipFilter;

struct _ArtVpathPolyUpEdgeClipFilter {
  ArtVpathIterator super;
  ArtVpathIterator *src;
  double edge_coord;
  void (*intersect)(double x1, double y1,
                    double x2, double y2,
                    ArtVpathPolyUpEdgeClipFilter *_this);
  int (*inside)(ArtVpathPolyUpEdgeClipFilter *_this); 
  double startx,starty;
  ArtVpath current_element;
};

  
typedef struct _ArtVpathPolyRectClipFilter ArtVpathPolyRectClipFilter;
  
struct _ArtVpathPolyRectClipFilter { 
  ArtVpathPolyUpEdgeClipFilter edge_clippers[4];
};

void art_vpath_poly_rect_clip_filter_init (ArtVpathIterator *src_iter,
                                           const ArtDRect *drect,
                                           ArtVpathPolyRectClipFilter *instance);

  
/* ArtVpathContractFilter: a filter that contracts a vpath by
 * replacing consecutive src_iter segments of type src_pathcode with a
 * single dst_pathcode. Can e.g. be used to construct a closed or
 * opened path from a src_iter that contains ART_LINE_CLIPPED segments
 */

typedef struct _ArtVpathContractFilter ArtVpathContractFilter;

struct _ArtVpathContractFilter {
  ArtVpathIterator super;
  ArtVpathIterator *src;
  ArtPathcode src_pathcode;
  ArtPathcode dst_pathcode;
  ArtVpath current_element;
};

void art_vpath_contract_filter_init (ArtVpathIterator *src_iter,
                                     ArtPathcode src_pathcode,
                                     ArtPathcode dst_pathcode,
                                     ArtVpathContractFilter *instance);


  
/* ArtVpathDashFilter: a filter that dashes src_iter using
 * dasharray. Only ART_LINE segements are dashed. Both ART_LINE and
 * ART_LINE_CLIPPED segements contribute to the path length used in
 * the dash calculations.*/
  
typedef struct _ArtVpathDashFilter ArtVpathDashFilter;

/* internal data structure keeping track of position into dash array */
typedef struct _ArtVpathDashPointer ArtVpathDashPointer;

struct _ArtVpathDashPointer {
  ArtVpathDash *dasharray;
  double length; /* sum( dasharray->dash[i]) */
  
  int index; /* where we currently are in the dash array */
  double offset; /* offset into dasharray->dash[dash_index] */
  int is_dash; /* true == current dash array element is a dash. gap otherwise */
};  
  
struct _ArtVpathDashFilter {
  ArtVpathIterator super;

  ArtVpathIterator *src;  
  double x0, y0; /* start point of current source segment */
  double dx, dy; /* unit vector in direction of current source segment */
  double dist0, dist1; /* start, end distance of current segment */
  double dist; /* current into whole path distance */
  ArtVpathDashPointer dashpointer; /* pointer into dasharray */
  ArtVpath current_element; 
};

void art_vpath_dash_filter_init (ArtVpathIterator *src_iter,
                                 ArtVpathDash *dasharray,
                                 ArtVpathDashFilter *instance);

  
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_VPATH_FILTERS_H__ */
