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
	.text

/*
 * sol_getsp()
 *
 * Return the current sp (for debugging)
 */
	.global sol_getsp
sol_getsp:
	retl
   	mov     %sp, %o0


/*
 * sol_curthread()
 *
 * Return a unique identifier for the currently active thread.
 */
	.global sol_curthread
sol_curthread:
    retl
    mov %g7, %o0
                  

	.global __MD_FlushRegisterWindows
	.global _MD_FlushRegisterWindows

__MD_FlushRegisterWindows:
_MD_FlushRegisterWindows:

	ta	3
	ret
	restore

