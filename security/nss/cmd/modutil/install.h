/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PK11INSTALL_H
#define PK11INSTALL_H

#include <prio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Pk11Install_ErrorHandler)(char *);

typedef enum {
	PK11_INSTALL_NO_ERROR=0,
	PK11_INSTALL_DIR_DOESNT_EXIST,
	PK11_INSTALL_FILE_DOESNT_EXIST,
	PK11_INSTALL_FILE_NOT_READABLE,
	PK11_INSTALL_ERROR_STRING,
	PK11_INSTALL_JAR_ERROR,
	PK11_INSTALL_NO_INSTALLER_SCRIPT,
	PK11_INSTALL_DELETE_TEMP_FILE,
	PK11_INSTALL_OPEN_SCRIPT_FILE,
	PK11_INSTALL_SCRIPT_PARSE,
	PK11_INSTALL_SEMANTIC,
	PK11_INSTALL_SYSINFO,
	PK11_INSTALL_NO_PLATFORM,
	PK11_INSTALL_BOGUS_REL_DIR,
	PK11_INSTALL_NO_MOD_FILE,
	PK11_INSTALL_ADD_MODULE,
	PK11_INSTALL_JAR_EXTRACT,
	PK11_INSTALL_DIR_NOT_WRITEABLE,
	PK11_INSTALL_CREATE_DIR,
	PK11_INSTALL_REMOVE_DIR,
	PK11_INSTALL_EXEC_FILE,
	PK11_INSTALL_WAIT_PROCESS,
	PK11_INSTALL_PROC_ERROR,
	PK11_INSTALL_USER_ABORT,
	PK11_INSTALL_UNSPECIFIED
} Pk11Install_Error;
#define PK11_INSTALL_SUCCESS PK11_INSTALL_NO_ERROR

/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ I n i t
 *
 * Does initialization that otherwise would be done on the fly.  Only
 * needs to be called by multithreaded apps, before they make any calls
 * to this library.
 */
void 
Pk11Install_Init();

/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ S e t E r r o r H a n d l e r
 *
 * Sets the error handler to be used by the library.  Returns the current
 * error handler function.
 */
Pk11Install_ErrorHandler
Pk11Install_SetErrorHandler(Pk11Install_ErrorHandler handler);


/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ R e l e a s e
 *
 * Releases static data structures used by the library.  Don't use the
 * library after calling this, unless you call Pk11Install_Init()
 * first.  This function doesn't have to be called at all unless you're
 * really anal about freeing memory before your program exits.
 */
void 
Pk11Install_Release();

/*************************************************************************
 *
 * P k 1 1 I n s t a l l _ D o I n s t a l l
 *
 * jarFile is the path of a JAR in the PKCS #11 module JAR format.
 * installDir is the directory relative to which files will be
 *   installed.
 * feedback is a file descriptor to which to write informative (not error)
 * status messages: what files are being installed, what modules are being
 * installed.  If feedback==NULL, no messages will be displayed.
 * If force != 0, interactive prompts will be suppressed.
 * If noverify == PR_TRUE, signatures won't be checked on the JAR file.
 */
Pk11Install_Error
Pk11Install_DoInstall(char *jarFile, const char *installDir,
	const char *tempDir, PRFileDesc *feedback, short force,
	PRBool noverify);

#ifdef __cplusplus
}
#endif

#endif /*PK11INSTALL_H*/
