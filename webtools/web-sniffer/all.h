/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(WINDOWS)
#include "config.h"
#endif

#if HAVE_LIBPTHREAD
#undef HAVE_LIBTHREAD
#undef WINDOWS
#endif

#include <ctype.h>
#include <errno.h>
#include <float.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_LIBPTHREAD
#include <pthread.h>
#endif
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/stat.h>
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if WINDOWS
#include <windows.h>
#define ECONNREFUSED WSAECONNREFUSED
#define ECONNRESET WSAECONNRESET
#define ETIMEDOUT WSAETIMEDOUT
typedef int socklen_t;
#endif

typedef struct App App;

#include "addurl.h"
#include "app.h"
#include "file.h"
#include "hash.h"
#include "html.h"
#include "http.h"
#include "io.h"
#include "mime.h"
#include "net.h"
#include "thread.h"
#include "url.h"
#include "utils.h"
#include "view.h"
