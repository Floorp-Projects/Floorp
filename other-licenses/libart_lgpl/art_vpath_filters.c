/*
 * art_vpath_filters.c: Filters for Libart_LGPL ArtVpaths
 *
 * Copyright (c)2002 Crocodile Clips Ltd.
 *
 * Author: Alex Fritze <alex@croczilla.com> (original author)
 *         Tim Dewhirst <tim.dewhirst@crocodile-clips.com>
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

#include <math.h>
#include <stdlib.h>
#include "art_misc.h"
#include "art_vpath_filters.h"

/**
 * art_vpath_new_vpath_array: Generate a heap-based ArtVpath-array from a vpath iterator
 * @src: Source Iterator
 * Return value: Newly allocated vpath
 **/
ArtVpath *art_vpath_new_vpath_array(ArtVpathIterator *src)
{
  ArtVpath *result;
  ArtVpath *current;
  int n_result, n_result_max;

  if (!src->current(src)) return 0;

  n_result = 0;
  n_result_max = 16;
  result = art_new (ArtVpath, n_result_max);

  while(1) {
    current =  src->current(src);
    if (!current) break;

    art_vpath_add_point (&result, &n_result, &n_result_max,
                         current->code, current->x, current->y);
    if (current->code == ART_END) break;
    src->next(src);
  }
  return result;
}


/************************************************************************/
/* array iterator */

/* virtual current() function for ArtVpathArrayIterator */
static ArtVpath *
_art_vpath_array_iterator_current (ArtVpathIterator *instance)
{
  return ((ArtVpathArrayIterator *)instance)->cursor;
}

/* virtual next() function for ArtVpathArrayIterator */
static void
_art_vpath_array_iterator_next (ArtVpathIterator *instance)
{
  if (((ArtVpathArrayIterator *)instance)->cursor &&
      ((ArtVpathArrayIterator *)instance)->cursor->code!=ART_END)
    ++(((ArtVpathArrayIterator *)instance)->cursor);
}

/**
 * art_vpath_array_iterator_init: Initialize an ArtVpathArrayIterator for the given ArtVpath-array
 * @arr: ArtVpath-array to iterate over
 * @instance: Iterator to initialize
 **/
void
art_vpath_array_iterator_init (ArtVpath *arr,
                               ArtVpathArrayIterator *instance)
{
  instance->super.current = &_art_vpath_array_iterator_current;
  instance->super.next = &_art_vpath_array_iterator_next;
  instance->cursor = arr;
}


/************************************************************************/
/* clip filter */

/* helper for Liang-Barsky clip algorithm */
static int
_art_vpath_clip_test(double p, double q, double *t1, double *t2)
{
  int result = 1;
  double r;

  if (p<0.0) {
    r = q/p;
    if (r>*t2)
      result = 0;
    else if (r>*t1)
      *t1 = r;
  }
  else if (p>0.0) {
    r = q/p;
    if (r<*t1)
      result = 0;
    else if (r<*t2)
      *t2 = r;
  }
  else if (q<0.0)
    result = 0;

  return result;
}

/* Liang-Barsky clipping implementation */
static void
_art_vpath_clip_do_next_segment(ArtVpathClipFilter *instance)
{
  ArtVpath current;
  double t1, t2, dx, dy;
  current = *(instance->src->current(instance->src));
  if (current.code == ART_END) return;

  instance->src->next(instance->src);
  if (instance->src->current(instance->src)->code != ART_LINETO) {
    /* no need to clip ART_MOVETO, ART_MOVETO_OPEN or ART_END;
       just push path onto stack*/
    instance->segment_stack[0] = *(instance->src->current(instance->src));
    instance->segment_stack_pointer = 0;
  }
  else {
    /* do a Liang-Barsky clip & push resulting segments onto stack */
    dx = instance->src->current(instance->src)->x - current.x;
    dy = instance->src->current(instance->src)->y - current.y;
    t1 = 0.0;
    t2 = 1.0;
    if (_art_vpath_clip_test(-dx, current.x - instance->cliprect->x0, &t1, &t2) &&
        _art_vpath_clip_test( dx, instance->cliprect->x1 - current.x, &t1, &t2) &&
        _art_vpath_clip_test(-dy, current.y - instance->cliprect->y0, &t1, &t2) &&
        _art_vpath_clip_test( dy, instance->cliprect->y1 - current.y, &t1, &t2)) {
      /* the line is at least partially within the cliprect; push
         segements onto stack in reverse order*/

      if (t2 < 1.0) {
        instance->segment_stack[0] = *(instance->src->current(instance->src));
        instance->segment_stack[0].code = ART_LINETO_CLIPPED;

        instance->segment_stack[1].x = current.x + t2*dx;
        instance->segment_stack[1].y = current.y + t2*dy;
        instance->segment_stack[1].code = ART_LINETO;
        instance->segment_stack_pointer = 1;
      }
      else {
        instance->segment_stack[0] = *(instance->src->current(instance->src));
        instance->segment_stack_pointer = 0;
      }

      if (t1 > 0.0) {
        ++instance->segment_stack_pointer;
        instance->segment_stack[instance->segment_stack_pointer].x = current.x + t1*dx;
        instance->segment_stack[instance->segment_stack_pointer].y = current.y + t1*dy;
        instance->segment_stack[instance->segment_stack_pointer].code = ART_LINETO_CLIPPED;
      }
    }
    else {
      /* the line is completely outside of the cliprect */
      instance->segment_stack[0] = *(instance->src->current(instance->src));
      instance->segment_stack[0].code = ART_LINETO_CLIPPED;
      instance->segment_stack_pointer = 0;
    }
  }
}

/* virtual current() function for ArtVpathClipFilter */
static ArtVpath *
_art_vpath_clip_filter_current (ArtVpathIterator *instance)
{
  if (((ArtVpathClipFilter *)instance)->segment_stack_pointer >= 0)
    return &(((ArtVpathClipFilter *)instance)->segment_stack[((ArtVpathClipFilter *)instance)->segment_stack_pointer]);
  else
    return 0;
}

/* virtual next() function for ArtVpathClipFilter */
static void
_art_vpath_clip_filter_next (ArtVpathIterator *instance)
{
  if (((ArtVpathClipFilter *)instance)->segment_stack_pointer <= 0)
    _art_vpath_clip_do_next_segment(((ArtVpathClipFilter *)instance));
  else
    --(((ArtVpathClipFilter *)instance)->segment_stack_pointer);
}

/**
 * art_vpath_clip_filter_init: Initialize an ArtVpathClipFilter
 * @src_iter: Source iterator
 * @drect: Clip rectangle
 * @instance: Filter instance to initialize
 *
 * Initializes a filter @instance that clips @src_iter to @drect.
 * Clipped segments are designated by ART_LINE_CLIPPED.
 **/
void art_vpath_clip_filter_init (ArtVpathIterator *src_iter,
                                 const ArtDRect *drect,
                                 ArtVpathClipFilter *instance)
{
  instance->super.current = &_art_vpath_clip_filter_current;
  instance->super.next = &_art_vpath_clip_filter_next;
  instance->src = src_iter;
  instance->cliprect = drect;
  instance->segment_stack_pointer = -1;

  if (instance->src->current(instance->src)) {
    instance->segment_stack[0] = *(instance->src->current(instance->src));
    instance->segment_stack_pointer = 0;
  }
}

/************************************************************************/
/* polygon to upright rect clip filter */

static void
_art_vpath_puecf_intersect_vert(double x1, double y1,
                                double x2, double y2,
                                ArtVpathPolyUpEdgeClipFilter *_this)
{
  _this->current_element.x = _this->edge_coord;
  _this->current_element.y = (y2-y1)/(x2-x1)*(_this->edge_coord - x1) + y1;
}

static void
_art_vpath_puecf_intersect_horz(double x1, double y1,
                                double x2, double y2,
                                ArtVpathPolyUpEdgeClipFilter *_this)
{
  _this->current_element.x = (x2-x1)/(y2-y1)*(_this->edge_coord - y1) + x1;
  _this->current_element.y = _this->edge_coord;
}

static int
_art_vpath_puecf_inside_horz_min(ArtVpathPolyUpEdgeClipFilter *_this)
{
  return (_this->src->current(_this->src)->y >= _this->edge_coord);
}

static int
_art_vpath_puecf_inside_horz_max(ArtVpathPolyUpEdgeClipFilter *_this)
{
  return (_this->src->current(_this->src)->y <= _this->edge_coord);
}

static int
_art_vpath_puecf_inside_vert_min(ArtVpathPolyUpEdgeClipFilter *_this)
{
  return (_this->src->current(_this->src)->x >= _this->edge_coord);
}

static int
_art_vpath_puecf_inside_vert_max(ArtVpathPolyUpEdgeClipFilter *_this)
{
  return (_this->src->current(_this->src)->x <= _this->edge_coord);
}

static ArtVpath *
_art_vpath_puecf_current (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  return &(_this->current_element);
}

/* we keep track of the state of the clipper by having a different
 * next() function for each state */
static void _art_vpath_puecf_next_start(ArtVpathIterator *instance);
static void _art_vpath_puecf_next_inside(ArtVpathIterator *instance);
static void _art_vpath_puecf_next_outside(ArtVpathIterator *instance);
static void _art_vpath_puecf_next_pending_lineto(ArtVpathIterator *instance);
static void _art_vpath_puecf_next_pending_moveto(ArtVpathIterator *instance);
static void _art_vpath_puecf_next_pending_end(ArtVpathIterator *instance);


static void
_art_vpath_puecf_next_start (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  int finished1 = 0;
  int finished2 = 0;

  while (!finished1) {
    switch (_this->src->current(_this->src)->code) {
      case ART_END:
        _this->current_element.code = ART_END;
        /* state stays 'start' */
        finished1 = 1;
        break;

      default:
        if (_this->inside(_this)) {
          _this->startx = _this->src->current(_this->src)->x;
          _this->starty = _this->src->current(_this->src)->y;
          _this->current_element.x = _this->startx;
          _this->current_element.y = _this->starty;
          _this->current_element.code = ART_MOVETO;
          _this->super.next = &_art_vpath_puecf_next_inside;
          finished1 = 1;
        }
        else {
          /* the subpath starts outside of the edge. find the entry
           * point. */
          finished2 = 0;
          while (!finished2) {
            _this->current_element = *_this->src->current(_this->src);
            _this->src->next(_this->src);
            switch (_this->src->current(_this->src)->code) {
              case ART_END:
              case ART_MOVETO:
                /* the whole subpath was outside of the edge. back to
                 * outer loop to treat next subpath. */
                finished2 = 1;
                break;
              default:
                if (_this->inside(_this)) {
                  _this->intersect(_this->src->current(_this->src)->x,
                                   _this->src->current(_this->src)->y,
                                   _this->current_element.x,
                                   _this->current_element.y,
                                   _this);
                  _this->current_element.code = ART_MOVETO;
                  _this->startx = _this->current_element.x;
                  _this->starty = _this->current_element.y;
                  _this->super.next = &_art_vpath_puecf_next_pending_lineto;
                  finished1 = 1;
                  finished2 = 1;
                }
                break;
            }
          }
        }
        break;
    }
  }
}

static void
_art_vpath_puecf_next_inside (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  _this->src->next(_this->src);
  switch (_this->src->current(_this->src)->code) {
    case ART_END:
      _this->current_element.code = ART_END;
      _this->super.next = &_art_vpath_puecf_next_start;
      break;

    case ART_MOVETO:
      if (_this->inside(_this)) {
          _this->current_element = *(_this->src->current(_this->src));
          /* state stays 'inside' */
        }
        else {
          _this->intersect(_this->src->current(_this->src)->x,
                           _this->src->current(_this->src)->y,
                           _this->current_element.x,
                           _this->current_element.y,
                           _this);
          _this->current_element.code = ART_MOVETO;
          _this->super.next = &_art_vpath_puecf_next_outside;
        }
      break;

    default:
      if (_this->inside(_this)) {
        _this->current_element = *(_this->src->current(_this->src));
        /* state stays 'inside' */
      }
      else {
        _this->intersect(_this->src->current(_this->src)->x,
                         _this->src->current(_this->src)->y,
                         _this->current_element.x,
                         _this->current_element.y,
                         _this);
        _this->current_element.code = ART_LINETO;
        _this->super.next = &_art_vpath_puecf_next_outside;
      }
      break;
  }
}

static void
_art_vpath_puecf_next_outside (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  int finished = 0;

  while (!finished) {
    _this->current_element = *_this->src->current(_this->src);
    _this->src->next(_this->src);
    switch (_this->src->current(_this->src)->code) {
      case ART_END:
        _this->current_element.code = ART_LINETO;
        _this->current_element.x = _this->startx;
        _this->current_element.y = _this->starty;
        _this->super.next = &_art_vpath_puecf_next_pending_end;
        finished = 1;
        break;

      case ART_MOVETO:
        if (_this->inside(_this)) {
          _this->intersect(_this->src->current(_this->src)->x,
                           _this->src->current(_this->src)->y,
                           _this->current_element.x,
                           _this->current_element.y,
                           _this);
          _this->current_element.code = ART_MOVETO;
          _this->super.next = &_art_vpath_puecf_next_pending_moveto;
          finished = 1;
        }
        break;

      default:
        if (_this->inside(_this)) {
          _this->intersect(_this->src->current(_this->src)->x,
                           _this->src->current(_this->src)->y,
                           _this->current_element.x,
                           _this->current_element.y,
                           _this);
          _this->current_element.code = ART_LINETO;
          _this->super.next = &_art_vpath_puecf_next_pending_lineto;
          finished = 1;
        }
        break;
    }
  }
}

static void
_art_vpath_puecf_next_pending_lineto (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  _this->current_element.x = _this->src->current(_this->src)->x;
  _this->current_element.y = _this->src->current(_this->src)->y;
  _this->current_element.code = ART_LINETO;
  _this->super.next = &_art_vpath_puecf_next_inside;
}

static void
_art_vpath_puecf_next_pending_moveto (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  _this->current_element.x = _this->src->current(_this->src)->x;
  _this->current_element.y = _this->src->current(_this->src)->y;
  _this->current_element.code = ART_LINETO;
  _this->super.next = &_art_vpath_puecf_next_inside;
}

static void
_art_vpath_puecf_next_pending_end (ArtVpathIterator *instance)
{
  ArtVpathPolyUpEdgeClipFilter* _this = (ArtVpathPolyUpEdgeClipFilter *)instance;
  _this->current_element.code = ART_END;
  _this->super.next = &_art_vpath_puecf_next_start;
}

void
_art_vpath_poly_upedge_clip_filter_init (ArtVpathIterator *src_iter,
                                         double coord, int min, int vert,
                                         ArtVpathPolyUpEdgeClipFilter *instance)
{
  instance->edge_coord = coord;
  instance->src = src_iter;
  instance->super.current = &_art_vpath_puecf_current;
  instance->super.next = &_art_vpath_puecf_next_start;

  if (vert) {
    instance->intersect = &_art_vpath_puecf_intersect_vert;
    if (min) {
      instance->inside = &_art_vpath_puecf_inside_vert_min;
    }
    else {
      instance->inside = &_art_vpath_puecf_inside_vert_max;
    }
  }
  else {
    instance->intersect = &_art_vpath_puecf_intersect_horz;
    if (min) {
      instance->inside = &_art_vpath_puecf_inside_horz_min;
    }
    else {
      instance->inside = &_art_vpath_puecf_inside_horz_max;
    }
  }

  instance->super.next(&(instance->super));
}

/**
 * art_vpath_poly_rect_clip_filter_init: Initialize an ArtVpathPolyRectClipFilter
 * @src_iter: Source iterator
 * @drect: Clip rectangle
 * @instance: Filter instance to initialize
 *
 * An ArtVpathPolyRectClipFilter clips a polygon described by
 * @src_iter to the upright rectangle @drect using the
 * Sutherland-Hodgman algorithm
 **/

void art_vpath_poly_rect_clip_filter_init (ArtVpathIterator *src_iter,
                                           const ArtDRect *drect,
                                           ArtVpathPolyRectClipFilter *instance)
{
  /* the first element in our edge_clippers array doubles as *our*
   * ArtVpathIterator implementation. hence we need to set up the
   * pipeline such that the *last* pipleline filter is the *first*
   * element in edge_clippers (e_c[0]):
   *
   *          e_c[3]       e_c[2]       e_c[1]       e_c[0]
   *            =            =            =            =
   *         +------+     +------+     +------+     +------+
   * src --> | xmin | --> | xmax | --> | ymin | --> | ymax |
   *         +------+     +------+     +------+     +------+
  */

  _art_vpath_poly_upedge_clip_filter_init(src_iter,
                                          drect->x0, 1, 1,
                                          &(instance->edge_clippers[3]));
  _art_vpath_poly_upedge_clip_filter_init(&(instance->edge_clippers[3].super),
                                          drect->x1, 0, 1,
                                          &(instance->edge_clippers[2]));
  _art_vpath_poly_upedge_clip_filter_init(&(instance->edge_clippers[2].super),
                                          drect->y0, 1, 0,
                                          &(instance->edge_clippers[1]));
  _art_vpath_poly_upedge_clip_filter_init(&(instance->edge_clippers[1].super),
                                          drect->y1, 0, 0,
                                          &(instance->edge_clippers[0]));
}


/************************************************************************/
/* contract filter */

/* helper function to do the contracting */
static void
_art_vpath_contract(ArtVpathContractFilter *instance)
{
  double x,y;
  do {
    x = instance->src->current(instance->src)->x;
    y = instance->src->current(instance->src)->y;
    instance->src->next(instance->src);
  } while (instance->src->current(instance->src)->code ==
           instance->src_pathcode);

  instance->current_element.x = x;
  instance->current_element.y = y;
  instance->current_element.code = instance->dst_pathcode;
}

/* virtual current() function for ArtVpathContractFilter */
static ArtVpath *
_art_vpath_contract_filter_current (ArtVpathIterator *instance)
{
  return &(((ArtVpathContractFilter *)instance)->current_element);
}

/* virtual next() function for ArtVpathContractFilter */
static void
_art_vpath_contract_filter_next (ArtVpathIterator *instance)
{
  ArtVpathContractFilter *_this = (ArtVpathContractFilter *)instance;

  if (_this->src->current(_this->src)->code == _this->src_pathcode) {
    _art_vpath_contract(_this);
  }
  else {
    _this->current_element = *(_this->src->current(_this->src));
    if (_this->current_element.code != ART_END) {
      _this->src->next(_this->src);
    }
  }
}

/**
 * art_vpath_contract_filter_init: Contract a vpath
 * @src_iter: Source Iterator
 * @src_pathcode: Pathcode of segments to be contracted.
 * @dst_pathcode: New pathcode for contracted segments.
 * @instance: Iterator to initialize
 *
 * A contract filter replaces consecutive @src_iter segments of type
 * @src_pathcode with a single @dst_pathcode. Can e.g. be used to
 * construct a closed or opened path from a @src_iter that contains
 * ART_LINE_CLIPPED segments.
 **/
void
art_vpath_contract_filter_init (ArtVpathIterator *src_iter,
                                ArtPathcode src_pathcode,
                                ArtPathcode dst_pathcode,
                                ArtVpathContractFilter *instance)
{
  instance->super.current = &_art_vpath_contract_filter_current;
  instance->super.next = &_art_vpath_contract_filter_next;
  instance->src = src_iter;
  instance->src_pathcode = src_pathcode;
  instance->dst_pathcode = dst_pathcode;
  instance->current_element = *(src_iter->current(src_iter));
  if (instance->current_element.code != ART_END) {
    src_iter->next(src_iter);
  }
}


/************************************************************************/
/* dash filter */

/* ++(dasharray pointer) */
void _art_dashpointer_advance(ArtVpathDashPointer *dp)
{
  dp->offset = 0.0;
  dp->is_dash = !dp->is_dash;
  dp->index = (dp->index < dp->dasharray->n_dash-1 ?
               dp->index+1 : 0);
}

/* seek dasharray pointer to dist */
void _art_dashpointer_seek(double dist, ArtVpathDashPointer *dp)
{
  dist = fmod(dist, dp->length);

  for (dp->index = 0; dist > 0.0; ++dp->index)
    dist -= dp->dasharray->dash[dp->index];

  if (dist < 0.0) {
    --dp->index;
    dp->offset = dp->dasharray->dash[dp->index] + dist;
  }

  dp->is_dash = !(dp->index%2);

  /* if the dasharray is odd, we have to effectively double it;
   * i.e. the first element is either a dash or gap depending whether
   * this is an even or odd traversal */
  if ((dp->dasharray->n_dash%2) && fmod(dist, dp->length*2.0) > dp->length)
    dp->is_dash = !dp->is_dash;
}

/* get the next dash/gap element from the current segment */
int _art_dash_segment_next_element(ArtVpathDashFilter *instance)
{
  if (instance->dist >= instance->dist1)
    return 0; /* no more elements from this segment */

  instance->current_element.code = (instance->dashpointer.is_dash ?
                                    ART_LINETO : ART_MOVETO_OPEN);

  instance->dist += instance->dashpointer.dasharray->dash[instance->dashpointer.index] -
    instance->dashpointer.offset;
  if (instance->dist > instance->dist1) {
    instance->dashpointer.offset = instance->dashpointer.dasharray->dash[instance->dashpointer.index] +
      instance->dist1 - instance->dist;
    instance->dist = instance->dist1;
  }
  else {
    _art_dashpointer_advance(&instance->dashpointer);
  }

  instance->current_element.x = instance->x0 + instance->dx*(instance->dist-instance->dist0);
  instance->current_element.y = instance->y0 + instance->dy*(instance->dist-instance->dist0);

  return 1;
}

/* dash filter helper */
void _art_dash_next_segment(ArtVpathDashFilter *instance)
{
  double d=0.0;
  int finished = 0;
  int clipped = 0;

  instance->x0 = instance->src->current(instance->src)->x;
  instance->y0 = instance->src->current(instance->src)->y;

  while (!finished) {
    instance->src->next(instance->src);
    switch(instance->src->current(instance->src)->code) {
      case ART_END:
        instance->current_element = *(instance->src->current(instance->src));
        finished = 1;
        break;
      case ART_LINETO:
        if (d!=0.0) {
          instance->dist += d;
          _art_dashpointer_seek(instance->dist, &instance->dashpointer);
        }
        instance->dist0 = instance->dist;
        instance->dx = instance->src->current(instance->src)->x -
          instance->x0;
        instance->dy = instance->src->current(instance->src)->y -
          instance->y0;
        d = sqrt( instance->dx * instance->dx + instance->dy * instance->dy);
        /* normalize (dx,dy) */
        if (d!=0.0) {
          instance->dx /= d;
          instance->dy /= d;
        }
        instance->dist1 = instance->dist + d;
        if (clipped) {
          instance->current_element.code = ART_MOVETO_OPEN;
          instance->current_element.x = instance->x0;
          instance->current_element.y = instance->y0;
          finished = 1;
        }
        else {
          finished = _art_dash_segment_next_element(instance);
        }
        break;
      case ART_LINETO_CLIPPED:
        instance->dx = instance->src->current(instance->src)->x -
          instance->x0;
        instance->dy = instance->src->current(instance->src)->y -
          instance->y0;
        d += sqrt( instance->dx * instance->dx + instance->dy * instance->dy);
        clipped = 1;
        /* intentionally no break; */
      default:
        instance->x0 = instance->src->current(instance->src)->x;
        instance->y0 = instance->src->current(instance->src)->y;
        break;
    }
  }
}

/* virtual current() function for ArtVpathDashFilter */
static ArtVpath *
_art_vpath_dash_filter_current (ArtVpathIterator *instance)
{
  return &(((ArtVpathDashFilter*)instance)->current_element);
}

/* virtual next() function for ArtVpathDashFilter */
static void
_art_vpath_dash_filter_next (ArtVpathIterator *instance)
{
  if (((ArtVpathDashFilter*)instance)->current_element.code == ART_END) return;
  if (!_art_dash_segment_next_element((ArtVpathDashFilter*)instance))
    _art_dash_next_segment((ArtVpathDashFilter*)instance);
}

/**
 * art_vpath_dash_filter_init: Initialize an ArtVpathDashFilter
 * @src_iter: Source iterator
 * @dasharray: Dash style descriptor
 * @instance: Filter instance to initialize
 *
 * Initializes a filter @instance that dashes @src_iter according to
 * @dasharray. Only ART_LINE segments are being dashes. Both ART_LINE
 * and ART_LINE_CLIPPED segments contribute to the path length used in
 * the dash calculations, ensuring that the offset of dashes in
 * clipped vpaths is correct.
 **/
void art_vpath_dash_filter_init (ArtVpathIterator *src_iter,
                                 ArtVpathDash *dasharray,
                                 ArtVpathDashFilter *instance)
{
  int i;
  instance->super.current = &_art_vpath_dash_filter_current;
  instance->super.next = &_art_vpath_dash_filter_next;
  instance->src = src_iter;
  instance->current_element = *(src_iter->current(src_iter));

  instance->dist = dasharray->offset;
  instance->dist1 = instance->dist;

  instance->dashpointer.dasharray = dasharray;
  instance->dashpointer.length = 0.0;
  instance->dashpointer.offset = 0.0;
  for (i=0; i<dasharray->n_dash; ++i)
    instance->dashpointer.length += dasharray->dash[i];
  _art_dashpointer_seek(instance->dist, &instance->dashpointer);
}


