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
    char * c = strFmt.toCString("");
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

UnicodeString & 
ICalComponent::propertyVToCalString(UnicodeString & sProp, 
                                    JulianPtrArray * v,
                                    UnicodeString & sResult) 
{
    UnicodeString * s_ptr;
    //UnicodeString s;
    //UnicodeString  sElement;
    t_int32 i;
    //UnicodeString & sPropTemp = sProp;
    //char * c;

    if (v != 0) {
        for (i = 0; i < v->GetSize(); i++)
        {
            s_ptr = (UnicodeString *) v->GetAt(i);
            
            //UnicodeString & sElement = *s_ptr;
            //if (FALSE) TRACE("size = %d", s_ptr->size());
            //c = sElement.toCString("");
            sResult += sProp;
            sResult += ':'; //ms_sCOLON_SYMBOL;
            sResult += (*s_ptr).toCString("");
            sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
            //UnicodeString & s;

            //if (FALSE) TRACE("sproptemp = %s", sPropTemp.toCString(""));
            
            //if (FALSE) TRACE("s = %s", s.toCString(""));
            
            //s += sPropTemp; 
            
            //s += ":"; 
            
            //s += sElement; 
            //s += "\r\n";
            ///////sResult += s;
            //sResult += multiLineFormat(s);
            //s.remove();
        }
        sResult = ICalProperty::multiLineFormat(sResult);
    }
    return sResult;
    //return sResult.toString();
}
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
  
void ICalComponent::deleteDateVector(JulianPtrArray * dateVector)
{
    t_int32 i;
    if (dateVector != 0) 
    {
        for (i = dateVector->GetSize() - 1; i >= 0; i--)
        {
            delete ((DateTime *) dateVector->GetAt(i));
        }
    }
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

