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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
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

/*
 * File:		aixwrap.c
 * Description:
 *     This file contains a single function, _MD_SELECT(), which simply
 *     invokes the select() function.  This file is used in an ugly
 *     hack to override the system select() function on AIX releases
 *     prior to 4.2.  (On AIX 4.2, we use a different mechanism to
 *     override select().)
 */

#ifndef AIX_RENAME_SELECT
#error aixwrap.c should only be used on AIX 3.2 or 4.1
#else

#include <sys/select.h>
#include <sys/poll.h>

int _MD_SELECT(int width, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    return select(width, r, w, e, t);
}

int _MD_POLL(void *listptr, unsigned long nfds, long timeout)
{
    return poll(listptr, nfds, timeout);
}

#endif /* AIX_RENAME_SELECT */
