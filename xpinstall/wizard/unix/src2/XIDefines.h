/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _XIDEFINES_H_
#define _XIDEFINES_H_

#include "XIErrors.h"
#include <gtk/gtk.h>


/*--------------------------------------------------------------------*
 *   Limits
 *--------------------------------------------------------------------*/
#define MAX_COMPONENTS 64
#define MAX_SETUP_TYPES 4
#define MAX_URLS 32
#define MAX_URL_LEN 1024
#define MAX_DEPENDEE_KEY_LEN 16
#define MAX_LEGACY_CHECKS 32


/*--------------------------------------------------------------------*
 *   Widget Dims
 *--------------------------------------------------------------------*/
#define XI_WIN_HEIGHT 320
#define XI_WIN_WIDTH  550
#define README_FONT "-misc-fixed-medium-r-normal--8-100-75-75-c-50-iso8859-1"
#define LICENSE_FONT "-misc-fixed-medium-r-normal--8-100-75-75-c-50-iso8859-1"


/*--------------------------------------------------------------------*
 *   Parse Keys
 *--------------------------------------------------------------------*/
#define GENERAL             "General"
#define DEFAULT_LOCATION    "Default Location" 
#define DEFAULT_SETUP_TYPE  "Default Setup Type"

#define CLEAN_UPGRADE       "Cleanup On Upgrade"
#define OBJECT_IGNOREd      "ObjectToIgnore%d"

#define DLG_WELCOME         "Dialog Welcome"
#define SHOW_DLG            "Show Dialog"
#define TITLE               "Title"
#define README              "Readme File"

#define DLG_LICENSE         "Dialog License"
#define LICENSE             "License File"

#define DLG_SETUP_TYPE      "Dialog Setup Type"
#define MSG0                "Message0"
#define MSG1                "Message1"
#define MSG2                "Message2"
#define SETUP_TYPEd         "Setup Type%d"
#define DESC_SHORT          "Description Short"
#define DESC_LONG           "Description Long"

#define LEGACY_CHECKd       "LegacyCheck%d"
#define FILENAME            "Filename"
#define MSG                 "Message"

#define DLG_COMPONENTS      "Dialog Select Components"
#define COMPONENT           "Component"
#define COMPONENTd          "Component%d"
#define Cd                  "C%d"
#define ARCHIVE             "Archive"
#define URLd                "URL%d"
#define INSTALL_SIZE        "Install Size"
#define ARCHIVE_SIZE        "Archive Size"
#define DEPENDENCYd         "Dependency%d"
#define DEPENDEEd           "Dependee%d"
#define ATTRIBUTES          "Attributes"
#define SELECTED_ATTR       "SELECTED"
#define INVISIBLE_ATTR      "INVISIBLE"
#define LAUNCHAPP_ATTR      "LAUNCHAPP"
#define DOWNLOAD_ONLY_ATTR  "DOWNLOAD_ONLY"

#define RUNAPPd             "RunApp%d"
#define POSTINSTALLRUNd     "PostInstallRun%d"
#define TARGET              "Target"
#define ARGS                "Arguments"

#define DLG_START_INSTALL   "Dialog Start Install"
#define XPINSTALL_ENGINE    "XPInstall Engine"


/*--------------------------------------------------------------------*
 *   Macros
 *--------------------------------------------------------------------*/
#define TMP_EXTRACT_SUBDIR "bin"
#define XPI_DIR "./xpi"

#define XPISTUB "libxpistub.so"
#define FN_INIT     "XPI_Init"
#define FN_INSTALL  "XPI_Install"
#define FN_EXIT     "XPI_Exit"

#define XI_IF_DELETE(_object)                           \
do {                                                    \
    if (_object)                                        \
        delete _object;                                 \
    _object = NULL;                                     \
} while(0);

#define XI_IF_FREE(_ptr)                                \
do {                                                    \
    if (_ptr)                                           \
        free(_ptr);                                     \
    _ptr = NULL;                                        \
} while(0);

#define XI_GTK_IF_FREE(_gtkWidgetPtr)                   \
do {                                                    \
    if (_gtkWidgetPtr && GTK_IS_WIDGET(_gtkWidgetPtr))  \
        gtk_widget_destroy(_gtkWidgetPtr);              \
    _gtkWidgetPtr = NULL;                               \
} while(0);

#define XI_ERR_BAIL(_function)                          \
do {                                                    \
    err = _function;                                    \
    if (err != OK)                                      \
    {                                                   \
        ErrorHandler(err);                              \
        goto BAIL;                                      \
    }                                                   \
} while (0); 

#define XI_VERIFY(_ptr)                                 \
do {                                                    \
    if (!_ptr)                                          \
        return ErrorHandler(E_INVALID_PTR);             \
} while (0);
     
#if defined(DEBUG_sgehani) || defined(DEBUG_druidd) || defined(DEBUG_root)
#define XI_ASSERT(_expr, _msg)                                              \
do {                                                                        \
    if (!(_expr))                                                           \
        printf("%s %d: ASSERTION FAILED! %s \n", __FILE__, __LINE__, _msg); \
} while(0);
#else
#define XI_ASSERT(_expr, _msg)
#endif

#define XI_GTK_UPDATE_UI()                              \
do {                                                    \
    while (gtk_events_pending())                        \
        gtk_main_iteration();                           \
} while (0);


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#endif /* _XIDEFINES_H_ */
