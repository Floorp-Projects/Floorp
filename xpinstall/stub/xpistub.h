/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Daniel Veditz <dveditz@netscape.com>
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


#include "nsError.h"
#include "prtypes.h"

PR_BEGIN_EXTERN_C

/** pfnXPIProgress  -- individual install item callback
 *
 *  This callback will be called twice for each installed item,
 *  First when it is scheduled (val and max will both be 0) and
 *  then during the finalize step.
 */
typedef void    (*pfnXPIProgress)(const char* msg, PRInt32 val, PRInt32 max);



/** XPI_Init
 *
 *  call XPI_Init() to initialize XPCOM and the XPInstall
 *  engine, and to pass in your callback functions.
 *
 *  @param aProgramDir   directory to use as "program" directory. If NULL default
 *                       will be used -- the location of the calling executable.
 *                       Must be native filename format.
 *  @param aLogName      filename for log.
 *  @param progressCB    Called for each installed file
 *
 *  @returns    XPCOM status code indicating success or failure
 */
PR_EXTERN(nsresult) XPI_Init( const char*       aProgramDir,
                              const char*       aLogName,
                              pfnXPIProgress    progressCB );

/** XPI_Install
 *
 *  Install an XPI package from a local file
 *
 *  @param file     Native filename of XPI archive
 *  @param args     Install.arguments, if any
 *  @param flags    the old SmartUpdate trigger flags. This may go away
 *
 *  @returns status  Status from the installed archive
 */
PR_EXTERN(PRInt32) XPI_Install( const char*    file,
                                const char* args, 
                                long flags         );

/** XPI_Exit
 * 
 *  call when done to shut down the XPInstall and XPCOM engines
 *  and free allocated memory
 */
PR_EXTERN(void) XPI_Exit();

PR_END_EXTERN_C

