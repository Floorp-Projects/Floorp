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

// prprtyfy.cpp
// John Sun
// 5:19 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "prprtyfy.h"
#include "prprty.h"
#include "bprprty.h"
#include "dprprty.h"
#include "duprprty.h"
#include "iprprty.h"
#include "pprprty.h"
#include "sprprty.h"


ICalPropertyFactory::ICalPropertyFactory()
{
}

//---------------------------------------------------------------------

ICalProperty *
ICalPropertyFactory::Make(ICalProperty::PropertyTypes aType, void * value, 
                          JulianPtrArray * parameters)
{

    PR_ASSERT(value != 0);
    if (value != 0)
    {
        switch (aType)
        {
        case ICalProperty::TEXT:
            //return (ICalProperty *) new StringProperty((UnicodeString *) value, parameters);
            return new StringProperty(*(UnicodeString *) value, parameters);
        case ICalProperty::DATETIME:
            return new DateTimeProperty(*(DateTime *) value, parameters);
        case ICalProperty::INTEGER:
            return new IntegerProperty(*(t_int32 *) value, parameters);
        case ICalProperty::PERIOD:
            return new PeriodProperty((Period *) value, parameters);
        case ICalProperty::DURATION:
            return new DurationProperty(*(Duration *) value, parameters);
        case ICalProperty::BOOLEAN:
            return new BooleanProperty(*(t_bool *) value, parameters);
        default:
            return 0;
        }
    }
    return 0;
}
//---------------------------------------------------------------------


