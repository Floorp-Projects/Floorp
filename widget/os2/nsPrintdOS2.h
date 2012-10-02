/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original Code: Syd Logan (syd@netscape.com) 3/12/99 */

#ifndef nsPrintdOS2_h___
#define nsPrintdOS2_h___

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* stolen from nsPostScriptObj.h. needs to be put somewhere else that
   both ps and gtk can see easily */
 
#ifndef PATH_MAX
#ifdef _POSIX_PATH_MAX
#define PATH_MAX	_POSIX_PATH_MAX
#else
#define PATH_MAX	256
#endif
#endif

typedef enum  
{
  printToFile = 0, 
  printToPrinter, 
  printPreview 
} printDest;

typedef struct OS2prdata {
        printDest destination;     /* print to file, printer or print preview */
        int copies;                /* number of copies to print   0 < n < 999 */
        char printer[ PATH_MAX ];  /* Printer selected - name*/
        char path[ PATH_MAX ];     /* If destination = printToFile, dest file */
        bool cancel;		     /* If true, user cancelled */
} OS2PrData;

#ifdef __cplusplus
}
#endif

#endif /* nsPrintdOS2_h___ */
