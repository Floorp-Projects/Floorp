/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
#ifndef _JAVASEC_H_
#define _JAVASEC_H_

#include "jritypes.h"

JRI_PUBLIC_API(const char *) java_netscape_security_getPrincipals(const char *charSetName);

JRI_PUBLIC_API(void) java_netscape_security_getPrivilegeDescs(const char *charSetName,
                                                              char *prinName, 
                                                              char** forever, 
                                                              char** session, 
                                                              char **denied);

JRI_PUBLIC_API(void) java_netscape_security_getTargetDetails(const char *charSetName,
                                                             char* targetName, 
                                                             char** details, 
                                                             char **risk);

JRI_PUBLIC_API(jint) java_netscape_security_removePrincipal(const char *charSetName, 
                                                            char *prinName);

JRI_PUBLIC_API(jint) java_netscape_security_removePrivilege(const char *charSetName, 
                                                            char *prinName, 
                                                            char *targetName);

JRI_PUBLIC_API(void) java_netscape_security_savePrivilege(int privilege);

#endif /* _JAVASEC_H_ */
