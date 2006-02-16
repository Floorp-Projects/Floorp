/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Original Code: Syd Logan (syd@netscape.com) 3/12/99 */

#ifndef nsPrintdOS2_h___
#define nsPrintdOS2_h___

#include <limits.h>

PR_BEGIN_EXTERN_C

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
        PRBool cancel;		     /* If PR_TRUE, user cancelled */
} OS2PrData;

PR_END_EXTERN_C

#endif /* nsPrintdOS2_h___ */
