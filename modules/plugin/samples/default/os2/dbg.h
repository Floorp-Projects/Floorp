/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corp.
 */

#ifndef __DBG_HPP_
#define __DBG_HPP_

#ifdef _DEBUG

void __cdecl dbgOut(LPSTR format, ...);
#define dbgOut1(x)        dbgOut(x)
#define dbgOut2(x,y)      dbgOut(x, y)
#define dbgOut3(x,y,z)      dbgOut(x, y, z)
#define dbgOut4(x,y,z,t)    dbgOut(x, y, z, t)
#define dbgOut5(x,y,z,t,u)    dbgOut(x, y, z, t, u)
#define dbgOut6(x,y,z,t,u,v)  dbgOut(x, y, z, t, u, v)
#define dbgOut7(x,y,z,t,u,v, a)  dbgOut(x, y, z, t, u, v, a)
#define dbgOut8(x,y,z,t,u,v, a, b)  dbgOut(x, y, z, t, u, v, a, b)

#else

#define dbgOut1(x)        ((void)0)
#define dbgOut2(x,y)      ((void)0)
#define dbgOut3(x,y,z)      ((void)0)
#define dbgOut4(x,y,z,t)    ((void)0)
#define dbgOut5(x,y,z,t,u)    ((void)0)
#define dbgOut6(x,y,z,t,u,v)  ((void)0)
#define dbgOut7(x,y,z,t,u,v,a)  ((void)0)
#define dbgOut8(x,y,z,t,u,v,a,b)  ((void)0)

#endif

#endif
