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

// icompfy.cpp
// John Sun
// 10:46 AM February 20 1998

#include "stdafx.h"
#include "jdefines.h"

#include "icompfy.h"
#include "vevent.h"
#include "vtodo.h"
#include "vjournal.h"
#include "vfrbsy.h"
#include "vtimezne.h"
#include "keyword.h"
//---------------------------------------------------------------------

ICalComponentFactory::ICalComponentFactory()
{
}

//---------------------------------------------------------------------

ICalComponent * 
ICalComponentFactory::Make(UnicodeString & name, JLog * initLog)
{
    ICalComponent * ret;

    t_int32 hashCode = name.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_VEVENT == hashCode)
    {
        ret = (ICalComponent *) new VEvent(initLog);    
        PR_ASSERT(ret != 0);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_VTODO == hashCode)
    {
        ret = (ICalComponent *) new VTodo(initLog);
        PR_ASSERT(ret != 0);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_VJOURNAL == hashCode)
    {
        ret = (ICalComponent *) new VJournal(initLog);
        PR_ASSERT(ret != 0);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_VFREEBUSY == hashCode)
    {
        ret = (ICalComponent *) new VFreebusy(initLog);    
        PR_ASSERT(ret != 0);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_VTIMEZONE == hashCode)
    {
        ret = (ICalComponent *) new VTimeZone(initLog);
        PR_ASSERT(ret != 0);
    }
    else 
    {
        ret = 0;
    }
    return ret;
}
//---------------------------------------------------------------------

