/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

// icalcomp.cpp
// John Sun
// 10:41 AM February 9 1998

#include "stdafx.h"
#include "jdefines.h"

#include <unistring.h>
#include "jutility.h"
#include "icalcomp.h"
#include "ptrarray.h"
#include "datetime.h"
#include "prprty.h"
#include "prprtyfy.h"
#include "keyword.h"

//---------------------------------------------------------------------

ICalComponent::ICalComponent() {}

//---------------------------------------------------------------------

ICalComponent::~ICalComponent() {}

//---------------------------------------------------------------------

UnicodeString 
ICalComponent::toStringFmt(UnicodeString & strFmt) 
{
    UnicodeString s, into, dateFmt;
    t_int32 i,j;    
    t_int32 k, l, m;
    //if (FALSE) TRACE("strFmt = -%s-\r\n", strFmt.toCString(""));

    for ( i = 0; i < strFmt.size(); )
    {
	
    //-
	///  If there's a special formatting character,
	///  handle it.  Otherwise, just emit the supplied
	///  character.
	///
	    j = strFmt.indexOf('%', i);
        if ( -1 != j)
        {
            dateFmt = "";
	        if (j > i)
            {
	            s += strFmt.extractBetween(i,j,into);
            }
            i = j + 1;
            if ( strFmt.size() > i)
            {

                // BELOW is to handle this
                // %(MMMM dddd yyyy)B
                //k = l = j;
                k = strFmt.indexOf("(", i); l = strFmt.indexOf(")", i);
                m = strFmt.indexOf("%", i);
                if (k != -1 && l != -1 && 
                    (k < m || m == -1) && k > j && 
                    (l < m || m == -1) && l > j)
                {

                    if (l > k + 1)
                        dateFmt = strFmt.extractBetween(k + 1, l, dateFmt);

                    i = l + 1;
                }
                //if (FALSE) TRACE("dateFmt = -%s-\r\n", dateFmt.toCString(""));

                s += toStringChar(strFmt[(TextOffset) i], dateFmt); 
                i++;
            }
            //if (FALSE) TRACE("s = -%s-\r\n", s.toCString(""));
	    }
	    else
        {
             s += strFmt.extractBetween(i, strFmt.size(),into);
             break;
        }
    }
    return s;
}// end 
//---------------------------------------------------------------------
 
UnicodeString 
ICalComponent::format(UnicodeString & sComponentName,
                      UnicodeString & strFmt,
                      UnicodeString sFilterAttendee,
                      t_bool delegateRequest) 
{
    UnicodeString s, into;
    s = JulianKeyword::Instance()->ms_sBEGIN_WITH_COLON;
    s += sComponentName;
    s += JulianKeyword::Instance()->ms_sLINEBREAK;


    t_int32 i,j;    
    //char * c = strFmt.toCString("");
    //if (FALSE) TRACE("%s sComponentName, %s strFmt, %s sFilterAttendee, %d delegateRequest\r\n", sComponentName.toCString(""), strFmt.toCString(""), sFilterAttendee.toCString(""), delegateRequest);

    for ( i = 0; i < strFmt.size(); )
    {
	
    /*-
	***  If there's a special formatting character,
	***  handle it.  Otherwise, just emit the supplied
	***  character.
	**/
	    j = strFmt.indexOf('%', i);	
        if ( -1 != j)
        {
	        if (j > i)
            {
	            s += strFmt.extractBetween(i,j,into);
            }
            i = j + 1;
            if (strFmt.size() > i)
            {
                if (delegateRequest)
                {
                    s += formatChar(strFmt[(TextOffset) i], "", delegateRequest);
                }
                else if (sFilterAttendee.size() > 0)
                {
                    s += formatChar(strFmt[(TextOffset) i], sFilterAttendee, FALSE);
                }
                else 
                {
                    s += formatChar(strFmt[(TextOffset) i], "", FALSE); 
                }
                i++;
            }
	    }
	    else
        {
            s += strFmt.extractBetween(i, strFmt.size(),into);
            break;
        }
    }
    s += JulianKeyword::Instance()->ms_sEND_WITH_COLON;
    s += sComponentName;
    s += JulianKeyword::Instance()->ms_sLINEBREAK;
    return s;
}// end 

//---------------------------------------------------------------------

UnicodeString 
ICalComponent::componentToString(ICAL_COMPONENT ic)
{
    switch (ic)
    {
    case ICAL_COMPONENT_VEVENT: return JulianKeyword::Instance()->ms_sVEVENT;
    case ICAL_COMPONENT_VTODO: return JulianKeyword::Instance()->ms_sVTODO;
    case ICAL_COMPONENT_VJOURNAL: return JulianKeyword::Instance()->ms_sVJOURNAL;
    case ICAL_COMPONENT_VFREEBUSY: return JulianKeyword::Instance()->ms_sVFREEBUSY;
    case ICAL_COMPONENT_VTIMEZONE: return JulianKeyword::Instance()->ms_sVTIMEZONE;
    case ICAL_COMPONENT_VALARM: return JulianKeyword::Instance()->ms_sVALARM;
    case ICAL_COMPONENT_TZPART: return JulianKeyword::Instance()->ms_sTZPART;
    default:
        return "";
    }
}

//---------------------------------------------------------------------

ICalComponent::ICAL_COMPONENT
ICalComponent::stringToComponent(UnicodeString & s, t_bool & error)
{
    s.toUpper();
    t_int32 hashCode = s.hashCode();
    error = FALSE;

    if (JulianKeyword::Instance()->ms_ATOM_VEVENT == hashCode) return ICAL_COMPONENT_VEVENT;
    else if (JulianKeyword::Instance()->ms_ATOM_VTODO == hashCode) return ICAL_COMPONENT_VTODO;
    else if (JulianKeyword::Instance()->ms_ATOM_VJOURNAL == hashCode) return ICAL_COMPONENT_VJOURNAL;
    else if (JulianKeyword::Instance()->ms_ATOM_VFREEBUSY == hashCode) return ICAL_COMPONENT_VFREEBUSY;
    else if (JulianKeyword::Instance()->ms_ATOM_VTIMEZONE == hashCode) return ICAL_COMPONENT_VTIMEZONE;
    else if (JulianKeyword::Instance()->ms_ATOM_VALARM == hashCode) return ICAL_COMPONENT_VALARM;
    else if (JulianKeyword::Instance()->ms_ATOM_TZPART == hashCode) return ICAL_COMPONENT_TZPART;
    else 
    {
        error = TRUE;
        return ICAL_COMPONENT_TZPART;
    }
}

//---------------------------------------------------------------------

t_bool ICalComponent::propertyNameToKeyLetter(UnicodeString & propertyName,
                                              t_int32 & outLetter)
{
    propertyName.toUpper();
    t_int32 hashCode = propertyName.hashCode();
    t_bool retStatus = TRUE;
    outLetter = ' ';

    if (JulianKeyword::Instance()->ms_ATOM_ATTENDEE == hashCode) outLetter = ms_cAttendees;    
    else if (JulianKeyword::Instance()->ms_ATOM_ATTACH == hashCode) outLetter = ms_cAttach;
    else if (JulianKeyword::Instance()->ms_ATOM_CATEGORIES == hashCode) outLetter = ms_cCategories;
    else if (JulianKeyword::Instance()->ms_ATOM_CLASS == hashCode) outLetter = ms_cClass;
    else if (JulianKeyword::Instance()->ms_ATOM_COMMENT == hashCode) outLetter = ms_cComment;
    else if (JulianKeyword::Instance()->ms_ATOM_COMPLETED == hashCode) outLetter = ms_cCompleted;
    else if (JulianKeyword::Instance()->ms_ATOM_CONTACT == hashCode) outLetter = ms_cContact;
    else if (JulianKeyword::Instance()->ms_ATOM_CREATED == hashCode) outLetter = ms_cCreated;
    else if (JulianKeyword::Instance()->ms_ATOM_DTEND == hashCode) outLetter = ms_cDTEnd;
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTART == hashCode) outLetter = ms_cDTStart;
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTAMP == hashCode) outLetter = ms_cDTStamp;
    else if (JulianKeyword::Instance()->ms_ATOM_DESCRIPTION == hashCode) outLetter = ms_cDescription;
    else if (JulianKeyword::Instance()->ms_ATOM_DUE == hashCode) outLetter = ms_cDue;
    else if (JulianKeyword::Instance()->ms_ATOM_DURATION == hashCode) outLetter = ms_cDuration;
    else if (JulianKeyword::Instance()->ms_ATOM_EXDATE == hashCode) outLetter = ms_cExDate;
    else if (JulianKeyword::Instance()->ms_ATOM_EXRULE == hashCode) outLetter = ms_cExRule;
    else if (JulianKeyword::Instance()->ms_ATOM_FREEBUSY == hashCode) outLetter = ms_cFreebusy;
    else if (JulianKeyword::Instance()->ms_ATOM_GEO == hashCode) outLetter = ms_cGEO;
    else if (JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED == hashCode) outLetter = ms_cLastModified;
    else if (JulianKeyword::Instance()->ms_ATOM_LOCATION == hashCode) outLetter = ms_cLocation;
    else if (JulianKeyword::Instance()->ms_ATOM_ORGANIZER == hashCode) outLetter = ms_cOrganizer;
    else if (JulianKeyword::Instance()->ms_ATOM_PERCENTCOMPLETE == hashCode) outLetter = ms_cPercentComplete;
    else if (JulianKeyword::Instance()->ms_ATOM_PRIORITY == hashCode) outLetter = ms_cPriority;
    else if (JulianKeyword::Instance()->ms_ATOM_RDATE == hashCode) outLetter = ms_cRDate;
    else if (JulianKeyword::Instance()->ms_ATOM_RRULE == hashCode) outLetter = ms_cRRule;
    else if (JulianKeyword::Instance()->ms_ATOM_RECURRENCEID == hashCode) outLetter = ms_cRecurrenceID;
    else if (JulianKeyword::Instance()->ms_ATOM_RELATEDTO == hashCode) outLetter = ms_cRelatedTo;
    else if (JulianKeyword::Instance()->ms_ATOM_REPEAT == hashCode) outLetter = ms_cRepeat;
    else if (JulianKeyword::Instance()->ms_ATOM_REQUESTSTATUS == hashCode) outLetter = ms_cRequestStatus;
    else if (JulianKeyword::Instance()->ms_ATOM_RESOURCES == hashCode) outLetter = ms_cResources;
    else if (JulianKeyword::Instance()->ms_ATOM_SEQUENCE == hashCode) outLetter = ms_cSequence;
    else if (JulianKeyword::Instance()->ms_ATOM_STATUS == hashCode) outLetter = ms_cStatus;
    else if (JulianKeyword::Instance()->ms_ATOM_SUMMARY == hashCode) outLetter = ms_cSummary;
    else if (JulianKeyword::Instance()->ms_ATOM_TRANSP == hashCode) outLetter = ms_cTransp;
    //else if (JulianKeyword::Instance()->ms_ATOM_TRIGGER == hashCode) outLetter = ms_cTrigger;
    else if (JulianKeyword::Instance()->ms_ATOM_UID == hashCode) outLetter = ms_cUID;
    else if (JulianKeyword::Instance()->ms_ATOM_URL == hashCode) outLetter = ms_cURL;
    //else if (JulianKeyword::Instance()->ms_ATOM_TZOFFSET == hashCode) outLetter = ms_cTZOffset;
    else if (JulianKeyword::Instance()->ms_ATOM_TZOFFSETTO == hashCode) outLetter = ms_cTZOffsetTo;
    else if (JulianKeyword::Instance()->ms_ATOM_TZOFFSETFROM == hashCode) outLetter = ms_cTZOffsetFrom;
    else if (JulianKeyword::Instance()->ms_ATOM_TZNAME == hashCode) outLetter = ms_cTZName;
    //else if (JulianKeyword::Instance()->ms_ATOM_DAYLIGHT == hashCode) outLetter = ms_cDayLight;
    //else if (JulianKeyword::Instance()->ms_ATOM_STANDARD == hashCode) outLetter = ms_cStandard;
    else if (JulianKeyword::Instance()->ms_ATOM_TZURL == hashCode) outLetter = ms_cTZURL;
    else if (JulianKeyword::Instance()->ms_ATOM_TZID == hashCode) outLetter = ms_cTZID;
    else 
    {
        retStatus = FALSE;
    }
    return retStatus;
}
//---------------------------------------------------------------------

UnicodeString & 
ICalComponent::makeFormatString(char ** ppsPropList, t_int32 iPropCount,
                                UnicodeString & out)
{
    // TODO: figure out what to do if no properties?
    t_int32 i = 0;
    t_int32 keyLetter = ' ';
    t_bool bFoundProperty = FALSE;
    char *cProp = 0;
    UnicodeString u;
    UnicodeString usProp;

    out = "";
    if (iPropCount > 0 && ppsPropList != 0)
    {
        for (i = 0; i < iPropCount; i++)
        {
            cProp = ppsPropList[i];
            usProp = cProp;
            bFoundProperty = ICalComponent::propertyNameToKeyLetter(usProp, keyLetter);
            if (bFoundProperty)
            {
                out += "%";
                // TODO: dangerous
                out += (char) (keyLetter);
            }
        }
    } else
    {
        // given an empty propList or a zero propCount, 
        // return all properties + alarms
        out = "%v%a%k%c%K%G%H%t%e%B%C%i%F%D%X%E%Y%O%M%L%J%P%p%x%y%R%o%A%T%r%s%g%S%h%U%u%Z%d%f%n%Q%I%V%w";
    }
    return out;
}   
//---------------------------------------------------------------------

void ICalComponent::deleteUnicodeStringVector(JulianPtrArray * stringVector)
{
    t_int32 i;
    if (stringVector != 0) 
    {
        for (i = stringVector->GetSize() - 1; i >= 0; i--)
        {
            delete ((UnicodeString *) stringVector->GetAt(i));
        }
    }
}
//---------------------------------------------------------------------

void 
ICalComponent::deleteICalComponentVector(JulianPtrArray * componentVector)
{
    t_int32 i;
    if (componentVector != 0) 
    {
        for (i = componentVector->GetSize() - 1; i >= 0; i--)
        {
            delete ((ICalComponent *) componentVector->GetAt(i));
        }
    }
}

//---------------------------------------------------------------------

void
ICalComponent::cloneICalComponentVector(JulianPtrArray * out, 
                                        JulianPtrArray * toClone)
{
    if (out != 0)
    {
        if (toClone != 0)
        {
            t_int32 i;
            ICalComponent * comp;
            ICalComponent * clone;
            for (i = 0; i < toClone->GetSize(); i++)
            {
                comp = (ICalComponent *) toClone->GetAt(i);
                clone = comp->clone(0);
                out->Add(clone);
            }
        }
    }
}
//---------------------------------------------------------------------

#if 0
void 
ICalComponent::setDateTimeValue(ICalProperty ** dateTimePropertyPtr,
                                DateTime inVal, 
                                JulianPtrArray * inParameters)
{ 
    PR_ASSERT(dateTimePropertyPtr != 0);
    if (dateTimePropertyPtr != 0)
    {
        if (((ICalProperty *) (*dateTimePropertyPtr)) == 0)
        {
            ((ICalProperty *) (*dateTimePropertyPtr)) =
                ICalPropertyFactory::Make(ICalProperty::DATETIME,
                    (void *) &inVal, inParameters);
        }
        else
        {
            ((ICalProperty *) (*dateTimePropertyPtr))->setValue((void *) &inVal);
            ((ICalProperty *) (*dateTimePropertyPtr))->setParameters(inParameters);
        }
    }
} 

//---------------------------------------------------------------------

void ICalComponent::getDateTimeValue(ICalProperty ** dateTimePropertyPtr,
                                     DateTime & outVal) 
{
    PR_ASSERT(dateTimePropertyPtr != 0);
    if (dateTimePropertyPtr != 0)
    {
        if ((ICalProperty *)(*dateTimePropertyPtr) == 0)
            outVal.setTime(-1);
        else
            outVal = *((DateTime *) ((ICalProperty *)(*dateTimePropertyPtr))->getValue());
    }
    else
    {
        outVal.setTime(-1);
    }
}
//---------------------------------------------------------------------

void 
ICalComponent::setStringValue(ICalProperty ** stringPropertyPtr,
                              UnicodeString inVal, 
                              JulianPtrArray * inParameters)
{ 
    PR_ASSERT(stringPropertyPtr != 0);
    if (stringPropertyPtr != 0)
    {
        if (((ICalProperty *) (*stringPropertyPtr)) == 0)
        {
            ((ICalProperty *) (*stringPropertyPtr)) =
                ICalPropertyFactory::Make(ICalProperty::TEXT,
                    (void *) &inVal, inParameters);
        }
        else
        {
            ((ICalProperty *) (*stringPropertyPtr))->setValue((void *) &inVal);
            ((ICalProperty *) (*stringPropertyPtr))->setParameters(inParameters);
        }
    }
} 

//---------------------------------------------------------------------

void 
ICalComponent::getStringValue(ICalProperty ** stringPropertyPtr,
                              UnicodeString & outVal) 
{
    PR_ASSERT(stringPropertyPtr != 0);
    if (stringPropertyPtr != 0)
    {
        if ((ICalProperty *)(*stringPropertyPtr) == 0)
            outVal = "";
        else
            outVal = *((UnicodeString *) ((ICalProperty *)(*stringPropertyPtr))->getValue());
    }
    else
    {
        outVal = "";
    }
}

//---------------------------------------------------------------------

void 
ICalComponent::setIntegerValue(ICalProperty ** integerPropertyPtr,
                              t_int32 inVal, JulianPtrArray * inParameters)
{ 
    PR_ASSERT(integerPropertyPtr != 0);
    if (integerPropertyPtr != 0)
    {
        if (((ICalProperty *) (*integerPropertyPtr)) == 0)
        {
            ((ICalProperty *) (*integerPropertyPtr)) =
                ICalPropertyFactory::Make(ICalProperty::INTEGER,
                    (void *) &inVal, inParameters);
        }
        else
        {
            ((ICalProperty *) (*integerPropertyPtr))->setValue((void *) &inVal);
            ((ICalProperty *) (*integerPropertyPtr))->setParameters(inParameters);
        }
    }
} 

//---------------------------------------------------------------------

void 
ICalComponent::getIntegerValue(ICalProperty ** integerPropertyPtr,
                              t_int32 & outVal) 
{
    PR_ASSERT(integerPropertyPtr != 0);
    if (integerPropertyPtr != 0)
    {
        if ((ICalProperty *)(*integerPropertyPtr) == 0)
            outVal = -1;
        else
            outVal = *((t_int32 *) ((ICalProperty *)(*integerPropertyPtr))->getValue());
    }
    else
    {
        outVal = -1;
    }
}
#endif /* #if 0 */
//---------------------------------------------------------------------

void ICalComponent::internalSetProperty(ICalProperty ** propertyPtr,
                                        ICalProperty * replaceProp,
                                        t_bool bForceOverwriteOnEmpty)
{
    PR_ASSERT(propertyPtr != 0);
    ICalProperty * prop = 0;
    prop = (ICalProperty *) (*(propertyPtr));
    t_bool bOverwrite = TRUE;
    if (replaceProp == 0 && !bForceOverwriteOnEmpty)
    {
        bOverwrite = FALSE;
    }
    if (bOverwrite)
    {
        if (prop != 0)
        {
            delete prop;
            prop = 0;
            (*(propertyPtr)) = 0;
        }
        if (replaceProp != 0)
        {
            prop = replaceProp->clone(0);
            (*(propertyPtr)) = prop;
        }
    }
}

//---------------------------------------------------------------------

void ICalComponent::internalSetPropertyVctr(JulianPtrArray ** propertyVctrPtr,
                                            JulianPtrArray * replaceVctr,
                                            t_bool bAddInsteadOfOverwrite,
                                            t_bool bForceOverwriteOnEmpty)
{

    // delete the contents of the old vector
    // delete the old vector
    // create a new vector if replaceVctr != 0
    // clone contents of replaceVctr.

    PR_ASSERT(propertyVctrPtr != 0);
    JulianPtrArray * propVctr = 0;
    propVctr = (JulianPtrArray *) (*(propertyVctrPtr));
    t_bool bOverwrite = TRUE;

    // don't overwrite if overwrite flag is false && replaceVctr is empty or null.
    if (bAddInsteadOfOverwrite)
    {
        bOverwrite = FALSE;
    }
    else if ((replaceVctr == 0 || replaceVctr->GetSize() == 0) && !bForceOverwriteOnEmpty)
    {
        bOverwrite = FALSE;
    }
    if (bOverwrite)
    {
        if (propVctr != 0)
        {
            ICalProperty::deleteICalPropertyVector(propVctr);
            delete propVctr;
            propVctr = 0;
            (*(propertyVctrPtr)) = 0;
        }
        if (replaceVctr != 0)
        {
            propVctr = new JulianPtrArray();
            PR_ASSERT(propVctr != 0);
            (*(propertyVctrPtr)) = propVctr;
            ICalProperty::CloneICalPropertyVector(replaceVctr, propVctr, 0);
        }
    }
    else if (bAddInsteadOfOverwrite)
    {
        t_int32 i;
        if (replaceVctr != 0)
        {
            ICalProperty * ip = 0;
            for (i = 0; i < replaceVctr->GetSize(); i++)
            {
                ip = ((ICalProperty *) replaceVctr->GetAt(i))->clone(0);
                if (ip != 0)
                {
                    propVctr->Add(ip);
                }
            }
        }
    }
}
//---------------------------------------------------------------------

void ICalComponent::internalSetXTokensVctr(JulianPtrArray ** xTokensVctrPtr,
                                           JulianPtrArray * replaceVctr,
                                           t_bool bAddInsteadOfOverwrite,
                                           t_bool bForceOverwriteOnEmpty)
{

    // delete the contents of the old vector
    // delete the old vector
    // create a new vector if replaceVctr != 0
    // clone contents of replaceVctr.

    PR_ASSERT(xTokensVctrPtr != 0);
    JulianPtrArray * xTokensVctr = 0;
    xTokensVctr = (JulianPtrArray *) (*(xTokensVctrPtr));
    t_bool bOverwrite = TRUE;

    // don't overwrite if overwrite flag is false && replaceVctr is empty or null.
    if (bAddInsteadOfOverwrite)
    {
        bOverwrite = FALSE;
    }
    else if ((replaceVctr == 0 || replaceVctr->GetSize() == 0) && !bForceOverwriteOnEmpty)
    {
        bOverwrite = FALSE;
    }
    if (bOverwrite)
    {
        if (xTokensVctr != 0)
        {
            ICalComponent::deleteUnicodeStringVector(xTokensVctr);
            delete xTokensVctr;
            xTokensVctr = 0;
            (*(xTokensVctrPtr)) = 0;
        }
        if (replaceVctr != 0)
        {
            xTokensVctr = new JulianPtrArray();
            PR_ASSERT(xTokensVctr != 0);
            (*(xTokensVctrPtr)) = xTokensVctr;
            ICalProperty::CloneUnicodeStringVector(replaceVctr, xTokensVctr);
        }
    }
    else if (bAddInsteadOfOverwrite)
    {
        t_int32 i;
        if (replaceVctr != 0)
        {
            UnicodeString * us = 0;
            UnicodeString u;
            for (i = 0; i < replaceVctr->GetSize(); i++)
            {
                u = *((UnicodeString *) replaceVctr->GetAt(i));
                us = new UnicodeString(u);
                if (us != 0)
                {
                    xTokensVctr->Add(us);
                }
            }
        }
    }
}

