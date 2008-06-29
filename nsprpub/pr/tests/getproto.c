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
 *************************************************************************
 *
 * File: getproto.c
 *
 * A test program for PR_GetProtoByName and PR_GetProtoByNumber
 *
 *************************************************************************
 */

#include "plstr.h"
#include "plerror.h"
#include "prinit.h"
#include "prprf.h"
#include "prnetdb.h"
#include "prerror.h"

int main()
{
    PRFileDesc *prstderr = PR_GetSpecialFD(PR_StandardError);
    PRBool failed = PR_FALSE;
    PRProtoEnt proto;
    char buf[2048];
    PRStatus rv;

    PR_STDIO_INIT();
    rv = PR_GetProtoByName("tcp", buf, sizeof(buf), &proto);
    if (PR_FAILURE == rv) {
        failed = PR_TRUE;
        PL_FPrintError(prstderr, "PR_GetProtoByName failed");
    }
    else if (6 != proto.p_num) {
        PR_fprintf(
            prstderr,"tcp is usually 6, but is %d on this machine\n",
            proto.p_num);
    }
    else PR_fprintf(prstderr, "tcp is protocol number %d\n", proto.p_num);

    rv = PR_GetProtoByName("udp", buf, sizeof(buf), &proto);
    if (PR_FAILURE == rv) {
        failed = PR_TRUE;
        PL_FPrintError(prstderr, "PR_GetProtoByName failed");
    }
    else if (17 != proto.p_num) {
        PR_fprintf(
            prstderr, "udp is usually 17, but is %d on this machine\n",
            proto.p_num);
    }
    else PR_fprintf(prstderr, "udp is protocol number %d\n", proto.p_num);

    rv = PR_GetProtoByNumber(6, buf, sizeof(buf), &proto);
    if (PR_FAILURE == rv) {
        failed = PR_TRUE;
        PL_FPrintError(prstderr, "PR_GetProtoByNumber failed");
    }
    else if (PL_strcmp("tcp", proto.p_name)) {
        PR_fprintf(
            prstderr, "Protocol number 6 is usually tcp, but is %s"
            " on this platform\n", proto.p_name);
    }
    else PR_fprintf(prstderr, "Protocol number 6 is %s\n", proto.p_name);

    rv = PR_GetProtoByNumber(17, buf, sizeof(buf), &proto);
    if (PR_FAILURE == rv) {
        failed = PR_TRUE;
        PL_FPrintError(prstderr, "PR_GetProtoByNumber failed");
    }
    else if (PL_strcmp("udp", proto.p_name)) {
        PR_fprintf(
            prstderr, "Protocol number 17 is usually udp, but is %s"
            " on this platform\n", proto.p_name);
    }
    else PR_fprintf(prstderr, "Protocol number 17 is %s\n", proto.p_name);

    PR_fprintf(prstderr, (failed) ? "FAILED\n" : "PASSED\n");
    return (failed) ? 1 : 0;
}
