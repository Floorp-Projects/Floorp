/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Initial Developers of this code under the MPL are Owen Taylor
 * <otaylor@redhat.com> and Christopher Blizzard <blizzard@redhat.com>.
 * Portions created by the Initial Developers are Copyright (C) 1999
 * Owen Taylor and Christopher Blizzard.  All Rights Reserved.  */

#ifndef __GTK_MOZAREA_H__
#define __GTK_MOZAREA_H__

#include <gtk/gtkwindow.h>
#include "gdksuperwin.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkMozArea GtkMozArea;
typedef struct _GtkMozAreaClass GtkMozAreaClass;

#define GTK_TYPE_MOZAREA                  (gtk_mozarea_get_type ())
#define GTK_MOZAREA(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_MOZAREA, GtkMozArea))
#define GTK_MOZAREA_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MOZAREA, GtkMozAreaClass))
#define GTK_IS_MOZAREA(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_MOZAREA))
#define GTK_IS_MOZAREA_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MOZAREA))

struct _GtkMozArea
{
  GtkWidget widget;
  GdkSuperWin *superwin;
  gboolean     toplevel_focus;

  /* store away the toplevel window */
  GdkWindow *toplevel_window;
};
  
struct _GtkMozAreaClass
{
  GtkWindowClass window_class;

  /* signals */
  void (* toplevel_focus_in ) (GtkMozArea *area);
  void (* toplevel_focus_out) (GtkMozArea *area);
  void (* toplevel_configure) (GtkMozArea *area);
};

GtkType    gtk_mozarea_get_type (void);
GtkWidget *gtk_mozarea_new ();
gboolean   gtk_mozarea_get_toplevel_focus(GtkMozArea *area);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_MOZAREA_H__ */
