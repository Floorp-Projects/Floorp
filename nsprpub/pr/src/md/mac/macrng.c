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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#if 0 /* XXX what the flip is this all about? No MAC Wizards here. */
#ifdef notdef
#include "xp_core.h"
#include "xp_file.h"
#endif
#endif /* 0 */

/* XXX are all these headers required for a call to TickCount()? */
#include <Events.h>
#include <OSUtils.h>
#include <QDOffscreen.h>
#include <PPCToolbox.h>
#include <Processes.h>
#include <LowMem.h>
#include "primpl.h"

extern PRSize _PR_MD_GetRandomNoise( buf, size )
{
    uint32 c = TickCount();
    return _pr_CopyLowBits((void *)buf, size,  &c, sizeof(c));
}
