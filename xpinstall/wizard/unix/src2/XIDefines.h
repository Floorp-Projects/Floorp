/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _XIDEFINES_H_
#define _XIDEFINES_H_

#include "XIErrors.h"
#include <gtk/gtk.h>

/*--------------------------------------------------------------------*
 *   Default Strings
 *--------------------------------------------------------------------*/
#define NEXT "Next >"
#define BACK "< Back"
#define ACCEPT "Accept"
#define DECLINE "Decline"
#define INSTALL "Install"
#define DEFAULT_TITLE "Mozilla Installer"
#define DEST_DIR "Destination Directory"
#define BROWSE "Browse..."
#define SELECT_DIR "Select a directory"
#define DESCRIPTION "Description"
#define DOWNLOADING "Downloading..."
#define PREPARING "Preparing installer modules..."
#define EXTRACTING "Extracting installer files..."
#define INSTALLING "Installing"


/*--------------------------------------------------------------------*
 *   Limits
 *--------------------------------------------------------------------*/
#define MAX_COMPONENTS 64
#define MAX_SETUP_TYPES 4
#define MAX_URLS 32
#define MAX_URL_LEN 1024
#define MAX_TMP_DIRS 1024


/*--------------------------------------------------------------------*
 *   Widget Dims
 *--------------------------------------------------------------------*/
#define XI_WIN_HEIGHT 320
#define XI_WIN_WIDTH  550
#define README_FONT "-misc-fixed-medium-r-*-*-*-80-*-*-*-*-*-*"
#define LICENSE_FONT "-misc-fixed-medium-r-*-*-*-80-*-*-*-*-*-*"


/*--------------------------------------------------------------------*
 *   Parse Keys
 *--------------------------------------------------------------------*/
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
#define SETUP_TYPE          "Setup Type"
#define SETUP_TYPEd         "Setup Type%d"
#define DESC_SHORT          "Description Short"
#define DESC_LONG           "Description Long"

#define DLG_COMPONENTS      "Dialog Select Components"
#define COMPONENT           "Component"
#define COMPONENTd          "Component%d"
#define Cd                  "C%d"
#define ARCHIVE             "Archive"
#define URLd                "URL%d"
#define SIZE                "Install Size"
#define DEPENDENCYd         "Dependency%d"
#define ATTRIBUTES          "Attributes"
#define SELECTED_ATTR       "SELECTED"
#define INVISIBLE_ATTR      "INVISIBLE"
#define LAUNCHAPP_ATTR      "LAUNCHAPP"

#define DLG_START_INSTALL   "Dialog Start Install"
#define XPINSTALL_ENGINE    "XPInstall Engine"


/*--------------------------------------------------------------------*
 *   Macros
 *--------------------------------------------------------------------*/
#define TMP_DIR_TEMPLATE "%s/.tmp.xi.%d" // <cwd>/.tmp.xi.<N>
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

#define XI_ERR_BAIL(_function)                          \
do {                                                    \
    err = _function;                                    \
    if (err != OK)                                      \
    {                                                   \
        ErrorHandler(err);                              \
        goto BAIL;                                      \
    }                                                   \
} while (0); 

#define XI_ERR_BAIL_EXCEPT(_function, _errexpected)     \
do {                                                    \
    err = _function;                                    \
    if (err != OK && err != _errexpected)               \
    {                                                   \
        ErrorHandler(err);                              \
        goto BAIL;                                      \
    }                                                   \
} while(0);
        
#define XI_VERIFY(_ptr)                                 \
do {                                                    \
    if (!_ptr)                                          \
        return ErrorHandler(E_INVALID_PTR);             \
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
