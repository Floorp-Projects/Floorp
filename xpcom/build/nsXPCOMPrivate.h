/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOMPrivate_h__
#define nsXPCOMPrivate_h__

#include "nscore.h"
#include "nsXPCOM.h"

/**
 * During this shutdown notification all threads which run XPCOM code must
 * be joined.
 */
#define NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID "xpcom-shutdown-threads"

/**
 * During this shutdown notification all module loaders must unload XPCOM
 * modules.
 */
#define NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID "xpcom-shutdown-loaders"

// PUBLIC
namespace mozilla {

/**
 * Shutdown XPCOM. You must call this method after you are finished
 * using xpcom.
 *
 * @param aServMgr          The service manager which was returned by NS_InitXPCOM.
 *                          This will release servMgr.  You may pass null.
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during shutdown
 *
 */
nsresult
ShutdownXPCOM(nsIServiceManager* aServMgr);

void SetICUMemoryFunctions();

/**
 * C++ namespaced version of NS_LogTerm.
 */
void LogTerm();

} // namespace mozilla


/* XPCOM Specific Defines
 *
 * XPCOM_DLL              - name of the loadable xpcom library on disk.
 * XUL_DLL                - name of the loadable XUL library on disk
 * XPCOM_SEARCH_KEY       - name of the environment variable that can be
 *                          modified to include additional search paths.
 * GRE_CONF_NAME          - Name of the GRE Configuration file
 */

#if defined(XP_WIN32)

#define XPCOM_SEARCH_KEY  "PATH"
#define GRE_CONF_NAME     "gre.config"
#define GRE_WIN_REG_LOC   L"Software\\mozilla.org\\GRE"
#define XPCOM_DLL         XUL_DLL
#define LXPCOM_DLL        LXUL_DLL
#define XUL_DLL           "xul.dll"
#define LXUL_DLL          L"xul.dll"

#else // Unix
#include <limits.h> // for PATH_MAX

#define XPCOM_DLL         XUL_DLL

// you have to love apple..
#ifdef XP_MACOSX
#define XPCOM_SEARCH_KEY  "DYLD_LIBRARY_PATH"
#define GRE_FRAMEWORK_NAME "XUL.framework"
#define XUL_DLL            "XUL"
#else
#define XPCOM_SEARCH_KEY  "LD_LIBRARY_PATH"
#define XUL_DLL   "libxul" MOZ_DLL_SUFFIX
#endif

#define GRE_CONF_NAME ".gre.config"
#define GRE_CONF_PATH "/etc/gre.conf"
#define GRE_CONF_DIR  "/etc/gre.d"
#define GRE_USER_CONF_DIR ".gre.d"
#endif

#if defined(XP_WIN)
  #define XPCOM_FILE_PATH_SEPARATOR       "\\"
  #define XPCOM_ENV_PATH_SEPARATOR        ";"
#elif defined(XP_UNIX)
  #define XPCOM_FILE_PATH_SEPARATOR       "/"
  #define XPCOM_ENV_PATH_SEPARATOR        ":"
#else
  #error need_to_define_your_file_path_separator_and_illegal_characters
#endif

#ifdef AIX
#include <sys/param.h>
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#elif defined(_MAX_PATH)
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

// Needed by the IPC layer from off the main thread
extern char16_t* gGREBinPath;

namespace mozilla {
namespace services {

/**
 * Clears service cache, sets gXPCOMShuttingDown
 */
void Shutdown();

} // namespace services
} // namespace mozilla

#endif
