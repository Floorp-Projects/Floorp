#ifndef __MOZ_DRAWINGAREA_H__
#define __MOZ_DRAWINGAREA_H__

#include <gdk/gdkwindow.h>
#include "mozcontainer.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MOZ_DRAWINGAREA_TYPE            (moz_drawingarea_get_type())
#define MOZ_DRAWINGAREA(obj)            (GTK_CHECK_CAST((obj), MOZ_DRAWINGAREA_TYPE, MozDrawingarea))
#define MOZ_DRAWINGAREA_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), MOZ_DRAWINGAREA_TYPE, MozDrawingareaClass))
#define IS_MOZ_DRAWINGAREA(obj)         (GTK_CHECK_TYPE((obj), MOZ_DRAWINGAREA_TYPE))
#define IS_MOZ_DRAWINGAREA_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), MOZ_DRAWINGAREA_TYPE))
#define MOZ_DRAWINGAREA_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), MOZ_DRAWINGAREA_TYPE, MozDrawingareaClass))

typedef struct _MozDrawingarea      MozDrawingarea;
typedef struct _MozDrawingareaClass MozDrawingareaClass;

struct _MozDrawingarea
{
  GObject         parent_instance;
  GdkWindow      *clip_window;
  GdkWindow      *inner_window;
  MozDrawingarea *parent;
};

struct _MozDrawingareaClass
{
  GObjectClass parent_class;
};

GtkType         moz_drawingarea_get_type       (void);
MozDrawingarea *moz_drawingarea_new            (MozDrawingarea *parent,
						MozContainer *widget_parent);
void            moz_drawingarea_move           (MozDrawingarea *drawingarea,
						gint x, gint y);
void            moz_drawingarea_resize         (MozDrawingarea *drawingarea,
						gint width, gint height);
void            moz_drawingarea_move_resize    (MozDrawingarea *drawingarea,
						gint x, gint y,
						gint width, gint height);
void            moz_drawingarea_set_visibility (MozDrawingarea *drawingarea,
						gboolean visibility);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MOZ_DRAWINGAREA_H__ */
