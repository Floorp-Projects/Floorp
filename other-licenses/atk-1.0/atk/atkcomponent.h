/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __ATK_COMPONENT_H__
#define __ATK_COMPONENT_H__

#include <atk/atkobject.h>
#include <atk/atkutil.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *AtkScrollType:
 *@ATK_SCROLL_TOP_LEFT: Scroll the object vertically and horizontally to the top
 *left corner of the window.
 *@ATK_SCROLL_BOTTOM_RIGHT: Scroll the object vertically and horizontally to the
 *bottom right corner of the window.
 *@ATK_SCROLL_TOP_EDGE: Scroll the object vertically to the top edge of the
 window.
 *@ATK_SCROLL_BOTTOM_EDGE: Scroll the object vertically to the bottom edge of
 *the window.
 *@ATK_SCROLL_LEFT_EDGE: Scroll the object vertically and horizontally to the
 *left edge of the window.
 *@ATK_SCROLL_RIGHT_EDGE: Scroll the object vertically and horizontally to the
 *right edge of the window.
 *@ATK_SCROLL_ANYWHERE: Scroll the object vertically and horizontally so that
 *as much as possible of the object becomes visible. The exact placement is
 *determined by the application.
 *
 * Specifies where an object should be placed on the screen when using scroll_to.
 **/
typedef enum {
  ATK_SCROLL_TOP_LEFT,
  ATK_SCROLL_BOTTOM_RIGHT,
  ATK_SCROLL_TOP_EDGE,
  ATK_SCROLL_BOTTOM_EDGE,
  ATK_SCROLL_LEFT_EDGE,
  ATK_SCROLL_RIGHT_EDGE,
  ATK_SCROLL_ANYWHERE
} AtkScrollType;

/*
 * The AtkComponent interface should be supported by any object that is 
 * rendered on the screen. The interface provides the standard mechanism 
 * for an assistive technology to determine and set the graphical
 * representation of an object.
 */

#define ATK_TYPE_COMPONENT                    (atk_component_get_type ())
#define ATK_IS_COMPONENT(obj)                 G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_COMPONENT)
#define ATK_COMPONENT(obj)                    G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_COMPONENT, AtkComponent)
#define ATK_COMPONENT_GET_IFACE(obj)          (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_COMPONENT, AtkComponentIface))

#ifndef _TYPEDEF_ATK_COMPONENT_
#define _TYPEDEF_ATK_COMPONENT_
typedef struct _AtkComponent AtkComponent;
#endif
typedef struct _AtkComponentIface  AtkComponentIface;

typedef void (*AtkFocusHandler) (AtkObject*, gboolean);

typedef struct _AtkRectangle       AtkRectangle;

struct _AtkRectangle
{
  gint x;
  gint y;
  gint width;
  gint height;
};

GType atk_rectangle_get_type (void);

#define ATK_TYPE_RECTANGLE (atk_rectangle_get_type ())
struct _AtkComponentIface
{
  GTypeInterface parent;

  guint          (* add_focus_handler)  (AtkComponent          *component,
                                         AtkFocusHandler        handler);

  gboolean       (* contains)           (AtkComponent          *component,
                                         gint                   x,
                                         gint                   y,
                                         AtkCoordType           coord_type);

  AtkObject*    (* ref_accessible_at_point)  (AtkComponent     *component,
                                         gint                   x,
                                         gint                   y,
                                         AtkCoordType           coord_type);
  void          (* get_extents)         (AtkComponent          *component,
                                         gint                  *x,
                                         gint                  *y,
                                         gint                  *width,
                                         gint                  *height,
                                         AtkCoordType          coord_type);
  void                     (* get_position)     (AtkComponent   *component,
                                                 gint           *x,
                                                 gint           *y,
                                                 AtkCoordType   coord_type);
  void                     (* get_size)                 (AtkComponent   *component,
                                                         gint           *width,
                                                         gint           *height);
  gboolean                 (* grab_focus)               (AtkComponent   *component);
  void                     (* remove_focus_handler)      (AtkComponent  *component,
                                                          guint         handler_id);
  gboolean                 (* set_extents)      (AtkComponent   *component,
                                                 gint           x,
                                                 gint           y,
                                                 gint           width,
                                                 gint           height,
                                                 AtkCoordType   coord_type);
  gboolean                 (* set_position)     (AtkComponent   *component,
                                                 gint           x,
                                                 gint           y,
                                                 AtkCoordType   coord_type);
  gboolean                 (* set_size)         (AtkComponent   *component,
                                                 gint           width,
                                                 gint           height);
  	
  AtkLayer                 (* get_layer)        (AtkComponent   *component);
  gint                     (* get_mdi_zorder)   (AtkComponent   *component);

  /*
   * signal handlers
   */
  void                     (* bounds_changed)   (AtkComponent   *component,
                                                 AtkRectangle   *bounds);
  gdouble                  (* get_alpha)        (AtkComponent   *component);

  /*
   * Scrolls this object so it becomes visible on the screen.
   * Since ATK 2.30
   */
  gboolean                (*scroll_to)          (AtkComponent   *component,
                                                 AtkScrollType   type);

  gboolean                (*scroll_to_point)    (AtkComponent   *component,
                                                 AtkCoordType    coords,
                                                 gint            x,
                                                 gint            y);
};

GType atk_component_get_type (void);

/* convenience functions */

guint                atk_component_add_focus_handler      (AtkComponent    *component,
                                                           AtkFocusHandler handler);
gboolean              atk_component_contains               (AtkComponent    *component,
                                                            gint            x,
                                                            gint            y,
                                                            AtkCoordType    coord_type);
AtkObject*            atk_component_ref_accessible_at_point(AtkComponent    *component,
                                                            gint            x,
                                                            gint            y,
                                                            AtkCoordType    coord_type);
void                  atk_component_get_extents            (AtkComponent    *component,
                                                            gint            *x,
                                                            gint            *y,
                                                            gint            *width,
                                                            gint            *height,
                                                            AtkCoordType    coord_type);
void                  atk_component_get_position           (AtkComponent    *component,
                                                            gint            *x,
                                                            gint            *y,
                                                            AtkCoordType    coord_type);
void                  atk_component_get_size               (AtkComponent    *component,
                                                            gint            *width,
                                                            gint            *height);
AtkLayer              atk_component_get_layer              (AtkComponent    *component);
gint                  atk_component_get_mdi_zorder         (AtkComponent    *component);
gboolean              atk_component_grab_focus             (AtkComponent    *component);
void                  atk_component_remove_focus_handler   (AtkComponent    *component,
                                                            guint           handler_id);
gboolean              atk_component_set_extents            (AtkComponent    *component,
                                                            gint            x,
                                                            gint            y,
                                                            gint            width,
                                                            gint            height,
                                                            AtkCoordType    coord_type);
gboolean              atk_component_set_position           (AtkComponent    *component,
                                                            gint            x,
                                                            gint            y,
                                                            AtkCoordType    coord_type);
gboolean              atk_component_set_size               (AtkComponent    *component,
                                                            gint            width,
                                                            gint            height);
gdouble               atk_component_get_alpha              (AtkComponent    *component);
gboolean              atk_component_scroll_to              (AtkComponent    *component,
                                                            AtkScrollType   type);

gboolean              atk_component_scroll_to_point        (AtkComponent    *component,
                                                            AtkCoordType    coords,
                                                            gint            x,
                                                            gint            y);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ATK_COMPONENT_H__ */
