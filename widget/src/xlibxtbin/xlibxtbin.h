/* widget -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2 */

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
 * The Initial Developer of the Original Code is 
 * Peter Hartshorn.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Based on GtkXtBin by Rusty Lynch - 02/27/2000
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

#ifndef __XLIB_XTBIN_H__
#define __XLIB_XTBIN_H__

#include "xlibrgb.h"

#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/X.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _XtTMRec {
    XtTranslations  translations;       /* private to Translation Manager    */
    XtBoundActions  proc_table;         /* procedure bindings for actions    */
    struct _XtStateRec *current_state;  /* Translation Manager state ptr     */
    unsigned long   lastEventTime;
} XtTMRec, *XtTM;   

typedef struct _CorePart {
    Widget          self;               /* pointer to widget itself          */
    WidgetClass     widget_class;       /* pointer to Widget's ClassRec      */
    Widget          parent;             /* parent widget                     */
    XrmName         xrm_name;           /* widget resource name quarkified   */
    Boolean         being_destroyed;    /* marked for destroy                */
    XtCallbackList  destroy_callbacks;  /* who to call when widget destroyed */
    XtPointer       constraints;        /* constraint record                 */
    Position        x, y;               /* window position                   */
    Dimension       width, height;      /* window dimensions                 */
    Dimension       border_width;       /* window border width               */
    Boolean         managed;            /* is widget geometry managed?       */
    Boolean         sensitive;          /* is widget sensitive to user events*/
    Boolean         ancestor_sensitive; /* are all ancestors sensitive?      */
    XtEventTable    event_table;        /* private to event dispatcher       */
    XtTMRec         tm;                 /* translation management            */
    XtTranslations  accelerators;       /* accelerator translations          */
    Pixel           border_pixel;       /* window border pixel               */
    Pixmap          border_pixmap;      /* window border pixmap or NULL      */
    WidgetList      popup_list;         /* list of popups                    */
    Cardinal        num_popups;         /* how many popups                   */
    String          name;               /* widget resource name              */
    Screen          *screen;            /* window's screen                   */
    Colormap        colormap;           /* colormap                          */
    Window          window;             /* window ID                         */
    Cardinal        depth;              /* number of planes in window        */
    Pixel           background_pixel;   /* window background pixel           */
    Pixmap          background_pixmap;  /* window background pixmap or NULL  */
    Boolean         visible;            /* is window mapped and not occluded?*/
    Boolean         mapped_when_managed;/* map window if it's managed?       */
} CorePart;

typedef struct _WidgetRec {
    CorePart    core;
 } WidgetRec, CoreRec;   
#ifdef __cplusplus
}
#endif /* __cplusplus */

class xtbin {
public:
  xtbin();
  ~xtbin();

  void xtbin_init();
  void xtbin_realize();
  void xtbin_new(Window aParent);
  void xtbin_destroy();
  void xtbin_resize(int aX, int aY, int aWidth, int aHeight);
  Window xtbin_xtwindow() { return xtwindow; }
  int xtbin_initialized();
  void sync();
  
private: 
  int initialized;
  XlibRgbHandle *mXlibRgbHandle;
  Display *xtdisplay;
  Window xtwindow;
  XtAppContext app_context;
  Window parent_window;
  Widget xtwidget;
  Window window;
  Window oldwindow;
  int x, y, width, height;
};


#endif /* __XLIB_XTBIN_H__ */

