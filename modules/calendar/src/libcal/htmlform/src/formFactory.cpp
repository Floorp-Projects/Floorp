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

#include "jdefines.h"

#include <calendar.h>
#include <gregocal.h>
#include <datefmt.h>
#include <time.h>
#include "smpdtfmt.h"
#include <simpletz.h>
#include "datetime.h"
#include "jutility.h"
#include "duration.h"

#include "datetime.h"
#include "duration.h"
#include "period.h"

#include <time.h>
#include "ptrarray.h"
#include "dategntr.h"
#include "rcrrence.h"
#include "deftgntr.h"
#include "byhgntr.h"
#include "bymgntr.h"
#include "bymdgntr.h"
#include "bymogntr.h"
#include "bywngntr.h"
#include "byydgntr.h"
#include "bydmgntr.h"
#include "bydwgntr.h"
#include "bydygntr.h"
#include "smpdtfmt.h"

#include "keyword.h"
#include "attendee.h"
#include "vevent.h"
#include "icalsrdr.h"
#include "icalfrdr.h"
#include "prprty.h"
#include "icalprm.h"
#include "freebusy.h"
#include "vfrbsy.h"
#include "nscal.h"

#include "form.h"
#include "formFactory.h"

JulianFormFactory::JulianFormFactory()
{
	firstCalendar = nil;
	ServerProxy = nil;
	jf = nil;
	thisICalComp = nil;
	serverICalComp = nil;
	isEvent = FALSE;
	isFreeBusy = FALSE;
	cb = nil;
	SetDetail(FALSE);
}

JulianFormFactory::JulianFormFactory(NSCalendar& imipCal, JulianServerProxy* jsp)
{
	firstCalendar = &imipCal;
	ServerProxy = jsp;
	jf = nil;
	thisICalComp = nil;
	serverICalComp = nil;
	isEvent = FALSE;
	isFreeBusy = FALSE;
	cb = nil;
	SetDetail(FALSE);
}

JulianFormFactory::JulianFormFactory(NSCalendar& imipCal, JulianForm& hostForm, pJulian_Form_Callback_Struct callbacks)
{
	firstCalendar = &imipCal;
	jf = &hostForm;
	
	thisICalComp = nil;
	serverICalComp = nil;
	isEvent = FALSE;
	isFreeBusy = FALSE;
	cb = callbacks;
	SetDetail(FALSE);
	ServerProxy = nil;
}

JulianFormFactory::~JulianFormFactory()
{
}

/*
** getHTML
**
** Call after setting firstCalendar. It will produce an Unicode
** string with html that should be inserted into a real html file.
** On any errors an empty string is returened.
**
*/
XP_Bool
JulianFormFactory::getHTML(UnicodeString* htmlstorage, XP_Bool want_detail)
{
UnicodeString ny("Not Yet");
UnicodeString im("<B>Invalid method</B>");
int32 icalIndex = 0;	// Index into venent or vfreebusy array
int32 icalMaxIndex;
char*		temp = nil;
char*		urlcallback = nil;

	// Set up the storage for the html
	s = htmlstorage;

	// Return an empty string on any error.
	// This case: firstCalendar isn't set. It's needed to
	// find what vevents, freebusy etc to parse.
	if (firstCalendar == nil) return TRUE;

	JulianPtrArray * e = firstCalendar->getEvents();
	JulianPtrArray * f = firstCalendar->getVFreebusy();

	// Look for only for one type of ICalComponent.
	// Start with vEvents and work down from there.
	// There is a bool for each ICalComponent class
	// that is handled.
	// If there isn't anything good here, set thisICalComp
	// to nil.
	if (e)
	{
		isEvent = TRUE;
		maxical = icalMaxIndex = e->GetSize() - 1;
	} else
	if (f)
	{
		isFreeBusy = TRUE;
		maxical = icalMaxIndex = f->GetSize() - 1;
	} else
	{
		thisICalComp = nil;
	}

	// Return an empty string on any error.
	// This case: There isn't any events, freebusy , etc
	// and there should be at least one of these types.
	if (icalMaxIndex < 0) return TRUE;

	SetDetail(want_detail);

	/*
	** Test Code to get callback to work
	*/
	if (cb && cb->callbackurl)
	{
		urlcallback = (*cb->callbackurl)((NET_CallbackURLFunc) (want_detail ? jf_detail_callback : jf_callback), (void *)jf);
		temp = new char[strlen(urlcallback) + Start_HTML.size()];
		sprintf(temp, Start_HTML.toCString(""), urlcallback);
	} else
		temp = Start_HTML.toCString("");
	doAddHTML(temp);
	if (cb && cb->callbackurl && temp)
	{
		delete temp;
	}

	if (icalMaxIndex > 0 && !detail)
	{
		doHeaderMuiltStart();
	}

	while (icalIndex <= icalMaxIndex)
	{
		if (isEvent)
		{
			thisICalComp = (VEvent *)e->GetAt(icalIndex++);
		} else 
		if (isFreeBusy)
		{
			thisICalComp = (VFreebusy *)f->GetAt(icalIndex++);
		} else
			break;	// thisICalComp needs to be set to something. It's
					// a problem if it's nil.

		// If in detail mode, check to see if there is a server version of this icalcomponent.
		// Otherwise set it to nil to show that this is the first time this has been seen.
		if (detail && ServerProxy)
		{
			serverICalComp = ServerProxy->getByUid(nil);
		} else {
			serverICalComp = nil;
		}

		if (icalMaxIndex > 1 && !detail)
		{
			doHeaderMuilt();
		} else
		// Find the right html form based on the method
		switch (firstCalendar->getMethod())
		{
			case NSCalendar::METHOD_PUBLISH:
					if (isEvent) 
					{
						HandlePublishVEvent();
					}

					if (isFreeBusy)
					{
						HandlePublishVFreeBusy(TRUE);
					}
					break;	

			case NSCalendar::METHOD_REQUEST:
			case NSCalendar::METHOD_ADD:

					if (isEvent) 
					{
						HandleRequestVEvent();
					}

					if (isFreeBusy)
					{
						HandleRequestVFreeBusy();
					}
					break;

			case NSCalendar::METHOD_REPLY: 
					
					if (isEvent) 
					{
						HandleEventReplyVEvent();
					}

					if (isFreeBusy)
					{
						HandlePublishVFreeBusy(FALSE);
					}

					break;

			case NSCalendar::METHOD_CANCEL: 
				
					if (isEvent) 
					{
						HandleEventCancelVEvent();
					}
					
					break;

			case NSCalendar::METHOD_REFRESH:
				
					if (isEvent) 
					{
						HandleEventRefreshRequestVEvent();
					}

					if (isFreeBusy)
					{
						doAddHTML ( ny );
					}
					break;

			case NSCalendar::METHOD_COUNTER: 
						
					if (isEvent) 
					{
						HandleEventCounterPropVEvent();
					}
					break;

			case NSCalendar::METHOD_DECLINECOUNTER: 

					if (isEvent) 
					{
						HandleDeclineCounterVEvent();
					}
					break;

			case NSCalendar::METHOD_INVALID: 
			default:
					doAddHTML ( im );
					break;	
		}

		doAddHTML( nbsp );

	}

	if (icalMaxIndex > 1 && !detail)
	{
		doHeaderMuiltEnd();
		doAddGroupButton(doCreateButton(&Buttons_Accept_Type, &Buttons_Accept_Label));
		doAddGroupButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
	}


	doAddHTML( End_HTML );

	return TRUE;
}

void
JulianFormFactory::doHeaderMuiltStart()
{
	char* m1 = new char[sizeof(MuiltEvent.toCString("")) + 50];

	if (m1)
	{
	UnicodeString	tempstring;

		doHeader(MuiltEvent_Header_HTML);
	
		doAddHTML( nbsp );

		// Get This messages contains 2 events
		sprintf (m1 , MuiltEvent.toCString(""), maxical + 1);

		doAddHTML( m1 );
		doAddHTML( Line_3_HTML );

		doAddHTML( Props_Head_HTML );

			doAddHTML( FBT_NewRow_HTML );
				doAddHTML( Cell_Start_HTML );
					doAddHTML(*doFont(*doItalic(*doBold(WhenStr , &tempstring ) , &tempstring ) , &tempstring) );
				doAddHTML( Cell_End_HTML );

				doAddHTML( Cell_Start_HTML );
					doFont(*doItalic(*doBold(WhatStr , &tempstring ) , &tempstring ) , &tempstring );
					doAddHTML( nbsp );
					doAddHTML(tempstring);
				doAddHTML( Cell_End_HTML );
			doAddHTML( FBT_EndRow_HTML );

		delete m1;
	}
}

void
JulianFormFactory::doHeaderMuiltEnd()
{
	doAddHTML( FBT_End_HTML );
}


/*
** Muilable icalcomponents. Header for non-detail version.
*/
void
JulianFormFactory::doHeaderMuilt()
{
	UnicodeString tempstring;
	UnicodeString dtstart_string("%B");
	UnicodeString summary_string("%S");
    UnicodeString uTemp;

	doAddHTML( FBT_NewRow_HTML );
		doAddHTML( Cell_Start_HTML );

		// doAddHTML( WhenStr );
		doAddHTML( Font_Fixed_Start_HTML );
			doPreProcessing(dtstart_string);
		doAddHTML( Font_Fixed_End_HTML );

		doAddHTML( Cell_End_HTML );

		doAddHTML( Cell_Start_HTML );
		// doAddHTML( What );
		doAddHTML( Start_BIG_Font );
//			doAddHTML( Font_Fixed_Start_HTML );
        
        uTemp = thisICalComp->toStringFmt(summary_string);
        doAddHTML(uTemp);
        //doAddHTML(thisICalComp->toStringFmt(summary_string));

//			doAddHTML( Font_Fixed_End_HTML );
		doAddHTML( End_Font );

		doAddHTML( Cell_End_HTML );

	doAddHTML( FBT_EndRow_HTML );
}

/*
** Cool "When:" formating.
*/
void
JulianFormFactory::doPreProcessing(UnicodeString icalpropstr)
{
t_int32 ICalPropChar = 	(t_int32)(icalpropstr[(TextOffset)1]);

	if (ICalPropChar == ms_cDTStart)
	{
		DateTime		start, end;
		XP_Bool allday = isEvent ? (XP_Bool)((TimeBasedEvent *)thisICalComp)->isAllDayEvent() : FALSE;

		if (isEvent)
		{
			start = ((VEvent *)thisICalComp)->getDTStart();
			end = ((VEvent *)thisICalComp)->getDTEnd();
		} else
		if (isFreeBusy)
		{
			start = ((VFreebusy *)thisICalComp)->getDTStart();
			end = ((VFreebusy *)thisICalComp)->getDTEnd();
		}

		doPreProcessingDateTime(icalpropstr, allday, start, end, *thisICalComp);
	} else
	if (ICalPropChar == ms_cAttendees)
	{
		doPreProcessingAttend(*thisICalComp);
	} else
	if (	ICalPropChar == ms_cOrganizer)
	{
		doPreProcessingOrganizer(*thisICalComp);
	}
}

void
JulianFormFactory::doPreProcessingAttend(ICalComponent& ic)
{
XP_Bool			lastOne = FALSE;	
UnicodeString	uni_comma	= ",";
UnicodeString	uni_AttendName	= "%(^N)v";
UnicodeString	attendStatusStr = UnicodeString("%(^S)v");
UnicodeString	attendStatus = ic.toStringFmt(attendStatusStr);
UnicodeString	attendNames = ic.toStringFmt(uni_AttendName);
UnicodeString	tempString;
UnicodeString	tempString2;
UnicodeString	tempString3;
UnicodeString	tempString4;
TextOffset		status_start = 0,
				status_end = 0,
				name_start = 0,
				name_end = 0;

	while (!lastOne)
	{
		status_end = attendStatus.indexOf(uni_comma, status_start);
		name_end   = attendNames.indexOf(uni_comma, name_start);

		// Hit the last name yet?
		if (status_end == -1)
		{
			lastOne = TRUE;
			status_end = attendStatus.size();
			name_end = attendNames.size();
		}

		// Add a comma after each name execpt for the last name.
		if (status_start > 0)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
		}

		attendStatus.extractBetween(status_start, status_end, tempString);
		if (tempString == JulianKeyword::Instance()->ms_sACCEPTED )
		{
			doAddHTML( Accepted_Gif_HTML );
		} else
		if (tempString == JulianKeyword::Instance()->ms_sDECLINED )
		{
			doAddHTML( Declined_Gif_HTML );
		} else
		if (tempString == JulianKeyword::Instance()->ms_sDELEGATED )
		{
			doAddHTML( Delegated_Gif_HTML );
		} else
		if (tempString == JulianKeyword::Instance()->ms_sVCALNEEDSACTION )
		{
			doAddHTML( NeedsAction_Gif_HTML );
		} else
			doAddHTML( Question_Gif_HTML );

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		attendNames.extractBetween(name_start, name_end, tempString);
		tempString.extractBetween(7, tempString.size(), tempString4);
		doAddHTML(
			doARef(
				*doFont( tempString4, &tempString2 ),
				tempString,
				&tempString3 )
			);

		status_start = status_end + 1;
		name_start   = name_end + 1;
		
		// Set it to an empty string
		tempString2.remove();
		tempString3.remove();
	}
}

void
JulianFormFactory::doPreProcessingOrganizer(ICalComponent& ic)
{
UnicodeString	uni_OrgName	= "%(^N)J";
UnicodeString	attendNames = ic.toStringFmt(uni_OrgName);
UnicodeString	tempString;
UnicodeString	tempString2;
UnicodeString	tempString3;
UnicodeString	tempString4;
TextOffset		name_start = 0,
				name_end = 0;

	name_end = attendNames.size();

	// Get the email address mailto:xxx@yyy.com
	// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
	attendNames.extractBetween(name_start, name_end, tempString);
	tempString.extractBetween(7, tempString.size(), tempString4);
	doAddHTML(
		doARef(
			*doFont( tempString4, &tempString2 ),
			tempString,
			&tempString3 )
		);
}

UnicodeString*
JulianFormFactory::doARef(UnicodeString& refText, UnicodeString& refTarget, UnicodeString* outputString)
{
	UnicodeString *Saved_S = s;
	s = outputString;

	doAddHTML( Aref_Start_HTML );
	doAddHTML( refTarget );
	doAddHTML( Aref_End_HTML );
	doAddHTML( refText );
	doAddHTML( ArefTag_End_HTML );

	s = Saved_S;

	return outputString;
}



UnicodeString*
JulianFormFactory::doFontFixed(UnicodeString& fontText, UnicodeString* outputString)
{
	UnicodeString *Saved_S = s;
	UnicodeString	out1;
	s = &out1;

	doAddHTML( Font_Fixed_Start_HTML );
	doAddHTML( fontText );
	doAddHTML( Font_Fixed_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

UnicodeString*
JulianFormFactory::doFont(UnicodeString& fontText, UnicodeString* outputString)
{
	UnicodeString *Saved_S = s;
	UnicodeString	out1;
	s = &out1;

	doAddHTML( Start_Font );
	doAddHTML( fontText );
	doAddHTML( End_Font );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

UnicodeString*
JulianFormFactory::doItalic(UnicodeString& ItalicText, UnicodeString* outputString)
{
	UnicodeString *Saved_S = s;
	UnicodeString	out1;
	s = &out1;

	doAddHTML( Italic_Start_HTML );
	doAddHTML( ItalicText );
	doAddHTML( Italic_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

UnicodeString*
JulianFormFactory::doBold(UnicodeString& BoldText, UnicodeString* outputString)
{
	UnicodeString *Saved_S = s;
	UnicodeString	out1;
	s = &out1;

	doAddHTML( Bold_Start_HTML );
	doAddHTML( BoldText );
	doAddHTML( Bold_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

void
JulianFormFactory::doPreProcessingDateTime(UnicodeString& icalpropstr, XP_Bool allday, DateTime &start, DateTime &end, ICalComponent &ic)
{
	UnicodeString dtstartI	= "%B";
	UnicodeString dtstart	= "%(EEE MMM dd, yyyy hh:mm a)B";
	UnicodeString dtend		= "%(EEE MMM dd, yyyy hh:mm a z)e";

	UnicodeString dtstartSD = "%(EEE MMM dd, yyyy hh:mm a)B - ";
	UnicodeString dtstartAD = "%(EEE MMM dd, yyyy)B - ";
	UnicodeString dtstartMD = "%(EEE MMM dd, yyyy hh:mm a z)B";
	UnicodeString dtendSD	= "%(hh:mm a z)e";
	UnicodeString dtdurSD	= " ( %D )";
	UnicodeString endstr	= ic.toStringFmt(dtend);
    UnicodeString uTemp;

	// All Day event. May spane more then the one day
	// Display as May 01, 1997 ( Day Event )
	// Note: fix this up for a single day event and for more then one day event
	if (allday)
	{
        uTemp = ic.toStringFmt(dtstartAD);
		doAddHTML(uTemp);
        //doAddHTML(	ic.toStringFmt(dtstartAD)	);

        doAddHTML(	Text_AllDay					);
	} else
	// Moment in time date, ie dtstart == dtend?
	// Display as Begins on May 01, 1997 12:00 pm
	if (start == end)
	{
		doAddHTML(	Text_StartOn				);

        uTemp = ic.toStringFmt(dtstartMD);
        doAddHTML(uTemp);
		//doAddHTML(	ic.toStringFmt(dtstartMD)	);
	
    } else
	// Both dates on the same day?
	// Display as May 01, 1997 12:00 pm - 3:00 pm ( 3 Hours )
	if (start.sameDMY(&end, TimeZone::createDefault()))
	{
        uTemp = ic.toStringFmt(dtstartSD);
        doAddHTML(uTemp);
		//doAddHTML(	ic.toStringFmt(dtstartSD)	);
		
        uTemp = ic.toStringFmt(dtendSD);
        doAddHTML(uTemp);
        //doAddHTML(	ic.toStringFmt(dtendSD)		);
		
        uTemp = ic.toStringFmt(dtdurSD);
        doAddHTML(uTemp);
        //doAddHTML(	ic.toStringFmt(dtdurSD)		);
	}
	
	// Default Case.
	// Display both dates in full. ie May 01, 1997 12:00 pm - May 02, 1997 2:00 pm
	else
	if (icalpropstr.indexOf(dtstartI) >= 0)
	{

        uTemp = ic.toStringFmt(dtstart);
        doAddHTML(uTemp);
		//doAddHTML( ic.toStringFmt(dtstart) );
		
        if (endstr.size() > 6)
		{
			doAddHTML( Text_To );
			doAddHTML( endstr );
		}
	}
}

void
JulianFormFactory::doPostProcessing(UnicodeString icalpropstr)
{
}

/*
**
** Assumed that serverICalComp is good.
*/
void
JulianFormFactory::doDifferenceProcessingAttendees()
{
XP_Bool			lastOne = FALSE,
				need_comma = FALSE;	
UnicodeString	uni_comma	= ",";
UnicodeString	uni_x	= "%(^N)v";
UnicodeString	NewAttendNames = thisICalComp->toStringFmt(uni_x);
UnicodeString	OldAttendNames = serverICalComp->toStringFmt(uni_x);
UnicodeString	tempString;
UnicodeString	tempString2;
UnicodeString	tempString3;
UnicodeString	tempString4;
TextOffset		new_name_start = 0,
				new_name_end = 0,
				old_name_start = 0,
				old_name_end = 0;

// Work and the "Added" section. These are email address that are in thisICalComp and
// not in serverICalComp. The general idea here is to march down the attends in NewAttendNames
// and see if they are in OldAttendNames

	doAddHTML( Props_HTML_Before_Label );
	doAddHTML( Start_Font );
		doAddHTML( Italic_Start_HTML );
			doAddHTML( " Added: " );
		doAddHTML( Italic_End_HTML );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Label );

	while (!lastOne)
	{
		new_name_end   = NewAttendNames.indexOf(uni_comma, new_name_start);

		// Hit the last name yet?
		if (new_name_end == -1)
		{
			lastOne = TRUE;
			new_name_end = NewAttendNames.size();
		}

		// Add a comma after each name execpt for the last name.
		if (need_comma)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
			need_comma = FALSE;
		}

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		NewAttendNames.extractBetween(new_name_start, new_name_end, tempString);	// Get "mailto:xxx@yyy.com"
		tempString.extractBetween(7, tempString.size(), tempString4);				// Remove "mailto:"

		// tempString4 now holds "xxx@yyy.com". See if it is in OldAttendNames
		if (OldAttendNames.indexOf(tempString4) == -1)
		{
			// Not there so this must be a new one. Format it for output
			doAddHTML( doARef( *doFont( tempString4, &tempString2 ), tempString, &tempString3 ) );
			need_comma = TRUE;
		}

		new_name_start   = new_name_end + 1;
		
		// Set it to an empty string
		tempString2.remove();
		tempString3.remove();
		tempString4.remove();
	}
	doAddHTML( Props_HTML_After_Data );	

// Work and the "Removed" section. These are email address that are in thisICalComp and
// not in serverICalComp. The general idea here is to march down the attends in NewAttendNames
// and see if they are in OldAttendNames

	doAddHTML( Props_HTML_Before_Label );
	doAddHTML( Start_Font );
		doAddHTML( Italic_Start_HTML );
			doAddHTML( " Removed: " );
		doAddHTML( Italic_End_HTML );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Label );

	need_comma = lastOne = FALSE;
	new_name_start = 0;
	while (!lastOne)
	{
		new_name_end   = OldAttendNames.indexOf(uni_comma, new_name_start);

		// Hit the last name yet?
		if (new_name_end == -1)
		{
			lastOne = TRUE;
			new_name_end = OldAttendNames.size();
		}

		// Add a comma after each name execpt for the last name.
		if (need_comma)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
			need_comma = FALSE;
		}

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		OldAttendNames.extractBetween(new_name_start, new_name_end, tempString);	// Get "mailto:xxx@yyy.com"
		tempString.extractBetween(7, tempString.size(), tempString4);				// Remove "mailto:"

		// tempString4 now holds "xxx@yyy.com". See if it is in OldAttendNames
		if (NewAttendNames.indexOf(tempString4) == -1)
		{
			// Not there so this must be a removed one. Format it for output
			doAddHTML( doARef( *doFont( tempString4, &tempString2 ), tempString, &tempString3 ) );
		}

		new_name_start   = new_name_end + 1;
		
		// Set it to an empty string
		tempString2.remove();
		tempString3.remove();
		tempString4.remove();
	}
	doAddHTML( Props_HTML_After_Data );	
}

/*
** Cool diff formating.
*/
void
JulianFormFactory::doDifferenceProcessing(UnicodeString icalpropstr)
{
	// First look to see if there is a server version of this.
	if (serverICalComp)
	{
		UnicodeString itipstr;
		UnicodeString serverstr;

		if (isPreProcessed(icalpropstr))
		{
			t_int32 ICalPropChar = 	(t_int32)(icalpropstr[(TextOffset)1]);

			// Attendees are different. Look at each one and see who has been add or deleted.
			// Right now do not show anything if just their role has changed.
			if (ICalPropChar == ms_cAttendees)
			{
				doDifferenceProcessingAttendees();
			} else
			// Normal case is just diff the string that would be printed.
			{
			UnicodeString *Saved_S = s;
			ICalComponent*	IC_Saved = thisICalComp;

				s = &itipstr;
				// Set itipstr to the formated version
				doPreProcessing(icalpropstr);

				s = &serverstr;
				thisICalComp = serverICalComp;
				// Set serverstr to the formated version
				doPreProcessing(icalpropstr);
				thisICalComp = IC_Saved;

				s = Saved_S;
			}
		} else {
			itipstr	  = thisICalComp->toStringFmt(icalpropstr);
			serverstr = serverICalComp->toStringFmt(icalpropstr);
		}

		if (itipstr != serverstr)
		{
			doAddHTML( Props_HTML_Before_Label );
			doAddHTML( Start_Font );
				doAddHTML( Italic_Start_HTML );
					doAddHTML( Text_Was );
				doAddHTML( Italic_End_HTML );
			doAddHTML( End_Font );

			doAddHTML( Props_HTML_After_Label );

			doAddHTML( Start_Font );
				doAddHTML( Italic_Start_HTML );
					doAddHTML( serverstr );
				doAddHTML( Italic_End_HTML );
			doAddHTML( End_Font );
			doAddHTML( Props_HTML_After_Data );	
		}
	}
}

void
JulianFormFactory::doProps(int32 labelCount, UnicodeString labels[], int32 dataCount, UnicodeString data[])
{
    UnicodeString uTemp;
	doAddHTML( Props_Head_HTML );
	for (int32 x = 0; x < dataCount; x ++)
	{
		doAddHTML( Props_HTML_Before_Label );
		doAddHTML( Start_Font );
		if (x < labelCount)
			doAddHTML( labels[x] );
		else
			doAddHTML( Props_HTML_Empty_Label );
		doAddHTML( End_Font );
		doAddHTML( Props_HTML_After_Label );
		doAddHTML( Start_Font );
        uTemp = thisICalComp->toStringFmt(data[x]);
		isPreProcessed(data[x]) ? doPreProcessing(data[x]) : doAddHTML(uTemp);
		//isPreProcessed(data[x]) ? doPreProcessing(data[x]) : doAddHTML( thisICalComp->toStringFmt(data[x]) );
		doAddHTML( End_Font );
		doAddHTML( Props_HTML_After_Data );	
		doDifferenceProcessing(data[x]);
	}
}

void
JulianFormFactory::doHeader(UnicodeString HeaderText)
{
	doAddHTML( General_Header_Start_HTML );
	doAddHTML( Start_Font );
	doAddHTML( HeaderText );
	doAddHTML( End_Font );

	if (detail)
	{
		doAddHTML( General_Header_Status_HTML );
		doAddHTML( Start_Font );
		// Only can handle new or not new
		doAddHTML( serverICalComp ? "(Update)" : "(New)" );
		doAddHTML( End_Font );
	}

	doAddHTML( General_Header_End_HTML );
}

void
JulianFormFactory::doClose()
{
	if (isDetail())
	{ 
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(&Buttons_Close_Type, &Buttons_Close_Label));
	}
}

void
JulianFormFactory::doStatus()
{
	// Start with status items that only can happen in detail mode.
	// Only suport vevents.
	if (isDetail() && isEvent)
	{
		// Add event allready on Calendar Server or not.
		doSingleTableLine(&EventNote, serverICalComp ? &EventInSchedule : &EventNotInSchedule);

		// See if it's not on the server, see if it overlaps any other event
		// doSingleTableLine(&EventConflict, &EventTest, FALSE);
	}
}

void
JulianFormFactory::doSingleTableLine(UnicodeString* labelString, UnicodeString* dataString, XP_Bool addSpacer)
{
	if (addSpacer) doAddHTML( Head2_HTML );
	doAddHTML( Props_HTML_Before_Label );
	if (labelString->size() == 0) // Is empty?
	{
		doAddHTML( Props_HTML_Empty_Label );
	} else {
		doAddHTML( Start_Font );
		doAddHTML( *labelString );
		doAddHTML( End_Font );
	}
	doAddHTML( Props_HTML_After_Label );
	doAddHTML( Start_Font );
	doAddHTML( *dataString );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Data );	
}

void
JulianFormFactory::doCommentText()
{
	// Comments Field
	doAddHTML( Head2_HTML );
	doAddHTML( Text_Label_Start_HTML );
	doAddHTML( Start_Font );
	doAddHTML( Text_Label );
	doAddHTML( End_Font );
	doAddHTML( Text_Label_End_HTML );
	doAddHTML( Text_Field_HTML );
}

//
// To add a group button on it's own line
void
JulianFormFactory::doAddGroupButton(UnicodeString GroupButton_HTML)
{
	doAddHTML( Start_Font );
	doAddHTML( GroupButton_HTML );
	doAddHTML( End_Font );
}

//
// To add a single button on it's own line
void
JulianFormFactory::doAddButton(UnicodeString SingleButton_HTML)
{
	doAddHTML( Buttons_GroupSingleStart_HTML );
	doAddGroupButton(SingleButton_HTML);
	doAddHTML( Buttons_GroupSingleEnd_HTML );
}

//
// To add a single button on it's own line
UnicodeString
JulianFormFactory::doCreateButton(UnicodeString *InputType, UnicodeString *ButtonName, XP_Bool addTextField)
{
	UnicodeString t = "";
/*** LOOK HERE *****/
//	char*	url_string = cb ? cb->callbackurl( (void *)nil, InputType->toCString("")) : "";
	char*	url_string =  "";
	char*	temp = new char[Buttons_Single_Start_HTML.size() + strlen(url_string)];

	if (temp)
	{
		strcpy(temp, Buttons_Single_Start_HTML.toCString(""));
		sprintf(temp, Buttons_Single_Start_HTML.toCString(""), url_string);
	}
	t += Start_Font;
	if (temp) t += temp;
	t += *InputType;
	t += Buttons_Single_Mid_HTML;
	t += *ButtonName;
	t += addTextField ? Buttons_Text_End_HTML : Buttons_Single_End_HTML;
	t += End_Font;

	return t;
}

//
// Make a FreeBusy Table
void
JulianFormFactory::doMakeFreeBusyTable()
{
UnicodeString temps;

	if (isFreeBusy)
	{
        UnicodeString   uTemp;
		UnicodeString	tz;
		UnicodeString	timezoneformat = "z";
		TimeZone*		default_tz = TimeZone::createDefault();
		DateTime		start;
		DateTime		end ;
		// set start and end (to remove warnings John Sun 5-1-98)
        start = ((VFreebusy *)thisICalComp)->getDTStart();
		end = ((VFreebusy *)thisICalComp)->getDTEnd();
        Duration		MinutesPerSlot = Duration(FBT_TickSetting);
		int32			SlotsPerHour = (60 / (MinutesPerSlot.getMinute() ? MinutesPerSlot.getMinute() : 30));
		char			TimeHour_HTML[200];

		sprintf(TimeHour_HTML, FBT_TimeHour_HTML.toCString(""), SlotsPerHour);

        

		// This is the Hour row ie 6am to 7 pm etc
		int32 start_hour = DateTime(FBT_HourStart).getHour(default_tz);
		int32 end_hour = DateTime(FBT_HourEnd).getHour(default_tz);

		if (DateTime(FBT_HourEnd).getMinute() != 0) end_hour++; // Only show full hours. This is to get 11pm to work
		tz = start.strftime(timezoneformat, default_tz);

		doAddHTML(FBT_Start_HTML);
		doAddHTML(FBT_NewRow_HTML);
		// This is the TimeZone Heading
		doAddHTML( Start_Font );
		doAddHTML(FBT_TimeHead_HTML);
		doAddHTML(tz);
		doAddHTML(FBT_TimeHeadEnd_HTML);
		doAddHTML( End_Font );
		doAddHTML( nbsp );

		// Error check here?
        int32 x;
		for (x = start_hour; x < end_hour; x++)
		{
		char ss[10];
		int32 y = (x == 0) ? 12 : (x > 12) ? (x - 12) : (x); // Translate midnight to 12, convert 24h to 12h 

			sprintf(ss, "%d", y);
			doAddHTML(TimeHour_HTML);
			doAddHTML(Start_Font);
			doAddHTML(FBT_TD_HourColor_HTML);
			doAddHTML(ss);
			switch (x)
			{
				case 0:		doAddHTML(FBT_AM); break;
				case 12:	doAddHTML(FBT_PM); break;
				default: 
					if ( x == start_hour) 
                    { 
                        temps = " a"; 
                        
                        uTemp = start.strftime(temps);
                        doAddHTML(uTemp);
                        //doAddHTML(start.strftime(temps));
                    };
			}
			doAddHTML(FBT_TD_HourColorEnd_HTML);
			doAddHTML(End_Font);
			doAddHTML(FBT_TimeHourEnd_HTML);
		}
		doAddHTML(FBT_TDOffsetCell_HTML);
		doAddHTML(FBT_EndRow_HTML);

		// Minutes 00 or 30
		doAddHTML(FBT_NewRow_HTML);
		UnicodeString tickarray[] = {FBT_TickMark1, FBT_TickMark2, FBT_TickMark3, FBT_TickMark4};
		int32 yoffset = (SlotsPerHour == 4) ? 1 : 2;
		for (x = start_hour; x < end_hour; x ++)
		{
			for (int32 y = 0; y < SlotsPerHour; y ++)
			{
				doAddHTML(FBT_TimeMin_HTML);
				doAddHTML(Start_Font);
				doAddHTML(FBT_TD_MinuteColor_HTML);
				doAddHTML(tickarray[y * yoffset]);
				doAddHTML(FBT_TD_MinuteColorEnd_HTML);
				doAddHTML(End_Font);
				doAddHTML(FBT_TimeMinEnd_HTML);
			}
		}
		doAddHTML(FBT_TDOffsetCell_HTML);
		doAddHTML(FBT_EndRow_HTML);

		// Tick marks
		doAddHTML(FBT_NewRow_HTML);
		doAddHTML(FBT_TDOffsetCell_HTML);
		for (x = start_hour; x < end_hour; x ++)
		{
			for (int y = 0; y < SlotsPerHour; y ++)
			{
				doAddHTML(FBT_TimeMin_HTML);
				doAddHTML(Start_Font);
				doAddHTML(y & 1 ? FBT_TickShort_HTML : FBT_TickLong_HTML);
				doAddHTML(End_Font);
				doAddHTML(FBT_TimeMinEnd_HTML);
			}
		}
		doAddHTML(FBT_TDOffsetCell_HTML);

		doAddHTML(FBT_EndRow_HTML);

		// Each day now
		for (DateTime currentday = start; currentday < end; currentday.nextDay())
		{
		Duration d_offset = Duration(FBT_TickOffset);
		DateTime dt = DateTime(currentday.getYear(), currentday.getMonth(), currentday.getDate(), start_hour, 0);

			doAddHTML(FBT_NewRow_HTML);
			doAddHTML(FBT_DayStart_HTML);

			doAddHTML(Start_Font);

            uTemp = currentday.strftime(FBT_DayFormat);
			doAddHTML(uTemp);
            //doAddHTML(currentday.strftime(FBT_DayFormat));
			
            doAddHTML(End_Font);

			doAddHTML(FBT_DayEmptyCell_HTML);

			JulianPtrArray* fbv = isFreeBusy ? ((VFreebusy *)thisICalComp)->getFreebusy() : nil;
			JulianPtrArray*	pv = nil;
			Freebusy *fb = nil;
			Period	*p  = nil;
			int32 fbIndex = 0;
			int32 pIndex = 0;
			DateTime StartOfSlot, EndOfSlot;

			if (fbv) fb = (Freebusy *)fbv->GetAt(fbIndex++);
			if (fb) pv = fb->getPeriod(); pIndex = 0;
			if (pv) p = (Period *)pv->GetAt(pIndex++);

			StartOfSlot = DateTime(currentday.getYear(), currentday.getMonth(), currentday.getDate(), dt.getHour(), dt.getMinute());
			EndOfSlot = DateTime(StartOfSlot);
			StartOfSlot.add(d_offset);
			EndOfSlot.add(MinutesPerSlot);
			EndOfSlot.subtract(d_offset);

			// Loop for the number of timeslots, each slot is 30 minutes. Default to 30 minutes if ther is a problem
			int32 slot_count = (end_hour - start_hour) * SlotsPerHour;
			for (x = 0; x < slot_count;  x++,StartOfSlot.add(MinutesPerSlot), EndOfSlot.add(MinutesPerSlot))
			{				
				// Do either free, busy, or not in this time slot
				DateTime PeriodEnd;

				if (p && p->isIntersecting(StartOfSlot, EndOfSlot))
				{
					if (fb->getType() == Freebusy::FB_TYPE_BUSY ||
                                            fb->getType() == Freebusy::FB_TYPE_BUSY_UNAVAILABLE ||
                                            fb->getType() == Freebusy::FB_TYPE_BUSY_TENTATIVE)
					{
						doAddHTML(FBT_DayBusyCell_HTML);
					}
                                        else
					if (fb->getType() == Freebusy::FB_TYPE_FREE)
					{
						doAddHTML(FBT_DayFreeCell_HTML);
					} else
					{
						doAddHTML(FBT_DayFreeCell_HTML); // Assume all non-busy time is free
					}
				} else
				{
					// doAddHTML(FBT_DayEmptyCell_HTML);
					doAddHTML(FBT_DayFreeCell_HTML); // Assume all non-busy time is free
				}

				dt.add(MinutesPerSlot);

				if (p) p->getEndingTime(PeriodEnd);
				if (p && (dt.compareTo(PeriodEnd) >= 0))
				{
					// need a new period
					if (pIndex >= pv->GetSize())	// Have more periods in this freebusy?
					{
						// No more periods in this freebusy, try the next freebusy
						if (fbIndex >= fbv->GetSize())
						{
							p = (Period *)nil;	// Ran out of periods and freebusys all togeather
							pv = (JulianPtrArray *)nil;
						} else
						{
							fb = (Freebusy *)fbv->GetAt(fbIndex++);
							pv = fb->getPeriod(); pIndex = 0;
						}
					}
					if (pv) p = (Period *)pv->GetAt(pIndex++);
				}
			}

			doAddHTML(FBT_DayEnd_HTML);
			doAddHTML(FBT_TimeMin_HTML);
			doAddHTML(FBT_DayEmptyCell_HTML);
			doAddHTML(FBT_TimeMinEnd_HTML);
			doAddHTML(FBT_EndRow_HTML);

			// Empty Row
			doAddHTML(FBT_NewRow_HTML);
			doAddHTML(FBT_DayEmptyCell_HTML); // No Title
			doAddHTML(FBT_DayEmptyCell_HTML); // offset to first timeslot

			// All of the time slots
			for (x = start_hour; x < end_hour; x++)
			{
				for (int32 y = 0; y < SlotsPerHour; y++) doAddHTML(FBT_DayEmptyCell_HTML);
			}
			doAddHTML(FBT_EndRow_HTML);
		}


		doAddHTML(FBT_End_HTML);

		// Legend
		doAddHTML(FBT_Legend_Start_HTML);
		doAddHTML(FBT_Legend_Text1_HTML);
		doAddHTML(Start_Font);
		doAddHTML(FBT_Legend_Title);
		doAddHTML(End_Font);
		doAddHTML(FBT_Legend_TextEnd_HTML);
		doAddHTML(FBT_Legend_Text2_HTML);
		doAddHTML(Start_Font);
		doAddHTML(FBT_Legend_Free);
		doAddHTML(End_Font);
		doAddHTML(FBT_Legend_TextEnd_HTML);
		doAddHTML(FBT_Legend_Text3_HTML);
		doAddHTML(Start_Font);
		doAddHTML(FBT_Legend_Busy);
		doAddHTML(End_Font);
		doAddHTML(FBT_Legend_TextEnd_HTML);
		doAddHTML(FBT_Legend_End_HTML);
	}
}

void
JulianFormFactory::HandlePublishVEvent()
{
	int32	data_length = isDetail() ? publish_D_Fields_Data_HTML_Length : publish_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? publish_D_Fields_Labels_Length : publish_Fields_Labels_Length;

	doHeader(publish_Header_HTML);
	doProps(label_length, isDetail() ? publish_D_Fields_Labels : publish_Fields_Labels, data_length, isDetail() ? publish_D_Fields_Data_HTML : publish_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( publish_End_HTML );
}

void
JulianFormFactory::HandlePublishVFreeBusy(XP_Bool isPublish)
{
	int32	data_length = isDetail() ? publishFB_D_Fields_Data_HTML_Length : publishFB_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? publishFB_D_Fields_Labels_Length : publishFB_Fields_Labels_Length;

	doHeader(isPublish ? publishFB_Header_HTML : replyFB_Header_HTML);
	doProps(label_length, isDetail() ? publishFB_D_Fields_Labels : publishFB_Fields_Labels, data_length, isDetail() ? publishFB_D_Fields_Data_HTML : publishFB_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		// doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
		doAddHTML( Head2_HTML );
		// doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
	}
	doAddHTML( Props_End_HTML );

	doMakeFreeBusyTable();

	doAddHTML( publishFB_End_HTML );
}

void
JulianFormFactory::HandleRequestVEvent()
{
	int32	data_length = isDetail() ? request_D_Fields_Data_HTML_Length : request_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_Fields_Labels_Length : request_Fields_Labels_Length;

	doHeader(request_Header_HTML);
	doProps(label_length, isDetail() ? request_D_Fields_Labels : request_Fields_Labels, data_length, isDetail() ? request_D_Fields_Data_HTML : request_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
	doStatus();
	doCommentText();
	if (isDetail()) doAddButton(Buttons_SaveDel_HTML);

	doAddHTML( Buttons_GroupStart_HTML );
	doAddGroupButton(doCreateButton(&Buttons_Accept_Type, &Buttons_Accept_Label));
	doAddGroupButton(doCreateButton(&Buttons_Decline_Type, &Buttons_Decline_Label));
	if (isDetail())
	{
		// Add Save copy here
		doAddGroupButton(doCreateButton(&Buttons_Tentative_Type, &Buttons_Tentative_Label));
		doAddGroupButton(doCreateButton(&Buttons_DelTo_Type, &Buttons_DelTo_Label, TRUE));
	} else {
		doAddGroupButton(doCreateButton(&Buttons_DelTo_Type, &Buttons_DelTo_Label, TRUE));
	}
	doAddHTML( Buttons_GroupEnd_HTML );

	doClose();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleRequestVFreeBusy( )
{
	int32	data_length = isDetail() ? request_D_FB_Fields_Data_HTML_Length : request_FB_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_FB_Fields_Labels_Length : request_FB_Fields_Labels_Length;

	doHeader(request_FB_Header_HTML);
	doProps(label_length, isDetail() ? request_D_FB_Fields_Labels : request_FB_Fields_Labels, data_length, isDetail() ? request_D_FB_Fields_Data_HTML : request_FB_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
	doStatus();

	doAddHTML( Head2_HTML );
	if (isDetail()) { doAddHTML( Props_End_HTML ); doMakeFreeBusyTable(); doAddHTML( Props_Head_HTML ); doAddHTML( Head2_HTML ); }

	doAddButton(doCreateButton(&Buttons_SendFB_Type, &Buttons_SendFB_Label));
	doClose();
	doAddHTML( Props_End_HTML );

	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleEventReplyVEvent()
{
	int32	data_length = isDetail() ? eventreply_D_Fields_Data_HTML_Length : eventreply_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventreply_D_Fields_Labels_Length : eventreply_Fields_Labels_Length;

	doHeader(eventreply_Header_HTML);
	doProps(label_length, isDetail() ? eventreply_D_Fields_Labels : eventreply_Fields_Labels, data_length, isDetail() ? eventreply_D_Fields_Data_HTML : eventreply_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Update_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Update_Label));
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventreply_End_HTML );
}

void
JulianFormFactory::HandleEventCancelVEvent()
{
	int32	data_length = isDetail() ? eventcancel_D_Fields_Data_HTML_Length : eventcancel_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventcancel_D_Fields_Labels_Length : eventcancel_Fields_Labels_Length;

	doHeader(eventcancel_Header_HTML);
	doProps(label_length, isDetail() ? eventcancel_D_Fields_Labels : eventcancel_Fields_Labels, data_length, isDetail() ? eventcancel_D_Fields_Data_HTML : eventcancel_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Update_Label));
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventcancel_End_HTML );
}

void
JulianFormFactory::HandleEventRefreshRequestVEvent()
{
	int32	data_length = isDetail() ? eventrefreshreg_D_Fields_Data_HTML_Length : eventrefreshreg_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventrefreshreg_D_Fields_Labels_Length : eventrefreshreg_Fields_Labels_Length;

	doHeader(eventrefreshreg_Header_HTML);
	doProps(label_length, isDetail() ? eventrefreshreg_D_Fields_Labels : eventrefreshreg_Fields_Labels, data_length, isDetail() ? eventrefreshreg_D_Fields_Data_HTML : eventrefreshreg_Fields_Data_HTML);
	doStatus();
	doCommentText();

	if (isDetail())
	{
		doAddButton(doCreateButton(&Buttons_Add_Type, &Buttons_Add_Label));
		doAddButton(doCreateButton(&Buttons_SendRefresh_Type, &Buttons_SendRefresh_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(&Buttons_SendRefresh_Type, &Buttons_SendRefresh_Label));
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventrefreshreg_End_HTML );
}

void
JulianFormFactory::HandleEventCounterPropVEvent()
{
	int32	data_length = isDetail() ? request_D_Fields_Data_HTML_Length : request_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_Fields_Labels_Length : request_Fields_Labels_Length;

	doHeader(eventcounterprop_Header_HTML);
	doProps(label_length, isDetail() ? request_D_Fields_Labels : request_Fields_Labels, data_length, isDetail() ? request_D_Fields_Data_HTML : request_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(&Buttons_Details_Type, &Buttons_Details_Label));
	doStatus();
	doCommentText();
	if (isDetail()) doAddButton(Buttons_SaveDel_HTML);

	doAddHTML( Buttons_GroupStart_HTML );
	doAddGroupButton(doCreateButton(&Buttons_Accept_Type, &Buttons_Accept_Label));
	doAddGroupButton(doCreateButton(&Buttons_Decline_Type, &Buttons_Decline_Label));
	doAddHTML( Buttons_GroupEnd_HTML );

	doClose();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleDeclineCounterVEvent()
{
	int32	data_length = eventdelinecounter_Fields_Data_HTML_Length;
	int32	label_length = eventdelinecounter_Fields_Labels_Length;

	doHeader(eventdelinecounter_Header_HTML);
	doProps(label_length, eventdelinecounter_Fields_Labels, data_length, eventdelinecounter_Fields_Data_HTML);
	doStatus();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

/*
** General HTML
*/
UnicodeString JulianFormFactory::Start_HTML					= "<FORM ACTION=%s METHOD=GET><P><TABLE BORDER=1 CELLPADDING=10 CELLSPACING=0><TR><TD>\n";
UnicodeString JulianFormFactory::Props_Head_HTML			= "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 colspacing=5 colpadding=5>\n";
UnicodeString JulianFormFactory::Props_HTML_Before_Label	= "<TR><TD WIDTH=110 ALIGN=RIGHT VALIGN=TOP><P ALIGN=RIGHT><B>";
UnicodeString JulianFormFactory::Props_HTML_After_Label		= "</B>&nbsp;&nbsp;</TD><TD>";
UnicodeString JulianFormFactory::Props_HTML_After_Data		= "&nbsp;</TD></TR>\n";
UnicodeString JulianFormFactory::Props_HTML_Empty_Label		= "<TR><TD WIDTH=110>&nbsp;</TD><TD>&nbsp;</TD></TR><TR><TD WIDTH=110><DIV ALIGN=right>&nbsp;</DIV></TD>";
UnicodeString JulianFormFactory::Props_End_HTML				= "</TABLE>";
UnicodeString JulianFormFactory::End_HTML					= "&nbsp;</TD></TR></TABLE>&nbsp;</FORM>\n";
UnicodeString JulianFormFactory::General_Header_Start_HTML	= "<TABLE WIDTH=100% BORDER=0 CELLPADDING=0 CELLSPACING=0 BGCOLOR=#006666><TR><TD>&nbsp;<FONT COLOR=#FFFFFF><B>";
UnicodeString JulianFormFactory::General_Header_Status_HTML	= "</B></FONT></TD><TD><P ALIGN=RIGHT><FONT COLOR=#FFFFFF><B>";
UnicodeString JulianFormFactory::General_Header_End_HTML	= "</B></FONT></TD></TR></TABLE>\n";
UnicodeString JulianFormFactory::Head2_HTML					= "<TR><TD COLSPAN=\"2\"><HR></TD></TR>\n";
UnicodeString JulianFormFactory::Italic_Start_HTML			= "<I>";
UnicodeString JulianFormFactory::Italic_End_HTML			= "</I>";
UnicodeString JulianFormFactory::Bold_Start_HTML			= "<B>";
UnicodeString JulianFormFactory::Bold_End_HTML				= "</B>";
UnicodeString JulianFormFactory::Aref_Start_HTML			= "<A HREF=\"";
UnicodeString JulianFormFactory::Aref_End_HTML				= "\">";
UnicodeString JulianFormFactory::ArefTag_End_HTML			= "</A>";
UnicodeString JulianFormFactory::nbsp						= "&nbsp;";
UnicodeString JulianFormFactory::Accepted_Gif_HTML			= "+ ";
UnicodeString JulianFormFactory::Declined_Gif_HTML			= "- ";
UnicodeString JulianFormFactory::Delegated_Gif_HTML			= "> ";
UnicodeString JulianFormFactory::NeedsAction_Gif_HTML		= "* ";
UnicodeString JulianFormFactory::Question_Gif_HTML			= "? ";
UnicodeString JulianFormFactory::Line_3_HTML				= "<HR ALIGN=CENTER SIZE=3>";
UnicodeString JulianFormFactory::Cell_Start_HTML			= "<TD>";
UnicodeString JulianFormFactory::Cell_End_HTML				= "</TD>";
UnicodeString JulianFormFactory::Font_Fixed_Start_HTML		= "<TT><FONT SIZE=2>";
UnicodeString JulianFormFactory::Font_Fixed_End_HTML		= "</FONT></TT>";

/*
** Free/Busy Table
*/
UnicodeString JulianFormFactory::FBT_Start_HTML				= "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=2 BGCOLOR=#FFFFFF>\n";
UnicodeString JulianFormFactory::FBT_End_HTML				= "</TABLE>\n";
UnicodeString JulianFormFactory::FBT_NewRow_HTML			= "<TR>\n";
UnicodeString JulianFormFactory::FBT_EndRow_HTML			= "</TR>\n";
UnicodeString JulianFormFactory::FBT_TimeHead_HTML			= "<TH ALIGN=RIGHT VALIGN=TOP ROWSPAN=2 BGCOLOR=#666666>\n";
UnicodeString JulianFormFactory::FBT_TimeHeadEnd_HTML		= "</TH>";
UnicodeString JulianFormFactory::FBT_TimeHour_HTML			= "<TD COLSPAN=%d BGCOLOR=#666666>";
UnicodeString JulianFormFactory::FBT_TimeHourEnd_HTML		= "</TD>\n";
UnicodeString JulianFormFactory::FBT_TimeMin_HTML			= "<TD WIDTH=15 BGCOLOR=#666666>";
UnicodeString JulianFormFactory::FBT_TimeMinEnd_HTML		= "</TD>\n";
UnicodeString JulianFormFactory::FBT_TD_HourColor_HTML		= "<TT><FONT COLOR=#FFFFFF><FONT SIZE=-1>";
UnicodeString JulianFormFactory::FBT_TD_HourColorEnd_HTML	= "</TT></FONT></FONT>";
UnicodeString JulianFormFactory::FBT_TD_MinuteColor_HTML	= "<TT><FONT COLOR=#C0C0C0><FONT SIZE=-2>";
UnicodeString JulianFormFactory::FBT_TD_MinuteColorEnd_HTML	= "</TT></FONT></FONT>";
UnicodeString JulianFormFactory::FBT_TDOffsetCell_HTML		= "<TD HEIGHT=5 BGCOLOR=#666666>&nbsp;</TD>";
UnicodeString JulianFormFactory::FBT_TickLong_HTML			= "<TD VALIGN=TOP WIDTH=15 HEIGHT=10 BGCOLOR=#666666><IMG SRC=\"tickmark.GIF\" BORDER=0 HEIGHT=10 WIDTH=1 ALIGN=BOTTOM></TD>";
UnicodeString JulianFormFactory::FBT_TickShort_HTML			= "<TD VALIGN=TOP WIDTH=15 HEIGHT=10 BGCOLOR=#666666><IMG SRC=\"tickmark.GIF\" BORDER=0 HEIGHT=5 WIDTH=1 ALIGN=BOTTOM></TD>";
UnicodeString JulianFormFactory::FBT_HourStart				= "19971101T000000";
UnicodeString JulianFormFactory::FBT_HourEnd				= "19971101T235959";

UnicodeString JulianFormFactory::FBT_DayStart_HTML			= "<TD NOWRAP ALIGN=LEFT BGCOLOR=#666666><TT><FONT COLOR=#FFFFFF><FONT SIZE=-1>";
UnicodeString JulianFormFactory::FBT_DayEnd_HTML			= "</FONT></FONT></TT></TD>";
UnicodeString JulianFormFactory::FBT_DayEmptyCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#666666><CENTER>&nbsp;</CENTER></TD>";
UnicodeString JulianFormFactory::FBT_DayFreeCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#C0C0C0><CENTER>&nbsp;</CENTER></TD>";
UnicodeString JulianFormFactory::FBT_DayBusyCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#FFFFFF><CENTER>&nbsp;</CENTER></TD>";
UnicodeString JulianFormFactory::FBT_EmptyRow_HTML			= "<TR><TD COLSPAN=31 BGCOLOR=#666666>&nbsp;</TD></TR>";

// Legend
UnicodeString JulianFormFactory::FBT_Legend_Start_HTML		= "<TABLE BORDER=1 WIDTH=100><TR><TD WIDTH=50%>";
UnicodeString JulianFormFactory::FBT_Legend_Text1_HTML		= "<CENTER>";
UnicodeString JulianFormFactory::FBT_Legend_Text2_HTML		= "<TD WIDTH=30% BGCOLOR=#C0C0C0><CENTER>";
UnicodeString JulianFormFactory::FBT_Legend_Text3_HTML		= "<TD WIDTH=30% BGCOLOR=#FFFFFF><CENTER>";
UnicodeString JulianFormFactory::FBT_Legend_TextEnd_HTML	= "&nbsp;</CENTER></TD>";
UnicodeString JulianFormFactory::FBT_Legend_End_HTML		= "</TR></TABLE>";

UnicodeString JulianFormFactory::FBT_Legend_Title			= "Legend:";
UnicodeString JulianFormFactory::FBT_Legend_Free			= "free";
UnicodeString JulianFormFactory::FBT_Legend_Busy			= "busy";

UnicodeString JulianFormFactory::FBT_AM						= " AM";
UnicodeString JulianFormFactory::FBT_PM						= " PM";
UnicodeString JulianFormFactory::FBT_TickMark1				= "00";
UnicodeString JulianFormFactory::FBT_TickMark2				= "15";
UnicodeString JulianFormFactory::FBT_TickMark3				= "30";
UnicodeString JulianFormFactory::FBT_TickMark4				= "45";
UnicodeString JulianFormFactory::FBT_TickSetting			= "PT00H30M";
UnicodeString JulianFormFactory::FBT_TickOffset				= "PT00H01M";
UnicodeString JulianFormFactory::FBT_DayFormat				= "EEE, MMM dd";

/* 
** Fonts
*/
// UnicodeString JulianFormFactory::Start_Font					= "<FONT FACE=\"PrimaSans BT, Helvetica, Arial\"><FONT SIZE=-1>";
// UnicodeString JulianFormFactory::End_Font					= "</FONT></FONT>";
// UnicodeString JulianFormFactory::Start_BIG_Font				= "<FONT FACE=\"PrimaSans BT, Helvetica, Arial\"><FONT SIZE=2>";

UnicodeString JulianFormFactory::Start_Font					= "<FONT SIZE=-1>";
UnicodeString JulianFormFactory::End_Font					= "</FONT>";
UnicodeString JulianFormFactory::Start_BIG_Font				= "<FONT SIZE=2>";

/*
** Buttons
*/
UnicodeString JulianFormFactory::Buttons_Single_Start_HTML	= "<INPUT TYPE=SUBMIT NAME=\"";
UnicodeString JulianFormFactory::Buttons_Single_Mid_HTML	= "\" VALUE=\"";
UnicodeString JulianFormFactory::Buttons_Single_End_HTML	= "\">";
UnicodeString JulianFormFactory::Buttons_Text_End_HTML		= "\">&nbsp;<INPUT TYPE=TEXT SIZE=29 MAXLENGTH=100>";

UnicodeString JulianFormFactory::Buttons_Details_Type		= "btnNotifySubmitMD";
UnicodeString JulianFormFactory::Buttons_Details_Label		= "More Details...";
UnicodeString JulianFormFactory::Buttons_Add_Type			= "btnNotifySubmitATS";
UnicodeString JulianFormFactory::Buttons_Add_Label			= "Add To Schedule";
UnicodeString JulianFormFactory::Buttons_Close_Type			= "btnNotifySubmitC";
UnicodeString JulianFormFactory::Buttons_Close_Label		= "Close";
UnicodeString JulianFormFactory::Buttons_Accept_Type		= "btnNotifySubmitAccept";
UnicodeString JulianFormFactory::Buttons_Accept_Label		= "Accept";
UnicodeString JulianFormFactory::Buttons_Update_Label		= "Update Schedule";
UnicodeString JulianFormFactory::Buttons_Decline_Type		= "btnNotifySubmitDecline";
UnicodeString JulianFormFactory::Buttons_Decline_Label		= "Decline";
UnicodeString JulianFormFactory::Buttons_Tentative_Type		= "btnNotifySubmitTent";
UnicodeString JulianFormFactory::Buttons_Tentative_Label	= "Tentative";
UnicodeString JulianFormFactory::Buttons_SendFB_Type		= "btnNotifySubmitFB";
UnicodeString JulianFormFactory::Buttons_SendFB_Label		= "Send Free/Busy Time Infomation";
UnicodeString JulianFormFactory::Buttons_SendRefresh_Type	= "btnNotifySubmitERR";
UnicodeString JulianFormFactory::Buttons_SendRefresh_Label	= "Send Refresh";
UnicodeString JulianFormFactory::Buttons_DelTo_Type			= "btnNotifySubmitDelTo";
UnicodeString JulianFormFactory::Buttons_DelTo_Label		= "Delegate to";

UnicodeString JulianFormFactory::Buttons_SaveDel_HTML		= "<INPUT type=checkbox name=btnSaveRefCopy> Save copy of delegated event";

UnicodeString JulianFormFactory::Buttons_GroupStart_HTML	= "<TR><TD VALIGN=CENTER COLSPAN=2 NOWRAP>";
UnicodeString JulianFormFactory::Buttons_GroupEnd_HTML		= "</TD></TR>";
UnicodeString JulianFormFactory::Buttons_GroupSingleStart_HTML= "<TR><TD WIDTH=110>&nbsp;\n</TD><TD>";
UnicodeString JulianFormFactory::Buttons_GroupSingleEnd_HTML= "</TD></TR>";

/*
** Text Fields
*/
UnicodeString JulianFormFactory::Text_Label_Start_HTML		= "<TR VALIGN=TOP><TD VALIGN=CENTER COLSPAN=2>";
UnicodeString JulianFormFactory::Text_Label					= "Comments (optional):&nbsp;";
UnicodeString JulianFormFactory::Text_Label_End_HTML		= "</TD></TR>";
UnicodeString JulianFormFactory::CommentsFieldName			= "txtNotifyComments";
UnicodeString JulianFormFactory::Text_Field_HTML			= "<TR><TD VALIGN=TOP COLSPAN=2><TEXTAREA NAME=txtNotifyComments ROWS=5 COLS=48 wrap=on></TEXTAREA></TD>";

/*
** ICalPropDefs
*/
UnicodeString JulianFormFactory::ICalPreProcess				= "%B%v%J";
UnicodeString JulianFormFactory::ICalPostProcess			= "";

/*
** Status Messages
*/
UnicodeString JulianFormFactory::EventInSchedule			= "This event is already in your schedule\r\n";
UnicodeString JulianFormFactory::EventNotInSchedule			= "This event is not yet in your schedule\r\n";
UnicodeString JulianFormFactory::EventConflict				= "Conflicts:";
UnicodeString JulianFormFactory::EventNote					= "Note:";
UnicodeString JulianFormFactory::EventTest					= "Staff meeting 3:30 - 4:30 PM :-(";
UnicodeString JulianFormFactory::Text_To					= " to ";
UnicodeString JulianFormFactory::Text_AllDay				= " ( Day Event) ";
UnicodeString JulianFormFactory::Text_StartOn				= " Begins on ";
UnicodeString JulianFormFactory::Text_Was					= "Was";

/*
** Muilt Stuff
*/
UnicodeString JulianFormFactory::MuiltEvent_Header_HTML	= "Published Calendar Events";
UnicodeString JulianFormFactory::MuiltFB_Header_HTML	= "Published Calendar Free/Busy";
UnicodeString JulianFormFactory::MuiltEvent				= "This messages contains %d events\r\n";
UnicodeString JulianFormFactory::WhenStr				= "When";
UnicodeString JulianFormFactory::WhatStr				= "What";
UnicodeString JulianFormFactory::SubjectSep				= ": ";

/*
** Publish
*/
UnicodeString JulianFormFactory::publish_Header_HTML		= "Published Event";
int32		  JulianFormFactory::publish_Fields_Labels_Length = 3;
UnicodeString JulianFormFactory::publish_Fields_Labels[]	= { "What:", "When:", "Location:"};
int32		  JulianFormFactory::publish_Fields_Data_HTML_Length = 4;
UnicodeString JulianFormFactory::publish_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%i" };
UnicodeString JulianFormFactory::publish_End_HTML			= "";

/*
** Publish Detail
*/
int32		  JulianFormFactory::publish_D_Fields_Labels_Length	= 14;
UnicodeString JulianFormFactory::publish_D_Fields_Labels[]		= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::publish_D_Fields_Data_HTML_Length = 15;
UnicodeString JulianFormFactory::publish_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Publish VFreeBusy
*/
UnicodeString JulianFormFactory::publishFB_Header_HTML			= "Published Free/Busy";
int32		  JulianFormFactory::publishFB_Fields_Labels_Length	= 2;
UnicodeString JulianFormFactory::publishFB_Fields_Labels[]		= { "What:", "When:"};
int32		  JulianFormFactory::publishFB_Fields_Data_HTML_Length = 2;
UnicodeString JulianFormFactory::publishFB_Fields_Data_HTML[]	= { "%S", "%B" };
UnicodeString JulianFormFactory::publishFB_End_HTML				= "";

/*
** Publish VFreeBusy Detail
*/
int32		  JulianFormFactory::publishFB_D_Fields_Labels_Length	= 4;
UnicodeString JulianFormFactory::publishFB_D_Fields_Labels[]		= { "What:", "When:",  "Created:", "Sent:"};
int32		  JulianFormFactory::publishFB_D_Fields_Data_HTML_Length = 4;
UnicodeString JulianFormFactory::publishFB_D_Fields_Data_HTML[]	= { "%S", "%B", "%t", "%C" };

/*
** Reply VFreeBusy
*/
UnicodeString JulianFormFactory::replyFB_Header_HTML			= "Reply Free/Busy";

/*
** Request VEvent
*/
UnicodeString JulianFormFactory::request_Header_HTML			= "Event Request";
int32		  JulianFormFactory::request_Fields_Labels_Length = 5;
UnicodeString JulianFormFactory::request_Fields_Labels[]		= { "What:", "When:", "Location:", "Organizer:", "Who:"};
int32		  JulianFormFactory::request_Fields_Data_HTML_Length = 6;
UnicodeString JulianFormFactory::request_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%v", "%i" };
UnicodeString JulianFormFactory::request_End_HTML			= "";

/*
** Request Detail VEvent
*/
int32		  JulianFormFactory::request_D_Fields_Labels_Length = 15;
UnicodeString JulianFormFactory::request_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Who:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:"};
int32		  JulianFormFactory::request_D_Fields_Data_HTML_Length = 16;
UnicodeString JulianFormFactory::request_D_Fields_Data_HTML[]= { "%S", "%B", "%L", "%J", "%v", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Request VFreeBusy
*/
UnicodeString JulianFormFactory::request_FB_Header_HTML			= "Free/Busy Time Request";
int32		  JulianFormFactory::request_FB_Fields_Labels_Length = 1;
UnicodeString JulianFormFactory::request_FB_Fields_Labels[]		= { "When:"};
int32		  JulianFormFactory::request_FB_Fields_Data_HTML_Length = 1;
UnicodeString JulianFormFactory::request_FB_Fields_Data_HTML[]	= { "%B" };
UnicodeString JulianFormFactory::request_FB_End_HTML				= "";

/*
** Request VFreeBusy Detail
*/
UnicodeString JulianFormFactory::request_D_FB_Header_HTML		= "Free/Busy Time Request";
int32		  JulianFormFactory::request_D_FB_Fields_Labels_Length = 1;
UnicodeString JulianFormFactory::request_D_FB_Fields_Labels[]	= { "When:"};
int32		  JulianFormFactory::request_D_FB_Fields_Data_HTML_Length = 1;
UnicodeString JulianFormFactory::request_D_FB_Fields_Data_HTML[]	= { "%B" };
UnicodeString JulianFormFactory::request_D_FB_End_HTML			= "";

/*
** Event Reply
*/
UnicodeString JulianFormFactory::eventreply_Header_HTML			= "Event Reply";
int32		  JulianFormFactory::eventreply_Fields_Labels_Length = 2;
UnicodeString JulianFormFactory::eventreply_Fields_Labels[]		= { "Who:", "Status:"};
int32		  JulianFormFactory::eventreply_Fields_Data_HTML_Length = 4;
UnicodeString JulianFormFactory::eventreply_Fields_Data_HTML[]	= { "%v", "%g", "%K", "%i" };
UnicodeString JulianFormFactory::eventreply_End_HTML				= "";

/*
** Event Reply Detail
*/
int32		  JulianFormFactory::eventreply_D_Fields_Labels_Length= 5;
UnicodeString JulianFormFactory::eventreply_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Who:", "Status:" };
int32		  JulianFormFactory::eventreply_D_Fields_Data_HTML_Length = 6;
UnicodeString JulianFormFactory::eventreply_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%v", "%g", "%K", "%i" };

/*
** Event Cancel
*/
UnicodeString JulianFormFactory::eventcancel_Header_HTML			= "Event Cancellation";
int32		  JulianFormFactory::eventcancel_Fields_Labels_Length = 2;
UnicodeString JulianFormFactory::eventcancel_Fields_Labels[]		= { "Status:", "UID:"};
int32		  JulianFormFactory::eventcancel_Fields_Data_HTML_Length = 3;
UnicodeString JulianFormFactory::eventcancel_Fields_Data_HTML[]	= { "%g", "%U", "%K" };
UnicodeString JulianFormFactory::eventcancel_End_HTML			= "";

/*
** Event Cancel Detail
*/
int32		  JulianFormFactory::eventcancel_D_Fields_Labels_Length= 14;
UnicodeString JulianFormFactory::eventcancel_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::eventcancel_D_Fields_Data_HTML_Length = 16;
UnicodeString JulianFormFactory::eventcancel_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%K", "%i" };

/*
** Event Refresh Request
*/
UnicodeString JulianFormFactory::eventrefreshreg_Header_HTML			= "Event Refresh Request";
int32		  JulianFormFactory::eventrefreshreg_Fields_Labels_Length = 2;
UnicodeString JulianFormFactory::eventrefreshreg_Fields_Labels[]		= { "Who:", "UID:"};
int32		  JulianFormFactory::eventrefreshreg_Fields_Data_HTML_Length = 2;
UnicodeString JulianFormFactory::eventrefreshreg_Fields_Data_HTML[]	= { "%v", "%U" };
UnicodeString JulianFormFactory::eventrefreshreg_End_HTML				= "";

/*
** Event Refresh Request Detail
*/
int32		  JulianFormFactory::eventrefreshreg_D_Fields_Labels_Length= 14;
UnicodeString JulianFormFactory::eventrefreshreg_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::eventrefreshreg_D_Fields_Data_HTML_Length = 15;
UnicodeString JulianFormFactory::eventrefreshreg_D_Fields_Data_HTML[]= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Event Counter Proposal
*/
UnicodeString JulianFormFactory::eventcounterprop_Header_HTML		= "Event Counter Proposal";

/*
** Event Deline Counter
*/
UnicodeString JulianFormFactory::eventdelinecounter_Header_HTML		= "Decline Counter Proposal";
int32		  JulianFormFactory::eventdelinecounter_Fields_Labels_Length= 1;
UnicodeString JulianFormFactory::eventdelinecounter_Fields_Labels[]	= { "What:" };
int32		  JulianFormFactory::eventdelinecounter_Fields_Data_HTML_Length = 3;
UnicodeString JulianFormFactory::eventdelinecounter_Fields_Data_HTML[]= { "%S", "%K", "%i" };
