/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __MOZ_CONTAINER_H__
#define __MOZ_CONTAINER_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * MozContainer
 *
 * This class serves two purposes in the nsIWidget implementation.
 *
 *   - It provides objects to receive signals from GTK for events on native
 *     windows.
 *
 *   - It provides a container parent for GtkWidgets.  The only GtkWidgets
 *     that need this in Mozilla are the GtkSockets for windowed plugins (Xt
 *     and XEmbed).
 *
 * Note that the window hierarchy in Mozilla differs from conventional
 * GtkWidget hierarchies.
 *
 * Mozilla's hierarchy exists through the GdkWindow hierarchy, and all child
 * GdkWindows (within a child nsIWidget hierarchy) belong to one MozContainer
 * GtkWidget.  If the MozContainer is unrealized or its GdkWindows are
 * destroyed for some other reason, then the hierarchy no longer exists.  (In
 * conventional GTK clients, the hierarchy is recorded by the GtkWidgets, and
 * so can be re-established after destruction of the GdkWindows.)
 *
 * One consequence of this is that the MozContainer does not know which of its
 * GdkWindows should parent child GtkWidgets.  (Conventional GtkContainers
 * determine which GdkWindow to assign child GtkWidgets.)
 *
 * Therefore, when adding a child GtkWidget to a MozContainer,
 * gtk_widget_set_parent_window should be called on the child GtkWidget before
 * it is realized.
 */
 
#define MOZ_CONTAINER_TYPE            (moz_container_get_type())
#define MOZ_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOZ_CONTAINER_TYPE, MozContainer))
#define MOZ_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOZ_CONTAINER_TYPE, MozContainerClass))
#define IS_MOZ_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOZ_CONTAINER_TYPE))
#define IS_MOZ_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOZ_CONTAINER_TYPE))
#define MOZ_CONAINTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOZ_CONTAINER_TYPE, MozContainerClass))

#if (GTK_CHECK_VERSION(2, 12, 0) || \
    (GTK_CHECK_VERSION(2, 10, 0) && defined(MOZ_PLATFORM_MAEMO)))
#define HAVE_GTK_MOTION_HINTS
#endif

typedef struct _MozContainer      MozContainer;
typedef struct _MozContainerClass MozContainerClass;

struct _MozContainer
{
    GtkContainer   container;
    GList         *children;
};

struct _MozContainerClass
{
    GtkContainerClass parent_class;
};

GType      moz_container_get_type (void);
GtkWidget *moz_container_new      (void);
void       moz_container_put      (MozContainer *container,
                                   GtkWidget    *child_widget,
                                   gint          x,
                                   gint          y);
void       moz_container_move          (MozContainer *container,
                                        GtkWidget    *child_widget,
                                        gint          x,
                                        gint          y,
                                        gint          width,
                                        gint          height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MOZ_CONTAINER_H__ */
