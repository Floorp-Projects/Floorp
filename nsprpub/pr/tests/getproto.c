/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
