/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * Copyright (C) 1999 Alexander Larsson.  All Rights Reserved.
 */
#ifndef GTKMOZILLA_H
#define GTKMOZILLA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtklayout.h>

#define GTK_TYPE_MOZILLA              (gtk_mozilla_get_type ())
#define GTK_MOZILLA(obj)              GTK_CHECK_CAST ((obj), GTK_TYPE_MOZILLA, GtkMozilla)
#define GTK_MOZILLA_CLASS(klass)      GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MOZILLA, GtkMozillaClass)
#define GTK_IS_MOZILLA(obj)           GTK_CHECK_TYPE ((obj), GTK_TYPE_MOZILLA)
#define GTK_IS_MOZILLA_CLASS(klass)   GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MOZILLA)


typedef enum GtkMozillaReloadType {
  GTK_URL_RELOAD = 0,
  GTK_URL_RELOAD_BYPASS_CACHE,
  GTK_URL_RELOAD_BYPASS_PROXY,
  GTK_URL_RELOAD_BYPASS_CACHE_AND_PROXY,
  GTK_URL_RELOAD_MAX
} GtkMozillaReloadType;

typedef enum GtkMozillaLoadType {
  GTK_LOAD_URL,
  GTK_LOAD_HISTORY,
  GTK_LOAD_LINK,
  GTK_LOAD_REFRESH
} GtkMozillaLoadType;

typedef struct _GtkMozilla       GtkMozilla;
typedef struct _GtkMozillaClass  GtkMozillaClass;
  
struct _GtkMozilla
{
  GtkLayout layout;

  void *mozilla_container;
};

struct _GtkMozillaClass
{
  GtkLayoutClass parent_class;

  gint (*will_load_url) (GtkMozilla  *mozilla,
                         const gchar *url,
                         GtkMozillaLoadType load_type);
  void (*begin_load_url) (GtkMozilla  *mozilla,
                          const gchar *url);
  void (*end_load_url) (GtkMozilla  *mozilla,
                        const gchar *url,
                        gint status);
};

extern GtkType    gtk_mozilla_get_type(void);
extern GtkWidget* gtk_mozilla_new(void);

extern void gtk_mozilla_load_url(GtkMozilla *moz, const char *url);
extern void gtk_mozilla_stop(GtkMozilla *moz);
extern void gtk_mozilla_reload(GtkMozilla *moz, GtkMozillaReloadType reload_type);
extern void gtk_mozilla_resize(GtkMozilla *moz, gint width, gint height);

extern void gtk_mozilla_back(GtkMozilla *moz);
extern void gtk_mozilla_forward(GtkMozilla *moz);
extern gint gtk_mozilla_can_back(GtkMozilla *moz);
extern gint gtk_mozilla_can_forward(GtkMozilla *moz);
extern void gtk_mozilla_goto_history(GtkMozilla *moz, gint index);
extern gint gtk_mozilla_get_history_length(GtkMozilla *moz);
extern gint gtk_mozilla_get_history_index(GtkMozilla *moz);

extern gint gtk_mozilla_stream_start(GtkMozilla *moz,
                                     const char *base_url,
                                     const char *action,
                                     const char *content_type);
extern gint gtk_mozilla_stream_start_html(GtkMozilla *moz,
                                          const char *base_url);
extern gint gtk_mozilla_stream_write(GtkMozilla *moz, 
                                     const char *data,
                                     gint offset,
                                     gint len);
extern void gtk_mozilla_stream_end(GtkMozilla *moz);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* GTKMOZILLA_H */
