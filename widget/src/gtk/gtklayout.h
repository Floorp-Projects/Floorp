/* Copyright Owen Taylor, 1998
 * 
 * This file may be distributed under either the terms of the
 * Netscape Public License, or the GNU Library General Public License
 *
 * Note: No GTK+ or Mozilla code should be added to this file.
 * The coding style should be that of the the GTK core.
 */

#ifndef __GTK_LAYOUT_H
#define __GTK_LAYOUT_H

#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkadjustment.h>


/*
 * We don't want to require gnome-libs, in which GtkLayout now lives, but
 * we do want to be able to link safely against it.
 * 
 * So my solution is to leave the __GTK_LAYOUT_H include guards the same to
 * prevent multiple inclusions, and #define new function names.  Better
 * solutions are welcome.  (All the symbols in gtklayout.c are static, so there
 * won't be any symbol-conflict issues.)
 */

/*
 * Generated with this JS code:
 names = ['new', 'get_type', 'put', 'move', 'set_size', 'freeze', 'thaw',
          'get_hadjustment', 'get_vadjustment', 'set_hadjustment',
          'set_vadjustment'];
 for (i = 0; i < names.length; i++) {
   print('#define gtk_layout_' + names[i] + ' moz_gtk_layout_' + names[i]);
 }
 */
#define gtk_layout_new moz_gtk_layout_new
#define gtk_layout_get_type moz_gtk_layout_get_type
#define gtk_layout_put moz_gtk_layout_put
#define gtk_layout_move moz_gtk_layout_move
#define gtk_layout_set_size moz_gtk_layout_set_size
#define gtk_layout_freeze moz_gtk_layout_freeze
#define gtk_layout_thaw moz_gtk_layout_thaw
#define gtk_layout_get_hadjustment moz_gtk_layout_get_hadjustment
#define gtk_layout_get_vadjustment moz_gtk_layout_get_vadjustment
#define gtk_layout_set_hadjustment moz_gtk_layout_set_hadjustment
#define gtk_layout_set_vadjustment moz_gtk_layout_set_vadjustment


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_LAYOUT(obj)          GTK_CHECK_CAST (obj, gtk_layout_get_type (), GtkLayout)
#define GTK_LAYOUT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_layout_get_type (), GtkLayoutClass)
#define GTK_IS_LAYOUT(obj)       GTK_CHECK_TYPE (obj, gtk_layout_get_type ())

typedef struct _GtkLayout        GtkLayout;
typedef struct _GtkLayoutClass   GtkLayoutClass;
typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
  GtkWidget *widget;
  GdkWindow *window;	/* For NO_WINDOW widgets */
  gint x;
  gint y;
};

struct _GtkLayout {
  GtkContainer container;

  GList *children;

  guint width;
  guint height;

  guint xoffset;
  guint yoffset;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  
  GdkWindow *bin_window;

  GdkVisibilityState visibility;
  gulong configure_serial;
  gint scroll_x;
  gint scroll_y;

  guint frozen : 1;
};

struct _GtkLayoutClass {
  GtkContainerClass parent_class;
};

GtkWidget*     gtk_layout_new             (GtkAdjustment *hadjustment,
				           GtkAdjustment *vadjustment);

guint          gtk_layout_get_type        (void);
void           gtk_layout_put             (GtkLayout     *layout, 
		                           GtkWidget     *widget, 
		                           gint           x, 
		                           gint           y);
  
void           gtk_layout_move            (GtkLayout     *layout, 
		                           GtkWidget     *widget, 
		                           gint           x, 
		                           gint           y);
  
void           gtk_layout_set_size        (GtkLayout     *layout, 
			                   guint          width,
			                   guint          height);

/* These disable and enable moving and repainting the scrolling window of the GtkLayout,
 * respectively.  If you want to update the layout's offsets but do not want it to
 * repaint itself, you should use these functions.
 */
void           gtk_layout_freeze          (GtkLayout     *layout);
void           gtk_layout_thaw            (GtkLayout     *layout);

GtkAdjustment* gtk_layout_get_hadjustment (GtkLayout     *layout);
GtkAdjustment* gtk_layout_get_vadjustment (GtkLayout     *layout);
void           gtk_layout_set_hadjustment (GtkLayout     *layout,
					   GtkAdjustment *adjustment);
void           gtk_layout_set_vadjustment (GtkLayout     *layout,
					   GtkAdjustment *adjustment);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_LAYOUT_H */
