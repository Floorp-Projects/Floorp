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

#ifndef __GTK_MOZBOX_H__
#define __GTK_MOZBOX_H__

#include <gtk/gtkwindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkMozBox GtkMozBox;
typedef struct _GtkMozBoxClass GtkMozBoxClass;

#define GTK_TYPE_MOZBOX                  (gtk_mozbox_get_type ())
#define GTK_MOZBOX(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_MOZBOX, GtkMozBox))
#define GTK_MOZBOX_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MOZBOX, GtkMozBoxClass))
#define GTK_IS_MOZBOX(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_MOZBOX))
#define GTK_IS_MOZBOX_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MOZBOX))

struct _GtkMozBox
{
  GtkWindow window;
  GdkWindow *parent_window;
  gint x;
  gint y;
};
  
struct _GtkMozBoxClass
{
  GtkWindowClass window_class;
};

GtkType    gtk_mozbox_get_type (void);
GtkWidget *gtk_mozbox_new (GdkWindow *parent_window);
void       gtk_mozbox_set_position (GtkMozBox *mozbox,
				    gint       x,
				    gint       y);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_MOZBOX_H__ */
