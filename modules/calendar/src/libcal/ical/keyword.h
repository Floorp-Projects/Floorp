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

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * keyword.h
 * John Sun
 * 3/4/98 4:57:30 PM
 */

#include <unistring.h>
#include "jatom.h"

#ifndef __KEYWORD_H_
#define __KEYWORD_H_

/**
 * singleton class to contain all ICAL keywords
 */
class JulianKeyword
{
private:
    static JulianKeyword * m_Instance;
    JulianKeyword();
        
public:
    static JulianKeyword * Instance();
    ~JulianKeyword();

    /* iCALENDAR KEYWORDS*/
    UnicodeString ms_sVCALENDAR;   
    JAtom ms_ATOM_VCALENDAR;

    /* iCALENDAR COMPONENTS KEYWORDS */
    UnicodeString ms_sVEVENT;      
    JAtom ms_ATOM_VEVENT; 

    UnicodeString ms_sVTODO;       
    JAtom ms_ATOM_VTODO;
    UnicodeString ms_sVJOURNAL;    
    JAtom ms_ATOM_VJOURNAL;
    UnicodeString ms_sVFREEBUSY;   
    JAtom ms_ATOM_VFREEBUSY;
    UnicodeString ms_sVTIMEZONE;   
    JAtom ms_ATOM_VTIMEZONE;
    UnicodeString ms_sVALARM;      
    JAtom ms_ATOM_VALARM;
    UnicodeString ms_sTZPART;
    JAtom ms_ATOM_TZPART;
    UnicodeString ms_sTIMEBASEDEVENT;
    UnicodeString ms_sNSCALENDAR;

    /* iCalendar COMPONENT PROPERTY KEYWORDS */
    UnicodeString ms_sATTENDEE;      
    JAtom ms_ATOM_ATTENDEE;
    
    UnicodeString ms_sATTACH;        
    JAtom ms_ATOM_ATTACH;

    UnicodeString ms_sCATEGORIES;
    JAtom ms_ATOM_CATEGORIES;

    UnicodeString ms_sCLASS;
    JAtom ms_ATOM_CLASS;

    UnicodeString ms_sCOMMENT;
    JAtom ms_ATOM_COMMENT;

    UnicodeString ms_sCOMPLETED;
    JAtom ms_ATOM_COMPLETED;

    UnicodeString ms_sCONTACT;
    JAtom ms_ATOM_CONTACT;

    UnicodeString ms_sCREATED;
    JAtom ms_ATOM_CREATED;

    UnicodeString ms_sDTEND;
    JAtom ms_ATOM_DTEND;

    UnicodeString ms_sDTSTART;
    JAtom ms_ATOM_DTSTART;

    UnicodeString ms_sDTSTAMP; 
    JAtom ms_ATOM_DTSTAMP;

    UnicodeString ms_sDESCRIPTION;
    JAtom ms_ATOM_DESCRIPTION;

    UnicodeString ms_sDUE;
    JAtom ms_ATOM_DUE;

    UnicodeString ms_sDURATION;
    JAtom ms_ATOM_DURATION;

    UnicodeString ms_sEXDATE;
    JAtom ms_ATOM_EXDATE;

    UnicodeString ms_sEXRULE;
    JAtom ms_ATOM_EXRULE;

    UnicodeString ms_sFREEBUSY;
    JAtom ms_ATOM_FREEBUSY;

    UnicodeString ms_sGEO;
    JAtom ms_ATOM_GEO;

    UnicodeString ms_sLASTMODIFIED;
    JAtom ms_ATOM_LASTMODIFIED;

    UnicodeString ms_sLOCATION;
    JAtom ms_ATOM_LOCATION;

    UnicodeString ms_sORGANIZER;
    JAtom ms_ATOM_ORGANIZER;

    UnicodeString ms_sPERCENTCOMPLETE;
    JAtom ms_ATOM_PERCENTCOMPLETE;

    UnicodeString ms_sPRIORITY;
    JAtom ms_ATOM_PRIORITY;

    UnicodeString ms_sRDATE;
    JAtom ms_ATOM_RDATE;

    UnicodeString ms_sRRULE;
    JAtom ms_ATOM_RRULE;

    UnicodeString ms_sRECURRENCEID;
    JAtom ms_ATOM_RECURRENCEID;

    /* UnicodeString ms_sRESPONSESEQUENCE = "RESPONSE-SEQUENCE"; */
    /* JAtom ms_ATOM_RESPONSESEQUENCE; */
    
    UnicodeString ms_sRELATEDTO;
    JAtom ms_ATOM_RELATEDTO;

    UnicodeString ms_sREPEAT;
    JAtom ms_ATOM_REPEAT;
 
    UnicodeString ms_sREQUESTSTATUS;
    JAtom ms_ATOM_REQUESTSTATUS;
    
    UnicodeString ms_sRESOURCES;
    JAtom ms_ATOM_RESOURCES;
 
    UnicodeString ms_sSEQUENCE;
    JAtom ms_ATOM_SEQUENCE;
 
    UnicodeString ms_sSTATUS;
    JAtom ms_ATOM_STATUS;
 
    UnicodeString ms_sSUMMARY;
    JAtom ms_ATOM_SUMMARY;
 
    UnicodeString ms_sTRANSP;
    JAtom ms_ATOM_TRANSP;
 
    UnicodeString ms_sTRIGGER;
    JAtom ms_ATOM_TRIGGER;
 
    UnicodeString ms_sUID;
    JAtom ms_ATOM_UID;
 
    UnicodeString ms_sURL;
    JAtom ms_ATOM_URL;
 
    UnicodeString ms_sTZOFFSET;
    JAtom ms_ATOM_TZOFFSET;

    UnicodeString ms_sTZOFFSETTO;
    JAtom ms_ATOM_TZOFFSETTO;
 
    UnicodeString ms_sTZOFFSETFROM;
    JAtom ms_ATOM_TZOFFSETFROM;
 
    UnicodeString ms_sTZNAME;
    JAtom ms_ATOM_TZNAME;
 
    UnicodeString ms_sDAYLIGHT;
    JAtom ms_ATOM_DAYLIGHT;
 
    UnicodeString ms_sSTANDARD;
    JAtom ms_ATOM_STANDARD;

    UnicodeString ms_sTZURL;
    JAtom ms_ATOM_TZURL;

    UnicodeString ms_sTZID;
    JAtom ms_ATOM_TZID;

    /* Boolean value KEYWORDS */
    UnicodeString ms_sTRUE;
    JAtom ms_ATOM_TRUE;
    
    UnicodeString ms_sFALSE;
    JAtom ms_ATOM_FALSE;
  
    /* ITIP METHOD NAME KEYWORDS */
    UnicodeString ms_sPUBLISH;
    JAtom ms_ATOM_PUBLISH;

    UnicodeString ms_sREQUEST;
    JAtom ms_ATOM_REQUEST;

    UnicodeString ms_sREPLY;
    JAtom ms_ATOM_REPLY;

    UnicodeString ms_sCANCEL;
    JAtom ms_ATOM_CANCEL;

    UnicodeString ms_sREFRESH;
    JAtom ms_ATOM_REFRESH;

    UnicodeString ms_sCOUNTER;
    JAtom ms_ATOM_COUNTER;

    UnicodeString ms_sDECLINECOUNTER;
    JAtom ms_ATOM_DECLINECOUNTER;

    UnicodeString ms_sADD;
    JAtom ms_ATOM_ADD;

    /* not really actual method names */
    UnicodeString ms_sDELEGATEREQUEST;
    JAtom ms_ATOM_DELEGATEREQUEST;

    UnicodeString ms_sDELEGATEREPLY;
    JAtom ms_ATOM_DELEGATEREPLY;

    /* NSCALENDAR */
    /* NSCalendar PROPERTY NAMES KEYWORDS */

    UnicodeString ms_sPRODID;
    JAtom ms_ATOM_PRODID;

    UnicodeString ms_sVERSION;
    JAtom ms_ATOM_VERSION;

    UnicodeString ms_sMETHOD;
    JAtom ms_ATOM_METHOD;

    UnicodeString ms_sSOURCE;
    JAtom ms_ATOM_SOURCE;

    UnicodeString ms_sCALSCALE;
    JAtom ms_ATOM_CALSCALE;

    UnicodeString ms_sNAME;
    JAtom ms_ATOM_NAME;

    /* Valid calscale value KEYWORDS, also accepts Iana-scale */
    UnicodeString ms_sGREGORIAN;
    JAtom ms_ATOM_GREGORIAN;
  
    /* End NSCalendar */
  
    /* TimeBasedEvent */
    /* CLASS property Keywords */

    UnicodeString ms_sPUBLIC;
    JAtom ms_ATOM_PUBLIC;

    UnicodeString ms_sPRIVATE;
    JAtom ms_ATOM_PRIVATE;

    UnicodeString ms_sCONFIDENTIAL;
    JAtom ms_ATOM_CONFIDENTIAL;
  
    /* STATUS property Keywords */
    UnicodeString ms_sNEEDSACTION;
    JAtom ms_ATOM_NEEDSACTION;

    /*UnicodeString ms_sCOMPLETED;*/
    /*JAtom ms_ATOM_COMPLETED;*/

    UnicodeString ms_sINPROCESS;
    JAtom ms_ATOM_INPROCESS;

    UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;

    UnicodeString ms_sCONFIRMED;
    JAtom ms_ATOM_CONFIRMED;

    UnicodeString ms_sCANCELLED;
    JAtom ms_ATOM_CANCELLED;

    /* TRANSP property Keywords */
    UnicodeString ms_sOPAQUE;
    JAtom ms_ATOM_OPAQUE;

    UnicodeString ms_sTRANSPARENT;
    JAtom ms_ATOM_TRANSPARENT;
  
    /* End TimeBasedEvent */

    /* ATTENDEE */
    /* parameter names as in the input stream. Define for possible future changes */
    UnicodeString ms_sROLE;
    JAtom ms_ATOM_ROLE;

    /* replaces TYPE */
    UnicodeString ms_sCUTYPE;
    JAtom ms_ATOM_CUTYPE;

    UnicodeString ms_sTYPE;
    JAtom ms_ATOM_TYPE;
    
    UnicodeString ms_sCN;
    JAtom ms_ATOM_CN;

    UnicodeString ms_sPARTSTAT;
    JAtom ms_ATOM_PARTSTAT;

    UnicodeString ms_sRSVP;
    JAtom ms_ATOM_RSVP;

    UnicodeString ms_sEXPECT;
    JAtom ms_ATOM_EXPECT;

    UnicodeString ms_sMEMBER;
    JAtom ms_ATOM_MEMBER;

    UnicodeString ms_sDELEGATED_TO;
    JAtom ms_ATOM_DELEGATED_TO;

    UnicodeString ms_sDELEGATED_FROM;
    JAtom ms_ATOM_DELEGATED_FROM;

    UnicodeString ms_sSENTBY;
    JAtom ms_ATOM_SENTBY;

    UnicodeString ms_sDIR;
    JAtom ms_ATOM_DIR;

    /* ROLE Keywords */
    /*UnicodeString ms_sATTENDEE;*/
    /*JAtom ms_ATOM_ATTENDEE;*/
    /*UnicodeString ms_sORGANIZER;*/
    /*JAtom ms_ATOM_ORGANIZER;*/
    /*UnicodeString ms_sOWNER;*/
    /*JAtom ms_ATOM_OWNER;*/
    /*UnicodeString ms_sDELEGATE;*/
    /*JAtom ms_ATOM_DELEGATE;*/
    UnicodeString ms_sCHAIR;
    JAtom ms_ATOM_CHAIR;
    
    /*UnicodeString ms_sPARTICIPANT;*/
    /*JAtom ms_ATOM_PARTICIPANT;*/
   
    UnicodeString ms_sREQ_PARTICIPANT;
    JAtom ms_ATOM_REQ_PARTICIPANT;
    
    UnicodeString ms_sOPT_PARTICIPANT;
    JAtom ms_ATOM_OPT_PARTICIPANT;

    UnicodeString ms_sNON_PARTICIPANT;
    JAtom ms_ATOM_NON_PARTICIPANT;
  
    /* TYPE Keywords */
    UnicodeString ms_sUNKNOWN;
    JAtom ms_ATOM_UNKNOWN;
    
    UnicodeString ms_sINDIVIDUAL;
    JAtom ms_ATOM_INDIVIDUAL;

    UnicodeString ms_sGROUP;
    JAtom ms_ATOM_GROUP;

    UnicodeString ms_sRESOURCE;
    JAtom ms_ATOM_RESOURCE;

    UnicodeString ms_sROOM;
    JAtom ms_ATOM_ROOM;

    /* STATUS Keywords */
    /*UnicodeString ms_sNEEDSACTION = "NEEDS-ACTION"; */
    /*JAtom ms_ATOM_NEEDSACTION;*/
    
    UnicodeString ms_sACCEPTED; 
    JAtom ms_ATOM_ACCEPTED;

    UnicodeString ms_sDECLINED;
    JAtom ms_ATOM_DECLINED;

    /*UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;

    UnicodeString ms_sCONFIRMED;
    JAtom ms_ATOM_CONFIRMED;

    UnicodeString ms_sCOMPLETED;
    JAtom ms_ATOM_COMPLETED;*/

    UnicodeString ms_sDELEGATED;
    JAtom ms_ATOM_DELEGATED;

    /*UnicodeString ms_sCANCELLED;
    JAtom ms_ATOM_CANCELLED;*/

    UnicodeString ms_sVCALNEEDSACTION;
    JAtom ms_ATOM_VCALNEEDSACTION;
  
    /* RSVP Keywords */
    /*UnicodeString ms_sFALSE;
    JAtom ms_ATOM_FALSE;

    UnicodeString ms_sTRUE;
    JAtom ms_ATOM_TRUE;*/

   /*  EXPECT Keywords*/
    UnicodeString ms_sFYI;
    JAtom ms_ATOM_FYI;
    
    UnicodeString ms_sREQUIRE;
    JAtom ms_ATOM_REQUIRE;

    /*UnicodeString ms_sREQUEST;
    JAtom ms_ATOM_REQUEST;*/

    UnicodeString ms_sIMMEDIATE;
    JAtom ms_ATOM_IMMEDIATE;

    /*End ATTENDEE*/

    /*RecurrenceID*/
    UnicodeString ms_sRANGE;
    JAtom ms_ATOM_RANGE;

    UnicodeString ms_sTHISANDPRIOR;
    JAtom ms_ATOM_THISANDPRIOR;

    UnicodeString ms_sTHISANDFUTURE;
    JAtom ms_ATOM_THISANDFUTURE;

    /*End RecurrenceID*/

    /* FREEBUSY */
    UnicodeString ms_sFBTYPE;
    JAtom ms_ATOM_FBTYPE;

    UnicodeString ms_sBSTAT; /* renamed from status */
    JAtom ms_ATOM_BSTAT;

    UnicodeString ms_sBUSY;
    JAtom ms_ATOM_BUSY;

    UnicodeString ms_sFREE;
    JAtom ms_ATOM_FREE;

    UnicodeString ms_sBUSY_UNAVAILABLE;
    JAtom ms_ATOM_BUSY_UNAVAILABLE;

    UnicodeString ms_sBUSY_TENTATIVE;
    JAtom ms_ATOM_BUSY_TENTATIVE;

    UnicodeString ms_sUNAVAILABLE;
    JAtom ms_ATOM_UNAVAILABLE;

    /*UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;*/

    /*End FREEBUSY*/
  
    /* ALARM */
    /* Alarm Categories keywords */
    UnicodeString ms_sAUDIO;
    JAtom ms_ATOM_AUDIO;

    UnicodeString ms_sDISPLAY;
    JAtom ms_ATOM_DISPLAY;
    
    UnicodeString ms_sEMAIL;
    JAtom ms_ATOM_EMAIL;

    UnicodeString ms_sPROCEDURE;
    JAtom ms_ATOM_PROCEDURE;

    /* End ALARM */
    /*DESCRIPTION*/
    UnicodeString ms_sENCODING;
    JAtom ms_ATOM_ENCODING;

    UnicodeString ms_sCHARSET;
    JAtom ms_ATOM_CHARSET;

    UnicodeString ms_sQUOTED_PRINTABLE;
    JAtom ms_ATOM_QUOTED_PRINTABLE;

    /* End DESCRIPTION */

    /* PARAMETER NAME, VALUE KEYWORDS */
    /*UnicodeString ms_sENCODING;
    JAtom ms_ATOM_ENCODING;*/

    UnicodeString ms_sVALUE;
    JAtom ms_ATOM_VALUE;
    
    UnicodeString ms_sLANGUAGE;
    JAtom ms_ATOM_LANGUAGE;

    UnicodeString ms_sALTREP;
    JAtom ms_ATOM_ALTREP;

    /*UnicodeString ms_sSENTBY;
    JAtom ms_ATOM_SENTBY;*/

    UnicodeString ms_sRELTYPE;
    JAtom ms_ATOM_RELTYPE;

    UnicodeString ms_sRELATED;
    JAtom ms_ATOM_RELATED;

    /*UnicodeString ms_sTZID;
    JAtom ms_ATOM_TZID;*/

    /* Reltype*/
    UnicodeString ms_sPARENT;
    JAtom ms_ATOM_PARENT;
    
    UnicodeString ms_sCHILD;
    JAtom ms_ATOM_CHILD;
    
    UnicodeString ms_sSIBLING;
    JAtom ms_ATOM_SIBLING;

    /* Related*/
    UnicodeString ms_sSTART;
    JAtom ms_ATOM_START;
    
    UnicodeString ms_sEND;
    JAtom ms_ATOM_END;

    /* Encoding types*/
    UnicodeString ms_s8bit;
    JAtom ms_ATOM_8bit;
    
    UnicodeString ms_sBase64;
    JAtom ms_ATOM_Base64;

    UnicodeString ms_s7bit;
    JAtom ms_ATOM_7bit;

    UnicodeString ms_sQ;
    JAtom ms_ATOM_Q;

    UnicodeString ms_sB;
    JAtom ms_ATOM_B;
  

    /* Value types*/
    UnicodeString ms_sURI;
    JAtom ms_ATOM_URI;

    UnicodeString ms_sTEXT;
    JAtom ms_ATOM_TEXT;

    UnicodeString ms_sBINARY;
    JAtom ms_ATOM_BINARY;

    UnicodeString ms_sDATE;
    JAtom ms_ATOM_DATE;

    UnicodeString ms_sRECUR;
    JAtom ms_ATOM_RECUR;

    UnicodeString ms_sTIME;
    JAtom ms_ATOM_TIME;

    UnicodeString ms_sDATETIME;
    JAtom ms_ATOM_DATETIME;

    UnicodeString ms_sPERIOD;
    JAtom ms_ATOM_PERIOD;

    /*UnicodeString ms_sDURATION;
    JAtom ms_ATOM_DURATION;*/

    UnicodeString ms_sBOOLEAN;
    JAtom ms_ATOM_BOOLEAN;

    UnicodeString ms_sINTEGER;
    JAtom ms_ATOM_INTEGER;

    UnicodeString ms_sFLOAT;
    JAtom ms_ATOM_FLOAT;

    UnicodeString ms_sCALADDRESS;
    JAtom ms_ATOM_CALADDRESS;

    UnicodeString ms_sUTCOFFSET;
    JAtom ms_ATOM_UTCOFFSET;

    /*End PARAMETER NAME, VALUE KEYWORDS*/

    /*other useful strings*/
    UnicodeString ms_sMAILTO_COLON;

    UnicodeString ms_sBEGIN;
    JAtom ms_ATOM_BEGIN;
    
    UnicodeString ms_sBEGIN_WITH_COLON;
    JAtom ms_ATOM_BEGIN_WITH_COLON;

    /*UnicodeString ms_sEND;
    JAtom ms_ATOM_END;*/

    UnicodeString ms_sEND_WITH_COLON;
    JAtom ms_ATOM_END_WITH_COLON;

    UnicodeString ms_sBEGIN_VCALENDAR;
    JAtom ms_ATOM_BEGIN_VCALENDAR;

    UnicodeString ms_sEND_VCALENDAR;
    JAtom ms_ATOM_END_VCALENDAR;

    UnicodeString ms_sBEGIN_VFREEBUSY;
    JAtom ms_ATOM_BEGIN_VFREEBUSY;

    UnicodeString ms_sEND_VFREEBUSY;
    JAtom ms_ATOM_END_VFREEBUSY;

    UnicodeString ms_sLINEBREAK;
    JAtom ms_ATOM_LINEBREAK;

    UnicodeString ms_sOK;
    JAtom ms_ATOM_OK;

    UnicodeString ms_sCOMMA_SYMBOL;
    JAtom ms_ATOM_COMMA_SYMBOL;

    UnicodeString ms_sCOLON_SYMBOL;
    JAtom ms_ATOM_COLON_SYMBOL;

    UnicodeString ms_sSEMICOLON_SYMBOL;

    UnicodeString ms_sRRULE_WITH_SEMICOLON;

    UnicodeString ms_sDEFAULT_DELIMS;

    UnicodeString ms_sLINEBREAKSPACE;
    UnicodeString ms_sALTREPQUOTE;
    UnicodeString ms_sLINEFEEDSPACE;

    /*  end other useful strings*/

   /*  recurrence keywords */
    UnicodeString ms_sUNTIL; 
    JAtom ms_ATOM_UNTIL;
    
    UnicodeString ms_sCOUNT;
    JAtom ms_ATOM_COUNT;

    UnicodeString ms_sINTERVAL;
    JAtom ms_ATOM_INTERVAL;

    UnicodeString ms_sFREQ;
    JAtom ms_ATOM_FREQ;

    UnicodeString ms_sBYSECOND;
    JAtom ms_ATOM_BYSECOND;

    UnicodeString ms_sBYMINUTE;
    JAtom ms_ATOM_BYMINUTE;

    UnicodeString ms_sBYHOUR;
    JAtom ms_ATOM_BYHOUR;

    UnicodeString ms_sBYDAY;
    JAtom ms_ATOM_BYDAY;

    UnicodeString ms_sBYMONTHDAY;
    JAtom ms_ATOM_BYMONTHDAY;

    UnicodeString ms_sBYYEARDAY;
    JAtom ms_ATOM_BYYEARDAY;

    UnicodeString ms_sBYWEEKNO;
    JAtom ms_ATOM_BYWEEKNO;

    UnicodeString ms_sBYMONTH;
    JAtom ms_ATOM_BYMONTH;

    UnicodeString ms_sBYSETPOS;
    JAtom ms_ATOM_BYSETPOS;

    UnicodeString ms_sWKST;
    JAtom ms_ATOM_WKST;

    /* frequency values*/

    UnicodeString ms_sSECONDLY;
    JAtom ms_ATOM_SECONDLY;
    UnicodeString ms_sMINUTELY;
    JAtom ms_ATOM_MINUTELY;
    UnicodeString ms_sHOURLY;
    JAtom ms_ATOM_HOURLY;
    UnicodeString ms_sDAILY;
    JAtom ms_ATOM_DAILY;
    UnicodeString ms_sWEEKLY;
    JAtom ms_ATOM_WEEKLY;
    UnicodeString ms_sMONTHLY;
    JAtom ms_ATOM_MONTHLY;
    UnicodeString ms_sYEARLY;
    JAtom ms_ATOM_YEARLY;

    /* day values */

    UnicodeString ms_sSU;
    JAtom ms_ATOM_SU;
    UnicodeString ms_sMO;
    JAtom ms_ATOM_MO;
    UnicodeString ms_sTU;
    JAtom ms_ATOM_TU;
    UnicodeString ms_sWE;
    JAtom ms_ATOM_WE;
    UnicodeString ms_sTH;
    JAtom ms_ATOM_TH;
    UnicodeString ms_sFR;
    JAtom ms_ATOM_FR;
    UnicodeString ms_sSA;
    JAtom ms_ATOM_SA;

    /* helperUnicodeStrings and atoms*/

    UnicodeString ms_sBYDAYYEARLY;
    JAtom ms_ATOM_BYDAYYEARLY;
    UnicodeString ms_sBYDAYMONTHLY;
    JAtom ms_ATOM_BYDAYMONTHLY;
    UnicodeString ms_sBYDAYWEEKLY;
    JAtom ms_ATOM_BYDAYWEEKLY;
    UnicodeString ms_sDEFAULT;
    JAtom ms_ATOM_DEFAULT;

    /* content-type */
    UnicodeString ms_sCONTENT_TRANSFER_ENCODING;
    
};
/**
 *  singleton class that contains JAtom ranges for parameter checking.
 */
class JulianAtomRange
{
private:
    static JulianAtomRange * m_Instance;
    JulianAtomRange();

public:

    ~JulianAtomRange();
    static JulianAtomRange * Instance();

    /* ATOM RANGES for PARAMETERS */
    JAtom ms_asAltrepLanguageParamRange[2];
    t_int32 ms_asAltrepLanguageParamRangeSize;

    JAtom ms_asTZIDValueParamRange[2];
    t_int32 ms_asTZIDValueParamRangeSize;
    
    JAtom ms_asLanguageParamRange[1];
    t_int32 ms_asLanguageParamRangeSize;
    
    JAtom ms_asEncodingValueParamRange[2];
    t_int32 ms_asEncodingValueParamRangeSize;

    JAtom ms_asSentByParamRange[1];
    t_int32 ms_asSentByParamRangeSize;

    JAtom ms_asReltypeParamRange[1];
    t_int32 ms_asReltypeParamRangeSize;

    JAtom ms_asRelatedValueParamRange[2];
    t_int32 ms_asRelatedValueParamRangeSize;

    JAtom ms_asBinaryURIValueRange[2];
    t_int32 ms_asBinaryURIValueRangeSize;

    JAtom ms_asDateDateTimeValueRange[2];
    t_int32 ms_asDateDateTimeValueRangeSize;

    JAtom ms_asDurationDateTimeValueRange[2];
    t_int32 ms_asDurationDateTimeValueRangeSize;

    JAtom ms_asDateDateTimePeriodValueRange[3];
    t_int32 ms_asDateDateTimePeriodValueRangeSize;

    JAtom ms_asRelTypeRange[3];
    JAtom ms_asRelatedRange[2];
    JAtom ms_asParameterRange[5]; 
    t_int32 ms_iParameterRangeSize;
    JAtom ms_asIrregularProperties[3];
    t_int32 ms_iIrregularPropertiesSize;
    JAtom ms_asValueRange[14];
    t_int32 ms_iValueRangeSize;
    JAtom ms_asEncodingRange[2];
    t_int32 ms_iEncodingRangeSize;
};
/*---------------------------------------------------------------------*/

/**
 * Singleton class to contain JLog error messages.
 * For now, messages are not localized.
 * NOTE: TODO: Localize the log error message one day
 */
class JulianLogErrorMessage
{

private:
    static JulianLogErrorMessage * m_Instance;
    JulianLogErrorMessage();

public:
    ~JulianLogErrorMessage();
    static JulianLogErrorMessage * Instance();
   
    UnicodeString ms_sDTEndBeforeDTStart;
    UnicodeString ms_sExportError;
    UnicodeString ms_sNTimeParamError;
    UnicodeString ms_sInvalidPromptValue;
    UnicodeString ms_sSTimeParamError;
    UnicodeString ms_sISO8601ParamError;
    UnicodeString ms_sInvalidTimeStringError;
    UnicodeString ms_sTimeZoneParamError;
    UnicodeString ms_sLocaleParamError;
    UnicodeString ms_sLocaleNotFoundError;
    UnicodeString ms_sPatternParamError;
    UnicodeString ms_sInvalidPatternError;
    UnicodeString ms_sRRuleParamError;
    UnicodeString ms_sERuleParamError;
    UnicodeString ms_sBoundParamError;
    UnicodeString ms_sInvalidRecurrenceError;
    UnicodeString ms_sUnzipNullError;
    UnicodeString ms_sCommandNotFoundError;
    UnicodeString ms_sInvalidTimeZoneError;
    UnicodeString ms_sFileNotFound;
    UnicodeString ms_sInvalidPropertyName;
    UnicodeString ms_sInvalidPropertyValue;
    UnicodeString ms_sInvalidParameterName;
    UnicodeString ms_sInvalidParameterValue;
    UnicodeString ms_sMissingStartingTime;
    UnicodeString ms_sMissingEndingTime;
    UnicodeString ms_sEndBeforeStartTime;
    UnicodeString ms_sMissingSeqNo;
    UnicodeString ms_sMissingReplySeq;
    UnicodeString ms_sMissingURL;
    UnicodeString ms_sMissingDTStamp;
    UnicodeString ms_sMissingUID;
    UnicodeString ms_sMissingDescription;
    UnicodeString ms_sMissingMethodProvided;
    UnicodeString ms_sInvalidVersionNumber;
    UnicodeString ms_sUnknownUID;
    UnicodeString ms_sMissingUIDInReply;
    UnicodeString ms_sMissingValidDTStamp;
    UnicodeString ms_sDeclineCounterCalledByAttendee;
    UnicodeString ms_sPublishCalledByAttendee;
    UnicodeString ms_sRequestCalledByAttendee;
    UnicodeString ms_sCancelCalledByAttendee;
    UnicodeString ms_sCounterCalledByOrganizer;
    UnicodeString ms_sRefreshCalledByOrganizer;
    UnicodeString ms_sReplyCalledByOrganizer;
    UnicodeString ms_sAddReplySequenceOutOfRange;
    UnicodeString ms_sDuplicatedProperty;
    UnicodeString ms_sDuplicatedParameter;
    UnicodeString ms_sConflictMethodAndStatus;
    UnicodeString ms_sConflictCancelAndConfirmedTentative;
    UnicodeString ms_sMissingUIDToMatchEvent;
    UnicodeString ms_sInvalidNumberFormat;
    UnicodeString ms_sDelegateRequestError;
    UnicodeString ms_sInvalidRecurrenceIDRange;
    UnicodeString ms_sUnknownRecurrenceID;
    UnicodeString ms_sPropertyValueTypeMismatch;
    UnicodeString ms_sInvalidRRule;
    UnicodeString ms_sInvalidExRule;
    UnicodeString ms_sInvalidRDate;
    UnicodeString ms_sInvalidExDate;
    UnicodeString ms_sInvalidEvent;
    UnicodeString ms_sInvalidComponent;
    UnicodeString ms_sInvalidAlarm;
    UnicodeString ms_sInvalidTZPart;
    UnicodeString ms_sInvalidAlarmCategory;
    UnicodeString ms_sInvalidAttendee;
    UnicodeString ms_sInvalidFreebusy;
    UnicodeString ms_sDurationAssertionFailed;
    UnicodeString ms_sDurationParseFailed;
    UnicodeString ms_sPeriodParseFailed;
    UnicodeString ms_sPeriodStartInvalid;
    UnicodeString ms_sPeriodEndInvalid;
    UnicodeString ms_sPeriodEndBeforeStart;
    UnicodeString ms_sPeriodDurationZero;
    UnicodeString ms_sFreebusyPeriodInvalid;
    UnicodeString ms_sOptParamInvalidPropertyValue;
    UnicodeString ms_sOptParamInvalidPropertyName;
    UnicodeString ms_sInvalidOptionalParam;
    UnicodeString ms_sAbruptEndOfParsing;
    UnicodeString ms_sLastModifiedBeforeCreated;
    UnicodeString ms_sMultipleOwners;
    UnicodeString ms_sMultipleOrganizers;
    UnicodeString ms_sMissingOwner;
    UnicodeString ms_sMissingDueTime;
    UnicodeString ms_sCompletedPercentMismatch;
    UnicodeString ms_sMissingFreqTagRecurrence;
    UnicodeString ms_sFreqIntervalMismatchRecurrence;
    UnicodeString ms_sInvalidPercentCompleteValue;
    UnicodeString ms_sInvalidPriorityValue;
    UnicodeString ms_sInvalidByHourValue;
    UnicodeString ms_sInvalidByMinuteValue;
    UnicodeString ms_sByDayFreqIntervalMismatch;
    UnicodeString ms_sInvalidByMonthDayValue;
    UnicodeString ms_sInvalidByYearDayValue;
    UnicodeString ms_sInvalidBySetPosValue;
    UnicodeString ms_sInvalidByWeekNoValue;
    UnicodeString ms_sInvalidWeekStartValue;
    UnicodeString ms_sInvalidByMonthValue;
    UnicodeString ms_sInvalidByDayValue;
    UnicodeString ms_sInvalidFrequency;
    UnicodeString ms_sInvalidDayArg;
    UnicodeString ms_sVerifyZeroError;
    UnicodeString ms_sRoundedPercentCompleteTo100;
    UnicodeString ms_sRS200;
    UnicodeString ms_sRS201;
    UnicodeString ms_sRS202;
    UnicodeString ms_sRS203;
    UnicodeString ms_sRS204;
    UnicodeString ms_sRS205;
    UnicodeString ms_sRS206;
    UnicodeString ms_sRS207;
    UnicodeString ms_sRS208;
    UnicodeString ms_sRS209;
    UnicodeString ms_sRS210;
    UnicodeString ms_sRS300;
    UnicodeString ms_sRS301;
    UnicodeString ms_sRS302;
    UnicodeString ms_sRS303;
    UnicodeString ms_sRS304;
    UnicodeString ms_sRS305;
    UnicodeString ms_sRS306;
    UnicodeString ms_sRS307;
    UnicodeString ms_sRS308;
    UnicodeString ms_sRS309;
    UnicodeString ms_sRS310;
    UnicodeString ms_sRS311;
    UnicodeString ms_sRS312;
    UnicodeString ms_sRS400;
    UnicodeString ms_sRS500;
    UnicodeString ms_sRS501;
    UnicodeString ms_sRS502;
    UnicodeString ms_sRS503;
    UnicodeString ms_sMissingUIDGenerateDefault;
    UnicodeString ms_sMissingStartingTimeGenerateDefault;
    UnicodeString ms_sMissingEndingTimeGenerateDefault;
    UnicodeString ms_sNegativeSequenceNumberGenerateDefault;
    UnicodeString ms_sDefaultTBEDescription;
    UnicodeString ms_sDefaultTBEClass;
    UnicodeString ms_sDefaultTBEStatus;
    UnicodeString ms_sDefaultTBETransp;
    UnicodeString ms_sDefaultTBERequestStatus;
    UnicodeString ms_sDefaultTBESummary;
    UnicodeString ms_sDefaultRecIDRange;
    UnicodeString ms_sDefaultAttendeeRole;
    UnicodeString ms_sDefaultAttendeeType;
    UnicodeString ms_sDefaultAttendeeExpect;
    UnicodeString ms_sDefaultAttendeeStatus;
    UnicodeString ms_sDefaultAttendeeRSVP;
    UnicodeString ms_sDefaultAlarmRepeat;
    UnicodeString ms_sDefaultAlarmDuration;
    UnicodeString ms_sDefaultAlarmCategories;
    UnicodeString ms_sDefaultFreebusyStatus;
    UnicodeString ms_sDefaultFreebusyType;
    UnicodeString ms_sDefaultDuration;                        
};

/*---------------------------------------------------------------------*/

/**
 * Singleton class that contains all formatting strings used
 * to print iCalendar object to output.
 */
class JulianFormatString
{

private:
    static JulianFormatString * m_Instance;
    JulianFormatString();

public:
    
    static JulianFormatString * Instance();
    ~JulianFormatString();

    /* DateTime string*/
    UnicodeString ms_asDateTimePatterns[16];
    UnicodeString ms_sDateTimeISO8601Pattern;
    UnicodeString ms_sDateTimeISO8601LocalPattern;
    UnicodeString ms_sDateTimeISO8601TimeOnlyPattern;
    UnicodeString ms_sDateTimeISO8601DateOnlyPattern;
    UnicodeString ms_sDateTimeDefaultPattern;

    /* Attendee strings*/
    UnicodeString ms_sAttendeeAllMessage;
    UnicodeString ms_sAttendeeDoneActionMessage;
    UnicodeString ms_sAttendeeDoneDelegateToOnly;
    UnicodeString ms_sAttendeeDoneDelegateFromOnly;
    UnicodeString ms_sAttendeeNeedsActionMessage;
    UnicodeString ms_sAttendeeNeedsActionDelegateToOnly; 
    UnicodeString ms_sAttendeeNeedsActionDelegateFromOnly; 

    UnicodeString ms_AttendeeStrDefaultFmt;

    /* Organizer strings */
    UnicodeString ms_OrganizerStrDefaultFmt;

    /* Freebusy strings */
    UnicodeString ms_FreebusyStrDefaultFmt;

    /* TZPart strings */
    UnicodeString ms_TZPartStrDefaultFmt;
    UnicodeString ms_sTZPartAllMessage;

    /* VEvent strings */
    UnicodeString ms_VEventStrDefaultFmt;
    UnicodeString ms_sVEventAllPropertiesMessage;    
    UnicodeString ms_sVEventCancelMessage;
    UnicodeString ms_sVEventRequestMessage; 
    UnicodeString ms_sVEventRecurRequestMessage;
    UnicodeString ms_sVEventCounterMessage;
    UnicodeString ms_sVEventDeclineCounterMessage;
    UnicodeString ms_sVEventAddMessage;
    UnicodeString ms_sVEventRefreshMessage;
    UnicodeString ms_sVEventReplyMessage;
    UnicodeString ms_sVEventPublishMessage;
    UnicodeString ms_sVEventRecurPublishMessage;
    UnicodeString ms_sVEventDelegateRequestMessage;
    UnicodeString ms_sVEventRecurDelegateRequestMessage;

    /* VFreebusy strings */
    UnicodeString ms_sVFreebusyReplyMessage;
    UnicodeString ms_sVFreebusyPublishMessage;
    UnicodeString ms_sVFreebusyRequestMessage;
    UnicodeString ms_sVFreebusyAllMessage;
    UnicodeString ms_VFreebusyStrDefaultFmt;

    /* VJournal strings */
    UnicodeString ms_sVJournalAllPropertiesMessage;
    UnicodeString ms_sVJournalCancelMessage;
    UnicodeString ms_sVJournalRequestMessage;
    UnicodeString ms_sVJournalRecurRequestMessage;
    UnicodeString ms_sVJournalCounterMessage;
    UnicodeString ms_sVJournalDeclineCounterMessage;
    UnicodeString ms_sVJournalAddMessage;
    UnicodeString ms_sVJournalRefreshMessage;
    UnicodeString ms_sVJournalReplyMessage;
    UnicodeString ms_sVJournalPublishMessage;
    UnicodeString ms_sVJournalRecurPublishMessage;
    UnicodeString ms_VJournalStrDefaultFmt;

    /* VTodo strings */
    UnicodeString ms_sVTodoAllPropertiesMessage;
    UnicodeString ms_sVTodoCancelMessage;
    UnicodeString ms_sVTodoRequestMessage;
    UnicodeString ms_sVTodoRecurRequestMessage;
    UnicodeString ms_sVTodoCounterMessage;
    UnicodeString ms_sVTodoDeclineCounterMessage;
    UnicodeString ms_sVTodoAddMessage;
    UnicodeString ms_sVTodoRefreshMessage;
    UnicodeString ms_sVTodoReplyMessage;
    UnicodeString ms_sVTodoPublishMessage;
    UnicodeString ms_sVTodoRecurPublishMessage;
    UnicodeString ms_VTodoStrDefaultFmt;

    /* VTimeZone strings */
    UnicodeString ms_VTimeZoneStrDefaultFmt;
    UnicodeString ms_sVTimeZoneAllMessage;
};

#if 0

/* iCALENDAR */
const UnicodeString ms_sVCALENDAR  = "VCALENDAR";   const JAtom ms_ATOM_VCALENDAR(ms_sVCALENDAR);

/* iCALENDAR COMPONENTS*/
const UnicodeString ms_sVEVENT     = "VEVENT";      const JAtom ms_ATOM_VEVENT(ms_sVEVENT); 
const UnicodeString ms_sVTODO      = "VTODO";       const JAtom ms_ATOM_VTODO(ms_sVTODO);
const UnicodeString ms_sVJOURNAL   = "VJOURNAL";    const JAtom ms_ATOM_VJOURNAL(ms_sVJOURNAL);
const UnicodeString ms_sVFREEBUSY  = "VFREEBUSY";   const JAtom ms_ATOM_VFREEBUSY(ms_sVFREEBUSY);
const UnicodeString ms_sVTIMEZONE  = "VTIMEZONE";   const JAtom ms_ATOM_VTIMEZONE(ms_sVTIMEZONE);
const UnicodeString ms_sVALARM     = "VALARM";      const JAtom ms_ATOM_VALARM(ms_sVALARM);
const UnicodeString ms_sTZPART     = "TZPART";

/*PROPERTIES*/
const UnicodeString ms_sATTENDEE         = "ATTENDEE";      const JAtom ms_ATOM_ATTENDEE(ms_sATTENDEE);
const UnicodeString ms_sATTACH           = "ATTACH";        const JAtom ms_ATOM_ATTACH(ms_sATTACH);
const UnicodeString ms_sCATEGORIES       = "CATEGORIES";    const JAtom ms_ATOM_CATEGORIES(ms_sCATEGORIES);
const UnicodeString ms_sCLASS            = "CLASS";         const JAtom ms_ATOM_CLASS(ms_sCLASS);
const UnicodeString ms_sCOMMENT          = "COMMENT";       const JAtom ms_ATOM_COMMENT(ms_sCOMMENT);
const UnicodeString ms_sCOMPLETED        = "COMPLETED";     const JAtom ms_ATOM_COMPLETED(ms_sCOMPLETED);
const UnicodeString ms_sCONTACT          = "CONTACT";       const JAtom ms_ATOM_CONTACT(ms_sCONTACT);
const UnicodeString ms_sCREATED          = "CREATED";       const JAtom ms_ATOM_CREATED(ms_sCREATED);
const UnicodeString ms_sDTEND            = "DTEND";         const JAtom ms_ATOM_DTEND(ms_sDTEND);
const UnicodeString ms_sDTSTART          = "DTSTART";       const JAtom ms_ATOM_DTSTART(ms_sDTSTART);
const UnicodeString ms_sDTSTAMP          = "DTSTAMP";       const JAtom ms_ATOM_DTSTAMP(ms_sDTSTAMP);
const UnicodeString ms_sDESCRIPTION      = "DESCRIPTION";   const JAtom ms_ATOM_DESCRIPTION(ms_sDESCRIPTION);
const UnicodeString ms_sDUE              = "DUE";       const JAtom ms_ATOM_DUE(ms_sDUE);
const UnicodeString ms_sDURATION         = "DURATION";  const JAtom ms_ATOM_DURATION(ms_sDURATION);
const UnicodeString ms_sEXDATE           = "EXDATE";    const JAtom ms_ATOM_EXDATE(ms_sEXDATE);
const UnicodeString ms_sEXRULE           = "EXRULE";    const JAtom ms_ATOM_EXRULE(ms_sEXRULE);
const UnicodeString ms_sFREEBUSY         = "FREEBUSY";  const JAtom ms_ATOM_FREEBUSY(ms_sFREEBUSY);
const UnicodeString ms_sGEO              = "GEO";       const JAtom ms_ATOM_GEO(ms_sGEO);
const UnicodeString ms_sLASTMODIFIED     = "LAST-MODIFIED"; const JAtom ms_ATOM_LASTMODIFIED(ms_sLASTMODIFIED);
const UnicodeString ms_sLOCATION         = "LOCATION";  const JAtom ms_ATOM_LOCATION(ms_sLOCATION);
const UnicodeString ms_sORGANIZER        = "ORGANIZER"; const JAtom ms_ATOM_ORGANIZER(ms_sORGANIZER);
const UnicodeString ms_sPERCENTCOMPLETE  = "PERCENT-COMPLETE"; const JAtom ms_ATOM_PERCENTCOMPLETE(ms_sPERCENTCOMPLETE);
const UnicodeString ms_sPRIORITY         = "PRIORITY";  const JAtom ms_ATOM_PRIORITY(ms_sPRIORITY);
const UnicodeString ms_sRDATE            = "RDATE";     const JAtom ms_ATOM_RDATE(ms_sRDATE);
const UnicodeString ms_sRRULE            = "RRULE";     const JAtom ms_ATOM_RRULE(ms_sRRULE);
const UnicodeString ms_sRECURRENCEID     = "RECURRENCE-ID"; const JAtom ms_ATOM_RECURRENCEID(ms_sRECURRENCEID);
/*const UnicodeString ms_sRESPONSESEQUENCE = "RESPONSE-SEQUENCE"; const JAtom ms_ATOM_RESPONSESEQUENCE(ms_sRESPONSESEQUENCE);*/
const UnicodeString ms_sRELATEDTO        = "RELATED-TO";    const JAtom ms_ATOM_RELATEDTO(ms_sRELATEDTO);
const UnicodeString ms_sREPEAT           = "REPEAT";    const JAtom ms_ATOM_REPEAT(ms_sREPEAT);
const UnicodeString ms_sREQUESTSTATUS    = "REQUEST-STATUS"; const JAtom ms_ATOM_REQUESTSTATUS(ms_sREQUESTSTATUS);
const UnicodeString ms_sRESOURCES        = "RESOURCES"; const JAtom ms_ATOM_RESOURCES(ms_sRESOURCES);
const UnicodeString ms_sSEQUENCE         = "SEQUENCE";  const JAtom ms_ATOM_SEQUENCE(ms_sSEQUENCE);
const UnicodeString ms_sSTATUS           = "STATUS";    const JAtom ms_ATOM_STATUS(ms_sSTATUS);
const UnicodeString ms_sSUMMARY          = "SUMMARY";   const JAtom ms_ATOM_SUMMARY(ms_sSUMMARY);
const UnicodeString ms_sTRANSP           = "TRANSP";    const JAtom ms_ATOM_TRANSP(ms_sTRANSP);
const UnicodeString ms_sTRIGGER          = "TRIGGER";   const JAtom ms_ATOM_TRIGGER(ms_sTRIGGER);
const UnicodeString ms_sUID              = "UID"; const JAtom ms_ATOM_UID(ms_sUID);
const UnicodeString ms_sURL              = "URL"; const JAtom ms_ATOM_URL(ms_sURL);
const UnicodeString ms_sTZOFFSET         = "TZOFFSET"; const JAtom ms_ATOM_TZOFFSET(ms_sTZOFFSET);
const UnicodeString ms_sTZOFFSETTO       = "TZOFFSETTO"; const JAtom ms_ATOM_TZOFFSETTO(ms_sTZOFFSETTO);
const UnicodeString ms_sTZOFFSETFROM     = "TZOFFSETFROM"; const JAtom ms_ATOM_TZOFFSETFROM(ms_sTZOFFSETFROM);
const UnicodeString ms_sTZNAME           = "TZNAME"; const JAtom ms_ATOM_TZNAME(ms_sTZNAME);
const UnicodeString ms_sDAYLIGHT         = "DAYLIGHT"; const JAtom ms_ATOM_DAYLIGHT(ms_sDAYLIGHT);
const UnicodeString ms_sSTANDARD         = "STANDARD"; const JAtom ms_ATOM_STANDARD(ms_sSTANDARD);
const UnicodeString ms_sTZURL            = "TZURL"; const JAtom ms_ATOM_TZURL(ms_sTZURL);
const UnicodeString ms_sTZID             = "TZID"; const JAtom ms_ATOM_TZID(ms_sTZID);

/*boolean value strings*/
const UnicodeString ms_sTRUE      = "TRUE"; const JAtom ms_ATOM_TRUE(ms_sTRUE);
const UnicodeString ms_sFALSE     = "FALSE"; const JAtom ms_ATOM_FALSE(ms_sFALSE);
  
/* ITIP METHOD NAMES*/
const UnicodeString ms_sPUBLISH        = "PUBLISH"; const JAtom ms_ATOM_PUBLISH(ms_sPUBLISH);
const UnicodeString ms_sREQUEST        = "REQUEST"; const JAtom ms_ATOM_REQUEST(ms_sREQUEST);
const UnicodeString ms_sREPLY          = "REPLY"; const JAtom ms_ATOM_REPLY(ms_sREPLY);
const UnicodeString ms_sCANCEL         = "CANCEL"; const JAtom ms_ATOM_CANCEL(ms_sCANCEL);
/*const UnicodeString ms_sRESEND         = "RESEND"; const JAtom ms_ATOM_RESEND(ms_sRESEND);*/
const UnicodeString ms_sREFRESH        = "REFRESH"; const JAtom ms_ATOM_REFRESH(ms_sREFRESH);
const UnicodeString ms_sCOUNTER        = "COUNTER"; const JAtom ms_ATOM_COUNTER(ms_sCOUNTER);
const UnicodeString ms_sDECLINECOUNTER = "DECLINECOUNTER"; const JAtom ms_ATOM_DECLINECOUNTER(ms_sDECLINECOUNTER);
const UnicodeString ms_sADD            = "ADD"; const JAtom ms_ATOM_ADD(ms_sADD);
/*const UnicodeString ms_sBUSYREQUEST   = "BUSY-REQUEST"; const JAtom ms_ATOM_BUSYREQUEST(ms_sBUSYREQUEST);
const UnicodeString ms_sBUSYREPLY     = "BUSY-REPLY"; const JAtom ms_ATOM_BUSYREPLY(ms_sBUSYREPLY);*/
  
/*NSCALENDAR*/
/* NSCalendar PROPERTY NAMES */
const UnicodeString ms_sPRODID   = "PRODID"; const JAtom ms_ATOM_PRODID(ms_sPRODID);
const UnicodeString ms_sVERSION  = "VERSION"; const JAtom ms_ATOM_VERSION(ms_sVERSION);
const UnicodeString ms_sMETHOD   = "METHOD"; const JAtom ms_ATOM_METHOD(ms_sMETHOD);
const UnicodeString ms_sSOURCE   = "SOURCE"; const JAtom ms_ATOM_SOURCE(ms_sSOURCE);
const UnicodeString ms_sCALSCALE = "CALSCALE"; const JAtom ms_ATOM_CALSCALE(ms_sCALSCALE);
const UnicodeString ms_sNAME     = "NAME"; const JAtom ms_ATOM_NAME(ms_sNAME);
/*const UnicodeString ms_sPROFILE_VERSION = "PROFILE-VERSION"; const JAtom ms_ATOM_PROFILE_VERSION(ms_sPROFILE_VERSION);*/

/* Valid calscale values, also accepts Iana-scale*/
const UnicodeString ms_sGREGORIAN = "GREGORIAN"; const JAtom ms_ATOM_GREGORIAN(ms_sGREGORIAN);
  
/* End NSCalendar */
  
/*TimeBasedEvent*/
/* CLASS property Keywords*/
const UnicodeString ms_sPUBLIC        = "PUBLIC"; const JAtom ms_ATOM_PUBLIC(ms_sPUBLIC);
const UnicodeString ms_sPRIVATE       = "PRIVATE"; const JAtom ms_ATOM_PRIVATE(ms_sPRIVATE);
const UnicodeString ms_sCONFIDENTIAL  = "CONFIDENTIAL"; const JAtom ms_ATOM_CONFIDENTIAL(ms_sCONFIDENTIAL);
  
/* STATUS property Keywords*/
const UnicodeString ms_sNEEDSACTION   = "NEEDS-ACTION"; const JAtom ms_ATOM_NEEDSACTION(ms_sNEEDSACTION);
/*const UnicodeString ms_sCOMPLETED     = "COMPLETED"; const JAtom ms_ATOM_COMPLETED(ms_sCOMPLETED);*/
const UnicodeString ms_sINPROCESS     = "IN-PROCESS"; const JAtom ms_ATOM_INPROCESS(ms_sINPROCESS);
const UnicodeString ms_sTENTATIVE     = "TENTATIVE"; const JAtom ms_ATOM_TENTATIVE(ms_sTENTATIVE);
const UnicodeString ms_sCONFIRMED     = "CONFIRMED"; const JAtom ms_ATOM_CONFIRMED(ms_sCONFIRMED);
const UnicodeString ms_sCANCELLED     = "CANCELLED"; const JAtom ms_ATOM_CANCELLED(ms_sCANCELLED);

/* TRANSP property Keywords*/
const UnicodeString ms_sOPAQUE        = "OPAQUE"; const JAtom ms_ATOM_OPAQUE(ms_sOPAQUE);
const UnicodeString ms_sTRANSPARENT   = "TRANSPARENT"; const JAtom ms_ATOM_TRANSPARENT(ms_sTRANSPARENT);
  
/*End of TimeBasedEvent*/

/*ATTENDEE*/
/* parameter names as in the input stream. Define for possible future changes*/
const UnicodeString ms_sROLE             = "ROLE"; const JAtom ms_ATOM_ROLE(ms_sROLE);
const UnicodeString ms_sTYPE             = "TYPE"; const JAtom ms_ATOM_TYPE(ms_sTYPE);
/*const UnicodeString ms_sSTATUS           = "STATUS"; const JAtom ms_ATOM_STATUS(ms_sSTATUS);*/
const UnicodeString ms_sRSVP             = "RSVP"; const JAtom ms_ATOM_RSVP(ms_sRSVP);
const UnicodeString ms_sEXPECT           = "EXPECT"; const JAtom ms_ATOM_EXPECT(ms_sEXPECT);
const UnicodeString ms_sMEMBER           = "MEMBER"; const JAtom ms_ATOM_MEMBER(ms_sMEMBER);
const UnicodeString ms_sDELEGATED_TO     = "DELEGATED-TO"; const JAtom ms_ATOM_DELEGATED_TO(ms_sDELEGATED_TO);
const UnicodeString ms_sDELEGATED_FROM   = "DELEGATED-FROM"; const JAtom ms_ATOM_DELEGATED_FROM(ms_sDELEGATED_FROM);

/* ROLE Keywords*/
/*const UnicodeString ms_sATTENDEE    = "ATTENDEE"; const JAtom ms_ATOM_ATTENDEE(ms_sATTENDEE);
const UnicodeString ms_sORGANIZER   = "ORGANIZER"; const JAtom ms_ATOM_ORGANIZER(ms_sORGANIZER);
const UnicodeString ms_sOWNER       = "OWNER"; const JAtom ms_ATOM_OWNER(ms_sOWNER);
const UnicodeString ms_sDELEGATE    = "DELEGATE"; const JAtom ms_ATOM_DELEGATE(ms_sDELEGATE);*/
const UnicodeString ms_sCHAIR         =   "CHAIR"; const JAtom ms_ATOM_CHAIR(ms_sCHAIR);
const UnicodeString ms_sPARTICIPANT   =   "PARTICIPANT"; const JAtom ms_ATOM_PARTICIPANT(ms_sPARTICIPANT);
const UnicodeString ms_sNON_PARTICIPANT = "NON-PARTICIPANT"; const JAtom ms_ATOM_NON_PARTICIPANT(ms_sNON_PARTICIPANT);
  
/* TYPE Keywords*/
const UnicodeString ms_sUNKNOWN     = "UNKNOWN"; const JAtom ms_ATOM_UNKNOWN(ms_sUNKNOWN);
const UnicodeString ms_sINDIVIDUAL  = "INDIVIDUAL"; const JAtom ms_ATOM_INDIVIDUAL(ms_sINDIVIDUAL);
const UnicodeString ms_sGROUP       = "GROUP"; const JAtom ms_ATOM_GROUP(ms_sGROUP);
const UnicodeString ms_sRESOURCE    = "RESOURCE"; const JAtom ms_ATOM_RESOURCE(ms_sRESOURCE);
const UnicodeString ms_sROOM        = "ROOM"; const JAtom ms_ATOM_ROOM(ms_sROOM);

/* STATUS Keywords*/
/*const UnicodeString ms_sNEEDSACTION = "NEEDS-ACTION"; const JAtom ms_ATOM_NEEDSACTION(ms_sNEEDSACTION);*/
const UnicodeString ms_sACCEPTED    = "ACCEPTED"; const JAtom ms_ATOM_ACCEPTED(ms_sACCEPTED);
const UnicodeString ms_sDECLINED    = "DECLINED"; const JAtom ms_ATOM_DECLINED(ms_sDECLINED);
/*const UnicodeString ms_sTENTATIVE   = "TENTATIVE"; const JAtom ms_ATOM_TENTATIVE(ms_sTENTATIVE);
const UnicodeString ms_sCONFIRMED   = "CONFIRMED"; const JAtom ms_ATOM_CONFIRMED(ms_sCONFIRMED);
const UnicodeString ms_sCOMPLETED   = "COMPLETED"; const JAtom ms_ATOM_COMPLETED(ms_sCOMPLETED);*/
const UnicodeString ms_sDELEGATED   = "DELEGATED"; const JAtom ms_ATOM_DELEGATED(ms_sDELEGATED);
/*const UnicodeString ms_sCANCELLED   = "CANCELLED"; const JAtom ms_ATOM_CANCELLED(ms_sCANCELLED);*/
const UnicodeString ms_sVCALNEEDSACTION = "NEEDS ACTION"; const JAtom ms_ATOM_VCALNEEDSACTION(ms_sVCALNEEDSACTION);
  
/* RSVP Keywords*/
/*const UnicodeString ms_sFALSE       = "FALSE"; const JAtom ms_ATOM_FALSE(ms_sFALSE);
const UnicodeString ms_sTRUE        = "TRUE"; const JAtom ms_ATOM_TRUE(ms_sTRUE);*/

/* EXPECT Keywords*/
const UnicodeString ms_sFYI         = "FYI"; const JAtom ms_ATOM_FYI(ms_sFYI);
const UnicodeString ms_sREQUIRE     = "REQUIRE"; const JAtom ms_ATOM_REQUIRE(ms_sREQUIRE);
/*const UnicodeString ms_sREQUEST     = "REQUEST"; const JAtom ms_ATOM_REQUEST(ms_sREQUEST);*/
const UnicodeString ms_sIMMEDIATE   = "IMMEDIATE"; const JAtom ms_ATOM_IMMEDIATE(ms_sIMMEDIATE);

/* End of ATTENDEE */
/* RecurrenceID */
const UnicodeString ms_sRANGE          = "RANGE"; const JAtom ms_ATOM_RANGE(ms_sRANGE);
const UnicodeString ms_sTHISANDPRIOR   = "THISANDPRIOR"; const JAtom ms_ATOM_THISANDPRIOR(ms_sTHISANDPRIOR);
const UnicodeString ms_sTHISANDFUTURE  = "THISANDFUTURE"; const JAtom ms_ATOM_THISANDFUTURE(ms_sTHISANDFUTURE);
/*End of RecurrenceID*/

/*FREEBUSY*/
/*const UnicodeString ms_sTYPE      = "TYPE"; const JAtom ms_ATOM_TYPE(ms_sTYPE);
const UnicodeString ms_sSTATUS    = "STATUS"; const JAtom ms_ATOM_STATUS(ms_sSTATUS);*/
const UnicodeString ms_sBUSY = "BUSY"; const JAtom ms_ATOM_BUSY(ms_sBUSY);
const UnicodeString ms_sFREE = "FREE"; const JAtom ms_ATOM_FREE(ms_sFREE);
const UnicodeString ms_sUNAVAILABLE = "UNAVAILABLE"; const JAtom ms_ATOM_UNAVAILABLE(ms_sUNAVAILABLE);
/*const UnicodeString ms_sTENTATIVE = "TENTATIVE"; const JAtom ms_ATOM_TENTATIVE(ms_sTENTATIVE);*/
/*End of FREEBUSY*/
  
/*ALARM*/
/* Alarm Categories keywords*/
const UnicodeString ms_sAUDIO = "AUDIO"; const JAtom ms_ATOM_AUDIO(ms_sAUDIO);
const UnicodeString ms_sDISPLAY = "DISPLAY"; const JAtom ms_ATOM_DISPLAY(ms_sDISPLAY);
const UnicodeString ms_sEMAIL = "EMAIL"; const JAtom ms_ATOM_EMAIL(ms_sEMAIL);
const UnicodeString ms_sPROCEDURE = "PROCEDURE"; const JAtom ms_ATOM_PROCEDURE(ms_sPROCEDURE);
/*End of ALARM*/

/*DESCRIPTION*/
const UnicodeString ms_sENCODING = "ENCODING"; const JAtom ms_ATOM_ENCODING(ms_sENCODING);
const UnicodeString ms_sCHARSET = "CHARSET"; const JAtom ms_ATOM_CHARSET(ms_sCHARSET);
const UnicodeString ms_sQUOTED_PRINTABLE = "QUOTED-PRINTABLE"; const JAtom ms_ATOM_QUOTED_PRINTABLE(ms_sQUOTED_PRINTABLE);
/*End of DESCRIPTION*/

/*PARSER UTIL*/
/*const UnicodeString ms_sENCODING  = "ENCODING"; const JAtom ms_ATOM_ENCODING(ms_sENCODING);*/
const UnicodeString ms_sVALUE     = "VALUE"; const JAtom ms_ATOM_VALUE(ms_sVALUE);
const UnicodeString ms_sLANGUAGE  = "LANGUAGE"; const JAtom ms_ATOM_LANGUAGE(ms_sLANGUAGE);
const UnicodeString ms_sALTREP    = "ALTREP"; const JAtom ms_ATOM_ALTREP(ms_sALTREP);
const UnicodeString ms_sSENTBY    = "SENT-BY"; const JAtom ms_ATOM_SENTBY(ms_sSENTBY);
const UnicodeString ms_sRELTYPE    = "RELTYPE"; const JAtom ms_ATOM_RELTYPE(ms_sRELTYPE);
const UnicodeString ms_sRELATED    = "RELATED"; const JAtom ms_ATOM_RELATED(ms_sRELATED);
/*const UnicodeString ms_sTZID             = "TZID"; const JAtom ms_ATOM_TZID(ms_sTZID);*/

/* Reltype*/
const UnicodeString ms_sPARENT    = "PARENT"; const JAtom ms_ATOM_PARENT(ms_sPARENT);
const UnicodeString ms_sCHILD    = "CHILD"; const JAtom ms_ATOM_CHILD(ms_sCHILD);
const UnicodeString ms_sSIBLING    = "SIBLING"; const JAtom ms_ATOM_SIBLING(ms_sSIBLING);

/* Related*/
const UnicodeString ms_sSTART    = "START"; const JAtom ms_ATOM_START(ms_sSTART);
const UnicodeString ms_sEND    = "END"; const JAtom ms_ATOM_END(ms_sEND);

/* Encoding types*/
const UnicodeString ms_s8bit = "8bit"; const JAtom ms_ATOM_8bit(ms_s8bit);
const UnicodeString ms_s7bit = "7bit"; const JAtom ms_ATOM_7bit(ms_s7bit);
const UnicodeString ms_sQ    = "Q"; const JAtom ms_ATOM_Q(ms_sQ);
const UnicodeString ms_sB    = "B"; const JAtom ms_ATOM_B(ms_sB);
  
/* Value types*/
const UnicodeString ms_sURI = "URI"; const JAtom ms_ATOM_URI(ms_sURI);
const UnicodeString ms_sTEXT = "TEXT"; const JAtom ms_ATOM_TEXT(ms_sTEXT);
const UnicodeString ms_sBINARY = "BINARY"; const JAtom ms_ATOM_BINARY(ms_sBINARY);
const UnicodeString ms_sDATE = "DATE"; const JAtom ms_ATOM_DATE(ms_sDATE);
const UnicodeString ms_sRECUR = "RECUR"; const JAtom ms_ATOM_RECUR(ms_sRECUR);
const UnicodeString ms_sTIME = "TIME"; const JAtom ms_ATOM_TIME(ms_sTIME);
const UnicodeString ms_sDATETIME = "DATE-TIME"; const JAtom ms_ATOM_DATETIME(ms_sDATETIME);
const UnicodeString ms_sPERIOD = "PERIOD"; const JAtom ms_ATOM_PERIOD(ms_sPERIOD);
/*const UnicodeString ms_sDURATION = "DURATION"; const JAtom ms_ATOM_DURATION(ms_sDURATION);*/
const UnicodeString ms_sBOOLEAN = "BOOLEAN"; const JAtom ms_ATOM_BOOLEAN(ms_sBOOLEAN);
const UnicodeString ms_sINTEGER = "INTEGER"; const JAtom ms_ATOM_INTEGER(ms_sINTEGER);
const UnicodeString ms_sFLOAT = "FLOAT"; const JAtom ms_ATOM_FLOAT(ms_sFLOAT);
const UnicodeString ms_sCALADDRESS = "CAL-ADDRESS"; const JAtom ms_ATOM_CALADDRESS(ms_sCALADDRESS);
const UnicodeString ms_sUTCOFFSET = "UTC-OFFSET"; const JAtom ms_ATOM_UTCOFFSET(ms_sUTCOFFSET);
/* End of PARSER UTIL */

/*other*/
const UnicodeString ms_sBEGIN = "BEGIN"; const JAtom ms_ATOM_BEGIN(ms_sBEGIN);
const UnicodeString ms_sBEGIN_WITH_COLON = "BEGIN:"; const JAtom ms_ATOM_BEGIN_WITH_COLON(ms_sBEGIN_WITH_COLON);
/*const UnicodeString ms_sEND = "END"; const JAtom ms_ATOM_END(ms_sEND);*/
const UnicodeString ms_sEND_WITH_COLON = "END:"; const JAtom ms_ATOM_END_WITH_COLON(ms_sEND_WITH_COLON);
const UnicodeString ms_sBEGIN_VCALENDAR = "BEGIN:VCALENDAR"; const JAtom ms_ATOM_BEGIN_VCALENDAR(ms_sBEGIN_VCALENDAR);
const UnicodeString ms_sEND_VCALENDAR= "END:VCALENDAR"; const JAtom ms_ATOM_END_VCALENDAR(ms_sEND_VCALENDAR);
const UnicodeString ms_sBEGIN_VFREEBUSY = "BEGIN:VFREEBUSY"; const JAtom ms_ATOM_BEGIN_VFREEBUSY(ms_sBEGIN_VFREEBUSY);
const UnicodeString ms_sEND_VFREEBUSY= "END:VFREEBUSY"; const JAtom ms_ATOM_END_VFREEBUSY(ms_sEND_VFREEBUSY);
const UnicodeString ms_sLINEBREAK = "\r\n"; const JAtom ms_ATOM_LINEBREAK(ms_sLINEBREAK);
const UnicodeString ms_sOK = "OK"; const JAtom ms_ATOM_OK(ms_sOK);
const UnicodeString ms_sCOMMA_SYMBOL = ","; const JAtom ms_ATOM_COMMA_SYMBOL(ms_sCOMMA_SYMBOL);
const UnicodeString ms_sCOLON_SYMBOL = ":"; const JAtom ms_ATOM_COLON_SYMBOL(ms_sCOLON_SYMBOL);

/* Recurrence strings*/

/* keywords*/

const UnicodeString ms_sUNTIL = "UNTIL"; const JAtom ms_ATOM_UNTIL(ms_sUNTIL);
const UnicodeString ms_sCOUNT = "COUNT"; const JAtom ms_ATOM_COUNT(ms_sCOUNT);
const UnicodeString ms_sINTERVAL = "INTERVAL"; const JAtom ms_ATOM_INTERVAL(ms_sINTERVAL);
const UnicodeString ms_sFREQ = "FREQ"; const JAtom ms_ATOM_FREQ(ms_sFREQ);
const UnicodeString ms_sBYSECOND = "BYSECOND"; const JAtom ms_ATOM_BYSECOND(ms_sBYSECOND);
const UnicodeString ms_sBYMINUTE = "BYMINUTE"; const JAtom ms_ATOM_BYMINUTE(ms_sBYMINUTE);
const UnicodeString ms_sBYHOUR = "BYHOUR"; const JAtom ms_ATOM_BYHOUR(ms_sBYHOUR);
const UnicodeString ms_sBYDAY = "BYDAY"; const JAtom ms_ATOM_BYDAY(ms_sBYDAY);
const UnicodeString ms_sBYMONTHDAY = "BYMONTHDAY"; const JAtom ms_ATOM_BYMONTHDAY(ms_sBYMONTHDAY);
const UnicodeString ms_sBYYEARDAY = "BYYEARDAY"; const JAtom ms_ATOM_BYYEARDAY(ms_sBYYEARDAY);
const UnicodeString ms_sBYWEEKNO = "BYWEEKNO"; const JAtom ms_ATOM_BYWEEKNO(ms_sBYWEEKNO);
const UnicodeString ms_sBYMONTH = "BYMONTH"; const JAtom ms_ATOM_BYMONTH(ms_sBYMONTH);
const UnicodeString ms_sBYSETPOS = "BYSETPOS"; const JAtom ms_ATOM_BYSETPOS(ms_sBYSETPOS);
const UnicodeString ms_sWKST = "WKST"; const JAtom ms_ATOM_WKST(ms_sWKST);

/* frequency values*/

const UnicodeString ms_sSECONDLY = "SECONDLY"; const JAtom ms_ATOM_SECONDLY(ms_sSECONDLY);
const UnicodeString ms_sMINUTELY = "MINUTELY"; const JAtom ms_ATOM_MINUTELY(ms_sMINUTELY);
const UnicodeString ms_sHOURLY = "HOURLY"; const JAtom ms_ATOM_HOURLY(ms_sHOURLY);
const UnicodeString ms_sDAILY = "DAILY"; const JAtom ms_ATOM_DAILY(ms_sDAILY);
const UnicodeString ms_sWEEKLY = "WEEKLY"; const JAtom ms_ATOM_WEEKLY(ms_sWEEKLY);
const UnicodeString ms_sMONTHLY = "MONTHLY"; const JAtom ms_ATOM_MONTHLY(ms_sMONTHLY);
const UnicodeString ms_sYEARLY = "YEARLY"; const JAtom ms_ATOM_YEARLY(ms_sYEARLY);

/* day values*/

const UnicodeString ms_sSU = "SU"; const JAtom ms_ATOM_SU(ms_sSU);
const UnicodeString ms_sMO = "MO"; const JAtom ms_ATOM_MO(ms_sMO);
const UnicodeString ms_sTU = "TU"; const JAtom ms_ATOM_TU(ms_sTU);
const UnicodeString ms_sWE = "WE"; const JAtom ms_ATOM_WE(ms_sWE);
const UnicodeString ms_sTH = "TH"; const JAtom ms_ATOM_TH(ms_sTH);
const UnicodeString ms_sFR = "FR"; const JAtom ms_ATOM_FR(ms_sFR);
const UnicodeString ms_sSA = "SA"; const JAtom ms_ATOM_SA(ms_sSA);

/* helperconst UnicodeStrings and atoms*/

const UnicodeString ms_sBYDAYYEARLY = "BYDAYYEARLY"; const JAtom ms_ATOM_BYDAYYEARLY(ms_sBYDAYYEARLY);
const UnicodeString ms_sBYDAYMONTHLY = "BYDAYMONTHLY"; const JAtom ms_ATOM_BYDAYMONTHLY(ms_sBYDAYMONTHLY);
const UnicodeString ms_sBYDAYWEEKLY = "BYDAYWEEKLY"; const JAtom ms_ATOM_BYDAYWEEKLY(ms_sBYDAYWEEKLY);
const UnicodeString ms_sDEFAULT = "DEFAULT"; const JAtom ms_ATOM_DEFAULT(ms_sDEFAULT);

struct ErrorMessage 
{
    char * errorname;
    char * message;
};

/* error messages */


const UnicodeString ms_sExportError =      "error: error writing to export file";
const UnicodeString ms_sNTimeParamError =      "error: ntime requires parameter";
const UnicodeString ms_sInvalidPromptValue =      "error: must be ON or OFF";
const UnicodeString ms_sSTimeParamError =      "error: stime requires parameter";
const UnicodeString ms_sISO8601ParamError =      "error: iso8601 requires a parameter";
const UnicodeString ms_sInvalidTimeStringError =      "error: cannot parse time/date string";
const UnicodeString ms_sTimeZoneParamError =      "error: timezone requires parameter";
const UnicodeString ms_sLocaleParamError =      "error: locale requires parameter";
const UnicodeString ms_sLocaleNotFoundError =      "error: locale not found";
const UnicodeString ms_sPatternParamError =      "error: param requires parameter";
const UnicodeString ms_sInvalidPatternError =      "error: bad format pattern cannot print";
const UnicodeString ms_sRRuleParamError =      "error: rrule requires parameter";
const UnicodeString ms_sERuleParamError =      "error: erule requires parameter";
const UnicodeString ms_sBoundParamError =      "error: bound requires parameter";
const UnicodeString ms_sInvalidRecurrenceError =      "error: bad recurrence cannot unzip";
const UnicodeString ms_sUnzipNullError =      "error: no recurrence defined";
const UnicodeString ms_sCommandNotFoundError =      "error: command not found";
const UnicodeString ms_sInvalidTimeZoneError =      "error: bad timezone";
const UnicodeString ms_sFileNotFound =      "error: file not found";
const UnicodeString ms_sInvalidPropertyName =      "error: invalid property name";
const UnicodeString ms_sInvalidPropertyValue =      "error: invalid property value";
const UnicodeString ms_sInvalidParameterName =      "error: invalid parameter name";
const UnicodeString ms_sInvalidParameterValue =      "error: invalid parameter value";
const UnicodeString ms_sMissingStartingTime =      "error: no starting time";
const UnicodeString ms_sMissingEndingTime =      "error: no ending time";
const UnicodeString ms_sEndBeforeStartTime =      "error: ending time occurs before starting time";
const UnicodeString ms_sMissingSeqNo =      "error:no sequence NO.";
const UnicodeString ms_sMissingReplySeq =      "error:no reply sequence NO.";
const UnicodeString ms_sMissingURL =      "error: no URL"; 
const UnicodeString ms_sMissingDTStamp =      "error: no DTStamp";
const UnicodeString ms_sMissingUID =      "error: no UID";
const UnicodeString ms_sMissingDescription =      "error: no Description";
const UnicodeString ms_sMissingMethodProvided =      "error: no method provided process as publish";
const UnicodeString ms_sInvalidVersionNumber =      "error: version number is not 2.0";
const UnicodeString ms_sUnknownUID =      "error: UID not related to any event in calendar, ask for resend";
const UnicodeString ms_sMissingUIDInReply =      "error: missing UID in reply, abort addReply";
const UnicodeString ms_sMissingValidDTStamp =      "error: missing valid DTStamp, abort addReply";
const UnicodeString ms_sDeclineCounterCalledByAttendee =      "error: an attendee cannot create an declinecounter message";
const UnicodeString ms_sPublishCalledByAttendee =      "error: an attendee cannot create an publish message";
const UnicodeString ms_sRequestCalledByAttendee =      "error: an attendee cannot create an request message";
const UnicodeString ms_sCancelCalledByAttendee =      "error: an attendee cannot create an cancel message";
const UnicodeString ms_sCounterCalledByOrganizer =      "error: an organizer cannot create an counter message";
const UnicodeString ms_sRefreshCalledByOrganizer =      "error: an organizer cannot create an resend message";
const UnicodeString ms_sReplyCalledByOrganizer =      "error: an organizer cannot create an reply message";
const UnicodeString ms_sAddReplySequenceOutOfRange =      "error: the sequence no. of this reply message is either too small or too big";
const UnicodeString ms_sDuplicatedProperty =      "error: this property already defined in this component, overriding old property";
const UnicodeString ms_sDuplicatedParameter =      "error: this parameter is already defined in this property, overriding old parameter value";
const UnicodeString ms_sConflictMethodAndStatus =      "error: the status of this message conflicts with the method type";
const UnicodeString ms_sConflictCancelAndConfirmedTentative =      "error: this cancel message does not have status CANCELLED";
const UnicodeString ms_sMissingUIDToMatchEvent =      "error: this reply, counter, resend, cancel message has no uid";
const UnicodeString ms_sInvalidNumberFormat =      "error: this string cannot be parsed into a number";
const UnicodeString ms_sDelegateRequestError =      "error: cannot create delegate request message";
const UnicodeString ms_sInvalidRecurrenceIDRange =      "error: recurrence-id range argument is invalid";
const UnicodeString ms_sUnknownRecurrenceID =      "error: recurrence-id not found in event list, need to keep all cancel messages referring to it";
const UnicodeString ms_sPropertyValueTypeMismatch = "error: the value type of this property does not match the VALUE parameter value";
const UnicodeString ms_sInvalidRRule =      "error: invalid rrule, ignoring rule";
const UnicodeString ms_sInvalidExRule =      "error: invalid exrule, ignoring exrule";
const UnicodeString ms_sInvalidRDate =      "error: invalid rdate, ignoring rdate";
const UnicodeString ms_sInvalidExDate =      "error: invalid rdate, ignoring exdate";
const UnicodeString ms_sInvalidEvent =      "error: invalid event, removing from event list";
const UnicodeString ms_sInvalidAlarm =      "error: invalid alarm, removing from alarm list";
const UnicodeString ms_sInvalidTZPart =      "error: invalid tzpart, not adding to tzpart list";
const UnicodeString ms_sInvalidAlarmCategory =      "error: invalid alarm category";
const UnicodeString ms_sInvalidAttendee =      "error: invalid attendee, not adding attendee to list";
const UnicodeString ms_sInvalidFreebusy =      "error: invalid freebusy, not adding freebusy to list";
const UnicodeString ms_sDurationAssertionFailed =      "error: weeks variable and other date variables are not 0, violating duration assertion";
const UnicodeString ms_sDurationParseFailed =      "error: duration parsing failed";
const UnicodeString ms_sPeriodParseFailed =      "error: period parsing failed";
const UnicodeString ms_sPeriodStartInvalid =      "error: period start date is before epoch";
const UnicodeString ms_sPeriodEndInvalid =      "error: period end date is before epoch";
const UnicodeString ms_sPeriodEndBeforeStart =      "error: period end date is before start date";
const UnicodeString ms_sPeriodDurationZero =      "error: period duration is zero length";
const UnicodeString ms_sFreebusyPeriodInvalid =      "error: freebusy period is invalid";
const UnicodeString ms_sOptParamInvalidPropertyValue =      "error: invalid property value for optional parameter";
const UnicodeString ms_sOptParamInvalidPropertyName =      "error: invalid optional parameter name";
const UnicodeString ms_sInvalidOptionalParam =      "error: bad optional parameters";
const UnicodeString ms_sAbruptEndOfParsing =      "error: terminated parsing of calendar component before reaching END:";
const UnicodeString ms_sLastModifiedBeforeCreated =      "error: last-modified time comes before created";
const UnicodeString ms_sMultipleOwners =      "error: more than one owner for this component";
const UnicodeString ms_sMultipleOrganizers =      "error: more than one organizer for this component";
const UnicodeString ms_sMissingOwner =      "error: no owner for this component";
const UnicodeString ms_sMissingDueTime =      "error: due time not set in VTodo component";
const UnicodeString ms_sCompletedPercentMismatch =      "error: completed not 100%, or not completed but 100% in VTodo";
const UnicodeString ms_sMissingFreqTagRecurrence =      "error: rule must contain FREQ tag";
const UnicodeString ms_sFreqIntervalMismatchRecurrence =      "error: frequency/interval inconsitencey";
const UnicodeString ms_sInvalidPercentCompleteValue =      "error: setting bad value to percent complete"; 
const UnicodeString ms_sInvalidPriorityValue =      "error: setting bad value to priority";
const UnicodeString ms_sInvalidByHourValue =      "error: Invalid BYHOUR entry";
const UnicodeString ms_sInvalidByMinuteValue =      "error: Invalid BYMINUTE entry";
const UnicodeString ms_sByDayFreqIntervalMismatch =      "error: frequency/interval inconsistency";
const UnicodeString ms_sInvalidByMonthDayValue =      "error: Invalid BYMONTHDAY entry";
const UnicodeString ms_sInvalidByYearDayValue =      "error: Invalid BYYEARDAY entry";
const UnicodeString ms_sInvalidBySetPosValue =      "error: Invalid BYSETPOS entry";
const UnicodeString ms_sInvalidByWeekNoValue =      "error: Invalid BYWEEKNO entry";
const UnicodeString ms_sInvalidWeekStartValue =      "error: Invalid week start value";
const UnicodeString ms_sInvalidByMonthValue =      "error: Invalid BYMONTH entry";
const UnicodeString ms_sInvalidByDayValue =      "error: Invalid BYDAY entry";
const UnicodeString ms_sInvalidFrequency =      "error: Invalid Frequency type";
const UnicodeString ms_sInvalidDayArg =      "error: Invalid Day arg";
const UnicodeString ms_sVerifyZeroError =      "error: read in a zero arg in verifyIntList";
static UnicodeString ms_sRS200 =      "2.00;Success.";
static UnicodeString ms_sRS201 =      "2.01;Success, but fallback taken on one or more property values.";
static UnicodeString ms_sRS202 =      "2.02;Success, invalid property ignored.";
static UnicodeString ms_sRS203 =      "2.03;Success, invalid property parameter ignored.";
static UnicodeString ms_sRS204 =      "2.04;Success, unknown non-standard property ignored.";
static UnicodeString ms_sRS205 =      "2.05;Success, unknown non-standard property value ignored.";
static UnicodeString ms_sRS206 =      "2.06;Success, invalid calendar component ignored.";
static UnicodeString ms_sRS207 =      "2.07;Success, request forwarded to calendar user.";
static UnicodeString ms_sRS208 =      "2.08;Success, repeating event ignored. Scheduled as a single event.";
static UnicodeString ms_sRS209 =      "2.09;Success, turncated end date/time to date boundary.";
static UnicodeString ms_sRS210 =      "2.10;Success, repeating to-do ignored. Scehduled as a single to-do";
static UnicodeString ms_sRS300 =      "3.00;Invalid property name.";
static UnicodeString ms_sRS301 =      "3.01;Invalid property value.";
static UnicodeString ms_sRS302 =      "3.02;Invalid property parameter.";
static UnicodeString ms_sRS303 =      "3.03;Invalid property parameter value.";
static UnicodeString ms_sRS304 =      "3.04;Invalid calendar component sequence.";
static UnicodeString ms_sRS305 =      "3.05;Invalid date or time.";
static UnicodeString ms_sRS306 =      "3.06;Invalid rule.";
static UnicodeString ms_sRS307 =      "3.07;Invalid calendar user.";
static UnicodeString ms_sRS308 =      "3.08;No authority.";
static UnicodeString ms_sRS309 =      "3.09;Unsupported Version.";
static UnicodeString ms_sRS310 =      "3.10;Request entry too large";
static UnicodeString ms_sRS311 =      "3.11;Missing required property";
static UnicodeString ms_sRS312 =      "3.12;Invalid calendar component;validity failure";
static UnicodeString ms_sRS400 =      "4.00;Event conflict.  Date/time is busy.";
static UnicodeString ms_sRS500 =      "5.00;Request not supported.";
static UnicodeString ms_sRS501 =      "5.01;Service unavailable.";
static UnicodeString ms_sRS502 =      "5.02;Invalid calendar service.";
static UnicodeString ms_sRS503 =      "5.03;No-scheduling support for user.";
const UnicodeString ms_sMissingUIDGenerateDefault =      "default: missing UID, generating new UID";
const UnicodeString ms_sMissingStartingTimeGenerateDefault =      "default: missing start time, generating default start time";
const UnicodeString ms_sMissingEndingTimeGenerateDefault =      "default: missing UID, generating default end time";
const UnicodeString ms_sNegativeSequenceNumberGenerateDefault =      "default: negative or no sequence number, generating default seqence number";
const UnicodeString ms_sDefaultTBEDescription =      "default: setting default description property in TimeBasedEvent to \"\"";
const UnicodeString ms_sDefaultTBEClass =      "default: setting default class property in TimeBasedEvent to \"\"";
const UnicodeString ms_sDefaultTBEStatus =      "default: setting default status property in TimeBasedEvent \"\"";
const UnicodeString ms_sDefaultTBETransp =      "default: setting default transp property in TimeBasedEvent to OPAQUE";
const UnicodeString ms_sDefaultTBERequestStatus =      "default: setting default request status property in TimeBasedEvent  2.00;Success";
const UnicodeString ms_sDefaultTBESummary =      "default: setting default summary property in TimeBasedEvent to DESCRIPTION value";
const UnicodeString ms_sDefaultRecIDRange =      "default: setting default range property in Recurrence-ID to \"\"";
const UnicodeString ms_sDefaultAttendeeRole =      "default: setting default role property in Attendee to ATTENDEE";
const UnicodeString ms_sDefaultAttendeeType =      "default: setting default type property in Attendee to UNKNOWN";
const UnicodeString ms_sDefaultAttendeeExpect =      "default: setting default expect property in Attendee to FYI";
const UnicodeString ms_sDefaultAttendeeStatus =      "default: setting default status property in Attendee to NEEDS-ACTION";
const UnicodeString ms_sDefaultAttendeeRSVP =      "default: setting default RSVP property in Attendee to FALSE";
const UnicodeString ms_sDefaultAlarmRepeat =      "default: setting default repeat property in Alarm to 1";
const UnicodeString ms_sDefaultAlarmDuration =      "default: setting default duration property in Alarm to 15 minutes";
const UnicodeString ms_sDefaultAlarmCategories =      "default: setting default categories property in Alarm to DISPLAY,AUDIO";
const UnicodeString ms_sDefaultFreebusyStatus =      "default: setting default status property in Freebusy to BUSY";
const UnicodeString ms_sDefaultFreebusyType =      "default: setting default type property in Freebusy to BUSY";
const UnicodeString ms_sDefaultDuration =      "default: setting duration to zero-length duration (PT0H)";

#endif /* #if 0 */

/*----------------------------------------------------------
** Key letters used for formatting strings 
**----------------------------------------------------------*/  
const t_int32 ms_cAlarms          = 'w';
const t_int32 ms_cAttach          = 'a';
const t_int32 ms_cAttendees       = 'v';
const t_int32 ms_cCategories      = 'k';
const t_int32 ms_cClass           = 'c';
const t_int32 ms_cComment         = 'K';
const t_int32 ms_cCompleted       = 'G';
const t_int32 ms_cContact         = 'H';
const t_int32 ms_cCreated         = 't';
const t_int32 ms_cDescription     = 'i';
const t_int32 ms_cDuration        = 'D';
const t_int32 ms_cDTEnd           = 'e';
const t_int32 ms_cDTStart         = 'B';
const t_int32 ms_cDTStamp         = 'C';
const t_int32 ms_cDue             = 'F';
const t_int32 ms_cExDate          = 'X';
const t_int32 ms_cExRule          = 'E';
const t_int32 ms_cGEO             = 'O';
const t_int32 ms_cLastModified    = 'M';
const t_int32 ms_cLocation        = 'L';
const t_int32 ms_cOrganizer       = 'J';
const t_int32 ms_cPercentComplete = 'P';
const t_int32 ms_cPriority        = 'p';
const t_int32 ms_cRDate           = 'x';
const t_int32 ms_cRRule           = 'y';
const t_int32 ms_cRecurrenceID    = 'R';
const t_int32 ms_cRelatedTo       = 'o';
const t_int32 ms_cRepeat          = 'A';
const t_int32 ms_cRequestStatus   = 'T';
const t_int32 ms_cResources       = 'r';
/*const t_int32 ms_cResponseSequence  = 'q';*/
const t_int32 ms_cSequence        = 's';
const t_int32 ms_cStatus          = 'g';
const t_int32 ms_cSummary         = 'S';
const t_int32 ms_cTransp          = 'h';
const t_int32 ms_cUID             = 'U';
const t_int32 ms_cURL             = 'u';
const t_int32 ms_cXTokens         = 'Z';

const t_int32 ms_cTZOffsetTo      = 'd';
const t_int32 ms_cTZOffsetFrom    = 'f';
const t_int32 ms_cTZName          = 'n';
const t_int32 ms_cTZURL           = 'Q';
const t_int32 ms_cTZID            = 'I';
const t_int32 ms_cTZParts         = 'V';
/*const t_int32 ms_cDAYLIGHT      = 'd';*/

const t_int32 ms_cFreebusy        = 'Y';

/* ATTENDEE ONLY (see attendee.h) */
/*
const t_int32 ms_cAttendeeName            = 'N';
const t_int32 ms_cAttendeeRole            = 'R';
const t_int32 ms_cAttendeeStatus        = 'S';  repeat 
const t_int32 ms_cAttendeeRSVP            = 'V';
const t_int32 ms_cAttendeeType            = 'T';
const t_int32 ms_cAttendeeExpect          = 'E';
const t_int32 ms_cAttendeeDelegatedTo     = 'D';
const t_int32 ms_cAttendeeDelegatedFrom   = 'd';
const t_int32 ms_cAttendeeMember          = 'M';
const t_int32 ms_cAttendeeDir             = 'l'; 'el' 
const t_int32 ms_cAttendeeSentBy          = 's';
const t_int32 ms_cAttendeeCN              = 'C';
const t_int32 ms_cAttendeeDisplayName     = 'z';
*/

/* FREEBUSY ONLY (see freebusy.h) */
/*
const t_int32 ms_cFreebusyType            = 'T';
const t_int32 ms_cFreebusyStatus          = 'S';
const t_int32 ms_cFreebusyPeriod          = 'P';
*/

const t_int32 ms_iMAX_LINE_LENGTH = 76;


#endif /* __KEYWORD_H_ */



