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
// keyword.cpp
// John Sun
// 3/19/98 9:49:32 AM

#include "stdafx.h"
#include "jdefines.h"

#include "keyword.h"
#include "jatom.h"

//---------------------------------------------------------------------

JulianKeyword * JulianKeyword::m_Instance = 0;

//---------------------------------------------------------------------

JulianKeyword::JulianKeyword()
{
    ms_sVCALENDAR  = "VCALENDAR";    
    ms_ATOM_VCALENDAR.setString(ms_sVCALENDAR);
    
    ms_sVEVENT     = "VEVENT";       
    ms_ATOM_VEVENT.setString(ms_sVEVENT); 
    
    ms_sVTODO      = "VTODO";        
    ms_ATOM_VTODO.setString(ms_sVTODO);
    
    ms_sVJOURNAL   = "VJOURNAL";     
    ms_ATOM_VJOURNAL.setString(ms_sVJOURNAL);
    
    ms_sVFREEBUSY  = "VFREEBUSY";    
    ms_ATOM_VFREEBUSY.setString(ms_sVFREEBUSY);
    
    ms_sVTIMEZONE  = "VTIMEZONE";    
    ms_ATOM_VTIMEZONE.setString(ms_sVTIMEZONE);
    
    ms_sVALARM     = "VALARM";       
    ms_ATOM_VALARM.setString(ms_sVALARM);
    
    ms_sTZPART     = "TZPART";
    ms_ATOM_TZPART.setString(ms_sTZPART);

    ms_sTIMEBASEDEVENT = "TimeBasedEvent";
    ms_sNSCALENDAR = "NSCalendar";

    ms_sATTENDEE         = "ATTENDEE";       
    ms_ATOM_ATTENDEE.setString(ms_sATTENDEE);
    
    ms_sATTACH           = "ATTACH";         
    ms_ATOM_ATTACH.setString(ms_sATTACH);
    
    ms_sCATEGORIES       = "CATEGORIES";     
    ms_ATOM_CATEGORIES.setString(ms_sCATEGORIES);
    
    ms_sCLASS            = "CLASS";          
    ms_ATOM_CLASS.setString(ms_sCLASS);
    
    ms_sCOMMENT          = "COMMENT";        
    ms_ATOM_COMMENT.setString(ms_sCOMMENT);
    
    ms_sCOMPLETED        = "COMPLETED";
    ms_ATOM_COMPLETED.setString(ms_sCOMPLETED);
    
    ms_sCONTACT          = "CONTACT";
    ms_ATOM_CONTACT.setString(ms_sCONTACT);
    ms_sCREATED          = "CREATED";
    ms_ATOM_CREATED.setString(ms_sCREATED);
    ms_sDTEND            = "DTEND";
    ms_ATOM_DTEND.setString(ms_sDTEND);

    ms_sDTSTART          = "DTSTART";
    ms_ATOM_DTSTART.setString(ms_sDTSTART);
    ms_sDTSTAMP          = "DTSTAMP";
    ms_ATOM_DTSTAMP.setString(ms_sDTSTAMP);
    ms_sDESCRIPTION      = "DESCRIPTION";
    ms_ATOM_DESCRIPTION.setString(ms_sDESCRIPTION);
    ms_sDUE              = "DUE";
    ms_ATOM_DUE.setString(ms_sDUE);
    ms_sDURATION         = "DURATION";
    ms_ATOM_DURATION.setString(ms_sDURATION);
    ms_sEXDATE           = "EXDATE";
    ms_ATOM_EXDATE.setString(ms_sEXDATE);
    ms_sEXRULE           = "EXRULE";
    ms_ATOM_EXRULE.setString(ms_sEXRULE);
    ms_sFREEBUSY         = "FREEBUSY";
    ms_ATOM_FREEBUSY.setString(ms_sFREEBUSY);
    ms_sGEO              = "GEO";
    ms_ATOM_GEO.setString(ms_sGEO);
    ms_sLASTMODIFIED     = "LAST-MODIFIED";
    ms_ATOM_LASTMODIFIED.setString(ms_sLASTMODIFIED);
    ms_sLOCATION         = "LOCATION";
    ms_ATOM_LOCATION.setString(ms_sLOCATION);
    ms_sORGANIZER        = "ORGANIZER";
    ms_ATOM_ORGANIZER.setString(ms_sORGANIZER);
    ms_sPERCENTCOMPLETE  = "PERCENT-COMPLETE";
    ms_ATOM_PERCENTCOMPLETE.setString(ms_sPERCENTCOMPLETE);
    ms_sPRIORITY         = "PRIORITY";
    ms_ATOM_PRIORITY.setString(ms_sPRIORITY);
    ms_sRDATE            = "RDATE";
    ms_ATOM_RDATE.setString(ms_sRDATE);
    ms_sRRULE            = "RRULE";
    ms_ATOM_RRULE.setString(ms_sRRULE);
    ms_sRECURRENCEID     = "RECURRENCE-ID";
    ms_ATOM_RECURRENCEID.setString(ms_sRECURRENCEID);
    //ms_sRESPONSESEQUENCE = "RESPONSE-SEQUENCE";
    //ms_ATOM_RESPONSESEQUENCE.setString(ms_sRESPONSESEQUENCE);
    ms_sRELATEDTO        = "RELATED-TO";
    ms_ATOM_RELATEDTO.setString(ms_sRELATEDTO);
    ms_sREPEAT           = "REPEAT";
    ms_ATOM_REPEAT.setString(ms_sREPEAT);
    ms_sREQUESTSTATUS    = "REQUEST-STATUS";
    ms_ATOM_REQUESTSTATUS.setString(ms_sREQUESTSTATUS);
    ms_sRESOURCES        = "RESOURCES";
    ms_ATOM_RESOURCES.setString(ms_sRESOURCES);
    ms_sSEQUENCE         = "SEQUENCE";
    ms_ATOM_SEQUENCE.setString(ms_sSEQUENCE);
    ms_sSTATUS           = "STATUS";
    ms_ATOM_STATUS.setString(ms_sSTATUS);
    ms_sSUMMARY          = "SUMMARY";
    ms_ATOM_SUMMARY.setString(ms_sSUMMARY);
    ms_sTRANSP           = "TRANSP";
    ms_ATOM_TRANSP.setString(ms_sTRANSP);
    ms_sTRIGGER          = "TRIGGER";
    ms_ATOM_TRIGGER.setString(ms_sTRIGGER);
    ms_sUID              = "UID";
    ms_ATOM_UID.setString(ms_sUID);
    ms_sURL              = "URL";
    ms_ATOM_URL.setString(ms_sURL);
    ms_sTZOFFSET         = "TZOFFSET";
    ms_ATOM_TZOFFSET.setString(ms_sTZOFFSET);
    ms_sTZOFFSETTO       = "TZOFFSETTO";
    ms_ATOM_TZOFFSETTO.setString(ms_sTZOFFSETTO);
    ms_sTZOFFSETFROM     = "TZOFFSETFROM";
    ms_ATOM_TZOFFSETFROM.setString(ms_sTZOFFSETFROM);
    ms_sTZNAME           = "TZNAME";
    ms_ATOM_TZNAME.setString(ms_sTZNAME);
    ms_sDAYLIGHT         = "DAYLIGHT";
    ms_ATOM_DAYLIGHT.setString(ms_sDAYLIGHT);
    ms_sSTANDARD         = "STANDARD";
    ms_ATOM_STANDARD.setString(ms_sSTANDARD);
    ms_sTZURL            = "TZURL";
    ms_ATOM_TZURL.setString(ms_sTZURL);
    ms_sTZID             = "TZID";
    ms_ATOM_TZID.setString(ms_sTZID);

    ms_sTRUE      = "TRUE";
    ms_ATOM_TRUE.setString(ms_sTRUE);
    ms_sFALSE     = "FALSE";
    ms_ATOM_FALSE.setString(ms_sFALSE);

    ms_sPUBLISH        = "PUBLISH";
    ms_ATOM_PUBLISH.setString(ms_sPUBLISH);
    ms_sREQUEST        = "REQUEST";
    ms_ATOM_REQUEST.setString(ms_sREQUEST);
    ms_sREPLY          = "REPLY";
    ms_ATOM_REPLY.setString(ms_sREPLY);
    ms_sCANCEL         = "CANCEL";
    ms_ATOM_CANCEL.setString(ms_sCANCEL);
    //ms_sRESEND         = "RESEND";
    //ms_ATOM_RESEND.setString(ms_sRESEND);
    ms_sREFRESH        = "REFRESH";
    ms_ATOM_REFRESH.setString(ms_sREFRESH);
    ms_sCOUNTER        = "COUNTER";
    ms_ATOM_COUNTER.setString(ms_sCOUNTER);
    ms_sDECLINECOUNTER = "DECLINECOUNTER";
    ms_ATOM_DECLINECOUNTER.setString(ms_sDECLINECOUNTER);
    ms_sADD            = "ADD";
    ms_ATOM_ADD.setString(ms_sADD);
    ms_sDELEGATEREQUEST= "DELEGATEREQUEST";
    ms_ATOM_DELEGATEREQUEST.setString(ms_sDELEGATEREQUEST);
    ms_sDELEGATEREPLY  = "DELEGATEREPLY";
    ms_ATOM_DELEGATEREPLY.setString(ms_sDELEGATEREPLY);
    //ms_sBUSYREQUEST   = "BUSY-REQUEST";
    //ms_ATOM_BUSYREQUEST.setString(ms_sBUSYREQUEST);
    //ms_sBUSYREPLY     = "BUSY-REPLY";
    //ms_ATOM_BUSYREPLY.setString(ms_sBUSYREPLY);

    ms_sPRODID   = "PRODID";
    ms_ATOM_PRODID.setString(ms_sPRODID);
    ms_sVERSION  = "VERSION";
    ms_ATOM_VERSION.setString(ms_sVERSION);
    ms_sMETHOD   = "METHOD";
    ms_ATOM_METHOD.setString(ms_sMETHOD);
    ms_sSOURCE   = "SOURCE";
    ms_ATOM_SOURCE.setString(ms_sSOURCE);
    ms_sCALSCALE = "CALSCALE";
    ms_ATOM_CALSCALE.setString(ms_sCALSCALE);
    ms_sNAME     = "NAME";
    ms_ATOM_NAME.setString(ms_sNAME);
    //ms_sPROFILE_VERSION = "PROFILE-VERSION";
    //ms_ATOM_PROFILE_VERSION.setString(ms_sPROFILE_VE

    ms_sGREGORIAN = "GREGORIAN";
    ms_ATOM_GREGORIAN.setString(ms_sGREGORIAN);

    ms_sPUBLIC        = "PUBLIC";
    ms_ATOM_PUBLIC.setString(ms_sPUBLIC);
    ms_sPRIVATE       = "PRIVATE";
    ms_ATOM_PRIVATE.setString(ms_sPRIVATE);
    ms_sCONFIDENTIAL  = "CONFIDENTIAL";
    ms_ATOM_CONFIDENTIAL.setString(ms_sCONFIDENTIAL);

    ms_sNEEDSACTION   = "NEEDS-ACTION";
    ms_ATOM_NEEDSACTION.setString(ms_sNEEDSACTION);
    //ms_sCOMPLETED     = "COMPLETED";
    //ms_ATOM_COMPLETED.setString(ms_sCOMPLETED);
    ms_sINPROCESS     = "IN-PROCESS";
    ms_ATOM_INPROCESS.setString(ms_sINPROCESS);
    ms_sTENTATIVE     = "TENTATIVE";
    ms_ATOM_TENTATIVE.setString(ms_sTENTATIVE);
    ms_sCONFIRMED     = "CONFIRMED";
    ms_ATOM_CONFIRMED.setString(ms_sCONFIRMED);
    ms_sCANCELLED     = "CANCELLED";
    ms_ATOM_CANCELLED.setString(ms_sCANCELLED);

    ms_sOPAQUE        = "OPAQUE";
    ms_ATOM_OPAQUE.setString(ms_sOPAQUE);
    ms_sTRANSPARENT   = "TRANSPARENT";
    ms_ATOM_TRANSPARENT.setString(ms_sTRANSPARENT);

    ms_sROLE             = "ROLE";
    ms_ATOM_ROLE.setString(ms_sROLE);
    ms_sCUTYPE           = "CUTYPE";
    ms_ATOM_CUTYPE.setString(ms_sCUTYPE);
    ms_sTYPE             = "TYPE";
    ms_ATOM_TYPE.setString(ms_sTYPE);
    ms_sCN               = "CN";
    ms_ATOM_CN.setString(ms_sCN);
    ms_sPARTSTAT         = "PARTSTAT";
    ms_ATOM_PARTSTAT.setString(ms_sPARTSTAT);
    ms_sRSVP             = "RSVP";
    ms_ATOM_RSVP.setString(ms_sRSVP);
    ms_sEXPECT           = "EXPECT";
    ms_ATOM_EXPECT.setString(ms_sEXPECT);
    ms_sMEMBER           = "MEMBER";
    ms_ATOM_MEMBER.setString(ms_sMEMBER);
    ms_sDELEGATED_TO     = "DELEGATED-TO";
    ms_ATOM_DELEGATED_TO.setString(ms_sDELEGATED_TO);
    ms_sDELEGATED_FROM   = "DELEGATED-FROM";
    ms_ATOM_DELEGATED_FROM.setString(ms_sDELEGATED_FROM);
    ms_sSENTBY           = "SENT-BY";
    ms_ATOM_SENTBY.setString(ms_sSENTBY);
    ms_sDIR              = "DIR";
    ms_ATOM_DIR.setString(ms_sDIR);

    //ms_sATTENDEE    = "ATTENDEE";
    //ms_ATOM_ATTENDEE.setString(ms_sATTENDEE);
    //ms_sORGANIZER   = "ORGANIZER";
    //ms_ATOM_ORGANIZER.setString(ms_sORGANIZER);
    //ms_sOWNER       = "OWNER";
    //ms_ATOM_OWNER.setString(ms_sOWNER);
    //ms_sDELEGATE    = "DELEGATE";
    //ms_ATOM_DELEGATE.setString(ms_sDELEGATE);
    ms_sCHAIR         =   "CHAIR";
    ms_ATOM_CHAIR.setString(ms_sCHAIR);
    //ms_sPARTICIPANT   =   "PARTICIPANT";
    //ms_ATOM_PARTICIPANT.setString(ms_sPARTICIPANT);
    ms_sREQ_PARTICIPANT = "REQ-PARTICIPANT";
    ms_ATOM_REQ_PARTICIPANT.setString(ms_sREQ_PARTICIPANT);
    ms_sOPT_PARTICIPANT = "OPT-PARTICIPANT";
    ms_ATOM_OPT_PARTICIPANT.setString(ms_sOPT_PARTICIPANT);
    ms_sNON_PARTICIPANT = "NON-PARTICIPANT";
    ms_ATOM_NON_PARTICIPANT.setString(ms_sNON_PARTICIPANT);


    ms_sUNKNOWN     = "UNKNOWN";
    ms_ATOM_UNKNOWN.setString(ms_sUNKNOWN);
    ms_sINDIVIDUAL  = "INDIVIDUAL";
    ms_ATOM_INDIVIDUAL.setString(ms_sINDIVIDUAL);
    ms_sGROUP       = "GROUP";
    ms_ATOM_GROUP.setString(ms_sGROUP);
    ms_sRESOURCE    = "RESOURCE";
    ms_ATOM_RESOURCE.setString(ms_sRESOURCE);
    ms_sROOM        = "ROOM";
    ms_ATOM_ROOM.setString(ms_sROOM);


    //ms_sNEEDSACTION = "NEEDS-ACTION";
    //ms_ATOM_NEEDSACTION.setString(ms_sNEEDSACTION);
    ms_sACCEPTED    = "ACCEPTED";
    ms_ATOM_ACCEPTED.setString(ms_sACCEPTED);
    ms_sDECLINED    = "DECLINED";
    ms_ATOM_DECLINED.setString(ms_sDECLINED);
    //ms_sTENTATIVE   = "TENTATIVE";
    //ms_ATOM_TENTATIVE.setString(ms_sTENTATIVE);
    //ms_sCONFIRMED   = "CONFIRMED";
    //ms_ATOM_CONFIRMED.setString(ms_sCONFIRMED);
    //ms_sCOMPLETED   = "COMPLETED";
    //ms_ATOM_COMPLETED.setString(ms_sCOMPLETED);
    ms_sDELEGATED   = "DELEGATED";
    ms_ATOM_DELEGATED.setString(ms_sDELEGATED);
    //ms_sCANCELLED   = "CANCELLED";
    //ms_ATOM_CANCELLED.setString(ms_sCANCELLED);
    ms_sVCALNEEDSACTION = "NEEDS ACTION";
    ms_ATOM_VCALNEEDSACTION.setString(ms_sVCALNEEDSACTION);

    //ms_sFALSE       = "FALSE";
    //ms_ATOM_FALSE.setString(ms_sFALSE);
    //ms_sTRUE        = "TRUE";
    //ms_ATOM_TRUE.setString(ms_sTRUE);

    ms_sFYI         = "FYI";
    ms_ATOM_FYI.setString(ms_sFYI);
    ms_sREQUIRE     = "REQUIRE";
    ms_ATOM_REQUIRE.setString(ms_sREQUIRE);
    //ms_sREQUEST     = "REQUEST";
    //ms_ATOM_REQUEST.setString(ms_sREQUEST);
    ms_sIMMEDIATE   = "IMMEDIATE";
    ms_ATOM_IMMEDIATE.setString(ms_sIMMEDIATE);

    ms_sRANGE          = "RANGE";
    ms_ATOM_RANGE.setString(ms_sRANGE);
    ms_sTHISANDPRIOR   = "THISANDPRIOR";
    ms_ATOM_THISANDPRIOR.setString(ms_sTHISANDPRIOR);
    ms_sTHISANDFUTURE  = "THISANDFUTURE";
    ms_ATOM_THISANDFUTURE.setString(ms_sTHISANDFUTURE);

    ms_sFBTYPE      = "FBTYPE";
    ms_ATOM_FBTYPE.setString(ms_sFBTYPE);
    ms_sBSTAT    = "BSTAT";
    ms_ATOM_BSTAT.setString(ms_sBSTAT);
    ms_sBUSY = "BUSY";
    ms_ATOM_BUSY.setString(ms_sBUSY);
    ms_sFREE = "FREE";
    ms_ATOM_FREE.setString(ms_sFREE);
    ms_sBUSY_UNAVAILABLE = "BUSY-UNAVAILABLE";
    ms_ATOM_BUSY_UNAVAILABLE.setString(ms_sBUSY_UNAVAILABLE);
    ms_sBUSY_TENTATIVE = "BUSY-TENTATIVE";
    ms_ATOM_BUSY_TENTATIVE.setString(ms_sBUSY_TENTATIVE);
    ms_sUNAVAILABLE = "UNAVAILABLE";
    ms_ATOM_UNAVAILABLE.setString(ms_sUNAVAILABLE);
    //ms_sTENTATIVE = "TENTATIVE";
    //ms_ATOM_TENTATIVE.setString(ms_sTENTATIVE);

    ms_sAUDIO = "AUDIO";
    ms_ATOM_AUDIO.setString(ms_sAUDIO);
    ms_sDISPLAY = "DISPLAY";
    ms_ATOM_DISPLAY.setString(ms_sDISPLAY);
    ms_sEMAIL = "EMAIL";
    ms_ATOM_EMAIL.setString(ms_sEMAIL);
    ms_sPROCEDURE = "PROCEDURE";
    ms_ATOM_PROCEDURE.setString(ms_sPROCEDURE);

    ms_sENCODING = "ENCODING";
    ms_ATOM_ENCODING.setString(ms_sENCODING);
    ms_sCHARSET = "CHARSET";
    ms_ATOM_CHARSET.setString(ms_sCHARSET);
    ms_sQUOTED_PRINTABLE = "QUOTED-PRINTABLE";
    ms_ATOM_QUOTED_PRINTABLE.setString(ms_sQUOTED_PRINTABLE);

    //ms_sENCODING  = "ENCODING";
    //ms_ATOM_ENCODING.setString(ms_sENCODING);
    ms_sVALUE     = "VALUE";
    ms_ATOM_VALUE.setString(ms_sVALUE);
    ms_sLANGUAGE  = "LANGUAGE";
    ms_ATOM_LANGUAGE.setString(ms_sLANGUAGE);
    ms_sALTREP    = "ALTREP";
    ms_ATOM_ALTREP.setString(ms_sALTREP);
    //ms_sSENTBY    = "SENT-BY";
    //ms_ATOM_SENTBY.setString(ms_sSENTBY);
    ms_sRELTYPE    = "RELTYPE";
    ms_ATOM_RELTYPE.setString(ms_sRELTYPE);
    ms_sRELATED    = "RELATED";
    ms_ATOM_RELATED.setString(ms_sRELATED);
    //ms_sTZID             = "TZID";
    //ms_ATOM_TZID.setString(ms_sTZID);


    ms_sPARENT    = "PARENT";
    ms_ATOM_PARENT.setString(ms_sPARENT);
    ms_sCHILD    = "CHILD";
    ms_ATOM_CHILD.setString(ms_sCHILD);
    ms_sSIBLING    = "SIBLING";
    ms_ATOM_SIBLING.setString(ms_sSIBLING);

    ms_sSTART    = "START";
    ms_ATOM_START.setString(ms_sSTART);
    ms_sEND    = "END";
    ms_ATOM_END.setString(ms_sEND);


    ms_s8bit = "8bit";
    ms_ATOM_8bit.setString(ms_s8bit);
    ms_sBase64 = "base64";
    ms_ATOM_Base64.setString(ms_sBase64);
    ms_s7bit = "7bit";
    ms_ATOM_7bit.setString(ms_s7bit);
    ms_sQ    = "Q";
    ms_ATOM_Q.setString(ms_sQ);
    ms_sB    = "B";
    ms_ATOM_B.setString(ms_sB);


    ms_sURI = "URI";
    ms_ATOM_URI.setString(ms_sURI);
    ms_sTEXT = "TEXT";
    ms_ATOM_TEXT.setString(ms_sTEXT);
    ms_sBINARY = "BINARY";
    ms_ATOM_BINARY.setString(ms_sBINARY);
    ms_sDATE = "DATE";
    ms_ATOM_DATE.setString(ms_sDATE);
    ms_sRECUR = "RECUR";
    ms_ATOM_RECUR.setString(ms_sRECUR);
    ms_sTIME = "TIME";
    ms_ATOM_TIME.setString(ms_sTIME);
    ms_sDATETIME = "DATE-TIME";
    ms_ATOM_DATETIME.setString(ms_sDATETIME);
    ms_sPERIOD = "PERIOD";
    ms_ATOM_PERIOD.setString(ms_sPERIOD);
    //ms_sDURATION = "DURATION";
    //ms_ATOM_DURATION.setString(ms_sDURATION);
    ms_sBOOLEAN = "BOOLEAN";
    ms_ATOM_BOOLEAN.setString(ms_sBOOLEAN);
    ms_sINTEGER = "INTEGER";
    ms_ATOM_INTEGER.setString(ms_sINTEGER);
    ms_sFLOAT = "FLOAT";
    ms_ATOM_FLOAT.setString(ms_sFLOAT);
    ms_sCALADDRESS = "CAL-ADDRESS";
    ms_ATOM_CALADDRESS.setString(ms_sCALADDRESS);
    ms_sUTCOFFSET = "UTC-OFFSET";
    ms_ATOM_UTCOFFSET.setString(ms_sUTCOFFSET);

    ms_sMAILTO_COLON = "MAILTO:";
    ms_sBEGIN = "BEGIN";
    ms_ATOM_BEGIN.setString(ms_sBEGIN);
    ms_sBEGIN_WITH_COLON = "BEGIN:";
    ms_ATOM_BEGIN_WITH_COLON.setString(ms_sBEGIN_WITH_COLON);
    //ms_sEND = "END";
    ms_ATOM_END.setString(ms_sEND);
    ms_sEND_WITH_COLON = "END:";
    ms_ATOM_END_WITH_COLON.setString(ms_sEND_WITH_COLON);
    ms_sBEGIN_VCALENDAR = "BEGIN:VCALENDAR";
    ms_ATOM_BEGIN_VCALENDAR.setString(ms_sBEGIN_VCALENDAR);
    ms_sEND_VCALENDAR= "END:VCALENDAR";
    ms_ATOM_END_VCALENDAR.setString(ms_sEND_VCALENDAR);
    ms_sBEGIN_VFREEBUSY = "BEGIN:VFREEBUSY";
    ms_ATOM_BEGIN_VFREEBUSY.setString(ms_sBEGIN_VFREEBUSY);
    ms_sEND_VFREEBUSY= "END:VFREEBUSY";
    ms_ATOM_END_VFREEBUSY.setString(ms_sEND_VFREEBUSY);
    ms_sLINEBREAK = "\r\n";
    ms_ATOM_LINEBREAK.setString(ms_sLINEBREAK);
    ms_sOK = "OK";
    ms_ATOM_OK.setString(ms_sOK);
    ms_sCOMMA_SYMBOL = ",";
    ms_ATOM_COMMA_SYMBOL.setString(ms_sCOMMA_SYMBOL);
    ms_sCOLON_SYMBOL = ":";
    ms_ATOM_COLON_SYMBOL.setString(ms_sCOLON_SYMBOL);
    ms_sSEMICOLON_SYMBOL = ";";
    ms_sDEFAULT_DELIMS = "\t\n\r";
    ms_sLINEBREAKSPACE = "\r\n ";
    ms_sALTREPQUOTE = ";ALTREP=\"";
    ms_sLINEFEEDSPACE = "\n ";

    ms_sRRULE_WITH_SEMICOLON = "RRULE;";

    ms_sUNTIL = "UNTIL";
    ms_ATOM_UNTIL.setString(ms_sUNTIL);
    ms_sCOUNT = "COUNT";
    ms_ATOM_COUNT.setString(ms_sCOUNT);
    ms_sINTERVAL = "INTERVAL";
    ms_ATOM_INTERVAL.setString(ms_sINTERVAL);
    ms_sFREQ = "FREQ";
    ms_ATOM_FREQ.setString(ms_sFREQ);
    ms_sBYSECOND = "BYSECOND";
    ms_ATOM_BYSECOND.setString(ms_sBYSECOND);
    ms_sBYMINUTE = "BYMINUTE";
    ms_ATOM_BYMINUTE.setString(ms_sBYMINUTE);
    ms_sBYHOUR = "BYHOUR";
    ms_ATOM_BYHOUR.setString(ms_sBYHOUR);
    ms_sBYDAY = "BYDAY";
    ms_ATOM_BYDAY.setString(ms_sBYDAY);
    ms_sBYMONTHDAY = "BYMONTHDAY";
    ms_ATOM_BYMONTHDAY.setString(ms_sBYMONTHDAY);
    ms_sBYYEARDAY = "BYYEARDAY";
    ms_ATOM_BYYEARDAY.setString(ms_sBYYEARDAY);
    ms_sBYWEEKNO = "BYWEEKNO";
    ms_ATOM_BYWEEKNO.setString(ms_sBYWEEKNO);
    ms_sBYMONTH = "BYMONTH";
    ms_ATOM_BYMONTH.setString(ms_sBYMONTH);
    ms_sBYSETPOS = "BYSETPOS";
    ms_ATOM_BYSETPOS.setString(ms_sBYSETPOS);
    ms_sWKST = "WKST";
    ms_ATOM_WKST.setString(ms_sWKST);

    ms_sSECONDLY = "SECONDLY";
    ms_ATOM_SECONDLY.setString(ms_sSECONDLY);
    ms_sMINUTELY = "MINUTELY";
    ms_ATOM_MINUTELY.setString(ms_sMINUTELY);
    ms_sHOURLY = "HOURLY";
    ms_ATOM_HOURLY.setString(ms_sHOURLY);
    ms_sDAILY = "DAILY";
    ms_ATOM_DAILY.setString(ms_sDAILY);
    ms_sWEEKLY = "WEEKLY";
    ms_ATOM_WEEKLY.setString(ms_sWEEKLY);
    ms_sMONTHLY = "MONTHLY";
    ms_ATOM_MONTHLY.setString(ms_sMONTHLY);
    ms_sYEARLY = "YEARLY";
    ms_ATOM_YEARLY.setString(ms_sYEARLY);

    ms_sSU = "SU";
    ms_ATOM_SU.setString(ms_sSU);
    ms_sMO = "MO";
    ms_ATOM_MO.setString(ms_sMO);
    ms_sTU = "TU";
    ms_ATOM_TU.setString(ms_sTU);
    ms_sWE = "WE";
    ms_ATOM_WE.setString(ms_sWE);
    ms_sTH = "TH";
    ms_ATOM_TH.setString(ms_sTH);
    ms_sFR = "FR";
    ms_ATOM_FR.setString(ms_sFR);
    ms_sSA = "SA";
    ms_ATOM_SA.setString(ms_sSA);

    ms_sBYDAYYEARLY = "BYDAYYEARLY";
    ms_ATOM_BYDAYYEARLY.setString(ms_sBYDAYYEARLY);
    ms_sBYDAYMONTHLY = "BYDAYMONTHLY";
    ms_ATOM_BYDAYMONTHLY.setString(ms_sBYDAYMONTHLY);
    ms_sBYDAYWEEKLY = "BYDAYWEEKLY";
    ms_ATOM_BYDAYWEEKLY.setString(ms_sBYDAYWEEKLY);
    ms_sDEFAULT = "DEFAULT";
    ms_ATOM_DEFAULT.setString(ms_sDEFAULT);

    ms_sCONTENT_TRANSFER_ENCODING = "Content-Transfer-Encoding";
}
   
//---------------------------------------------------------------------

JulianKeyword * JulianKeyword::Instance()
{
    if (m_Instance == 0)
        m_Instance = new JulianKeyword();
    PR_ASSERT(m_Instance != 0);
    return m_Instance;
}

//---------------------------------------------------------------------

JulianKeyword::~JulianKeyword()
{
    /*
    if (m_Instance != 0)
    {
        delete m_Instance; m_Instance = 0;
    }
    */
}

//---------------------------------------------------------------------
// JulianAtomRange
//---------------------------------------------------------------------

JulianAtomRange * JulianAtomRange::m_Instance = 0;

//---------------------------------------------------------------------

JulianAtomRange::~JulianAtomRange()
{
    /*
    if (m_Instance != 0)
    {
        delete m_Instance; m_Instance = 0;
    }
    */
}

//---------------------------------------------------------------------

JulianAtomRange *
JulianAtomRange::Instance()
{
    if (m_Instance == 0)
        m_Instance = new JulianAtomRange();
    PR_ASSERT(m_Instance != 0);
    return m_Instance;
}

//---------------------------------------------------------------------

JulianAtomRange::JulianAtomRange()
{
 // ATOM RANGES
    ms_asAltrepLanguageParamRange[0] = JulianKeyword::Instance()->ms_ATOM_ALTREP;
    ms_asAltrepLanguageParamRange[1] = JulianKeyword::Instance()->ms_ATOM_LANGUAGE;
    ms_asAltrepLanguageParamRangeSize = 2;

    ms_asTZIDValueParamRange[0] = JulianKeyword::Instance()->ms_ATOM_VALUE; 
    ms_asTZIDValueParamRange[1] = JulianKeyword::Instance()->ms_ATOM_TZID;
    ms_asTZIDValueParamRangeSize = 2;
    
    ms_asLanguageParamRange[0] = JulianKeyword::Instance()->ms_ATOM_LANGUAGE;
    ms_asLanguageParamRangeSize = 1;
    
    ms_asEncodingValueParamRange[0] = JulianKeyword::Instance()->ms_ATOM_ENCODING;
    ms_asEncodingValueParamRange[1] = JulianKeyword::Instance()->ms_ATOM_VALUE;
    ms_asEncodingValueParamRangeSize = 2;

    ms_asSentByParamRange[0] = JulianKeyword::Instance()->ms_ATOM_SENTBY;
    ms_asSentByParamRangeSize = 1;

    ms_asReltypeParamRange[0] = JulianKeyword::Instance()->ms_ATOM_RELTYPE;
    ms_asReltypeParamRangeSize = 1;

    ms_asRelatedValueParamRange[0] = JulianKeyword::Instance()->ms_ATOM_VALUE;
    ms_asRelatedValueParamRange[1] = JulianKeyword::Instance()->ms_ATOM_RELATED;
    ms_asRelatedValueParamRangeSize = 2;

    ms_asBinaryURIValueRange[0] = JulianKeyword::Instance()->ms_ATOM_BINARY;    
    ms_asBinaryURIValueRange[1] = JulianKeyword::Instance()->ms_ATOM_URI;
    ms_asBinaryURIValueRangeSize = 2;

    ms_asDateDateTimeValueRange[0] = JulianKeyword::Instance()->ms_ATOM_DATE;
    ms_asDateDateTimeValueRange[1] = JulianKeyword::Instance()->ms_ATOM_DATETIME;
    ms_asDateDateTimeValueRangeSize = 2;

    ms_asDurationDateTimeValueRange[0] = JulianKeyword::Instance()->ms_ATOM_DURATION;
    ms_asDurationDateTimeValueRange[1] = JulianKeyword::Instance()->ms_ATOM_DATETIME;
    ms_asDurationDateTimeValueRangeSize = 2;

    ms_asDateDateTimePeriodValueRange[0] = JulianKeyword::Instance()->ms_ATOM_DATE;
    ms_asDateDateTimePeriodValueRange[1] = JulianKeyword::Instance()->ms_ATOM_DATETIME;
    ms_asDateDateTimePeriodValueRange[2] = JulianKeyword::Instance()->ms_ATOM_PERIOD;
    ms_asDateDateTimePeriodValueRangeSize = 3;

    ms_asRelTypeRange[0] = JulianKeyword::Instance()->ms_ATOM_PARENT;
    ms_asRelTypeRange[1] = JulianKeyword::Instance()->ms_ATOM_CHILD;
    ms_asRelTypeRange[2] = JulianKeyword::Instance()->ms_ATOM_SIBLING;

    ms_asRelatedRange[0] = JulianKeyword::Instance()->ms_ATOM_START;
    ms_asRelatedRange[1] = JulianKeyword::Instance()->ms_ATOM_END;
    
    ms_asParameterRange[0] = JulianKeyword::Instance()->ms_ATOM_ALTREP; 
    ms_asParameterRange[1] = JulianKeyword::Instance()->ms_ATOM_ENCODING; 
    ms_asParameterRange[2] = JulianKeyword::Instance()->ms_ATOM_LANGUAGE; 
    ms_asParameterRange[3] = JulianKeyword::Instance()->ms_ATOM_TZID; 
    ms_asParameterRange[4] = JulianKeyword::Instance()->ms_ATOM_VALUE; 
    ms_iParameterRangeSize = 5;

    ms_asIrregularProperties[0] = JulianKeyword::Instance()->ms_ATOM_ATTENDEE; 
    ms_asIrregularProperties[1] = JulianKeyword::Instance()->ms_ATOM_FREEBUSY;
    ms_asIrregularProperties[2] = JulianKeyword::Instance()->ms_ATOM_RECURRENCEID;
    ms_iIrregularPropertiesSize = 3;
    
    ms_asValueRange[0] = JulianKeyword::Instance()->ms_ATOM_BINARY;
    ms_asValueRange[1] = JulianKeyword::Instance()->ms_ATOM_BOOLEAN;
    ms_asValueRange[2] = JulianKeyword::Instance()->ms_ATOM_CALADDRESS;
    ms_asValueRange[3] = JulianKeyword::Instance()->ms_ATOM_DATE;
    ms_asValueRange[4] = JulianKeyword::Instance()->ms_ATOM_DATETIME;
    ms_asValueRange[5] = JulianKeyword::Instance()->ms_ATOM_DURATION;
    ms_asValueRange[6] = JulianKeyword::Instance()->ms_ATOM_FLOAT;
    ms_asValueRange[7] = JulianKeyword::Instance()->ms_ATOM_INTEGER;
    ms_asValueRange[8] = JulianKeyword::Instance()->ms_ATOM_PERIOD;
    ms_asValueRange[9] = JulianKeyword::Instance()->ms_ATOM_RECUR;
    ms_asValueRange[10] = JulianKeyword::Instance()->ms_ATOM_TEXT;
    ms_asValueRange[11] = JulianKeyword::Instance()->ms_ATOM_TIME;
    ms_asValueRange[12] = JulianKeyword::Instance()->ms_ATOM_URI;
    ms_asValueRange[13] = JulianKeyword::Instance()->ms_ATOM_UTCOFFSET;
    ms_iValueRangeSize = 14;
    ms_asEncodingRange[0] = JulianKeyword::Instance()->ms_ATOM_8bit;
    ms_asEncodingRange[1] = JulianKeyword::Instance()->ms_ATOM_Base64;
    ms_iEncodingRangeSize = 2;
};

//---------------------------------------------------------------------
// JulianErrorMessage
//---------------------------------------------------------------------

//---------------------------------------------------------------------

JulianLogErrorMessage * JulianLogErrorMessage::m_Instance = 0;

//---------------------------------------------------------------------

JulianLogErrorMessage *
JulianLogErrorMessage::Instance()
{
    if (m_Instance == 0)
        m_Instance = new JulianLogErrorMessage();
    PR_ASSERT(m_Instance != 0);
    return m_Instance;
}

//---------------------------------------------------------------------

JulianLogErrorMessage::~JulianLogErrorMessage()
{
    /*
    if (m_Instance != 0)
    {
        delete m_Instance; m_Instance = 0;
    }
    */
}

//---------------------------------------------------------------------

JulianLogErrorMessage::JulianLogErrorMessage()
{
    
    ms_sDTEndBeforeDTStart = "error: DTEnd before DTStart.  Setting DTEnd equal to DTStart.";

    ms_sExportError = "error: error writing to export file";
    ms_sNTimeParamError = "error: ntime requires parameter";
    ms_sInvalidPromptValue = "error: must be ON or OFF";
    ms_sSTimeParamError = "error: stime requires parameter";
    ms_sISO8601ParamError = "error: iso8601 requires a parameter";
    ms_sInvalidTimeStringError = "error: cannot parse time/date string";
    ms_sTimeZoneParamError = "error: timezone requires parameter";
    ms_sLocaleParamError = "error: locale requires parameter";
    ms_sLocaleNotFoundError = "error: locale not found";
    ms_sPatternParamError = "error: param requires parameter";
    ms_sInvalidPatternError = "error: bad format pattern cannot print";
    ms_sRRuleParamError = "error: rrule requires parameter";
    ms_sERuleParamError = "error: erule requires parameter";
    ms_sBoundParamError = "error: bound requires parameter";
    //ms_sInvalidRecurrenceError = "error: bad recurrence cannot unzip";
    ms_sInvalidRecurrenceError = "Recurrence rules are too complicated.  Only the first instance was scheduled.";
    ms_sUnzipNullError = "error: no recurrence defined";
    ms_sCommandNotFoundError = "error: command not found";
    ms_sInvalidTimeZoneError = "error: bad timezone";
    ms_sFileNotFound = "error: file not found";
    ms_sInvalidPropertyName = "error: invalid property name";
    ms_sInvalidPropertyValue = "error: invalid property value";
    ms_sInvalidParameterName = "error: invalid parameter name";
    ms_sInvalidParameterValue = "error: invalid parameter value";
    ms_sMissingStartingTime = "error: no starting time";
    ms_sMissingEndingTime = "error: no ending time";
    ms_sEndBeforeStartTime = "error: ending time occurs before starting time";
    ms_sMissingSeqNo = "error:no sequence NO.";
    ms_sMissingReplySeq = "error:no reply sequence NO.";
    ms_sMissingURL = "error: no URL"; 
    ms_sMissingDTStamp = "error: no DTStamp";
    ms_sMissingUID = "error: no UID";
    ms_sMissingDescription = "error: no Description";
    ms_sMissingMethodProvided = "error: no method provided process as publish";
    ms_sInvalidVersionNumber = "error: version number is not 2.0";
    ms_sUnknownUID = "error: UID not related to any event in calendar, ask for resend";
    ms_sMissingUIDInReply = "error: missing UID in reply, abort addReply";
    ms_sMissingValidDTStamp = "error: missing valid DTStamp, abort addReply";
    ms_sDeclineCounterCalledByAttendee = "error: an attendee cannot create an declinecounter message";
    ms_sPublishCalledByAttendee = "error: an attendee cannot create an publish message";
    ms_sRequestCalledByAttendee = "error: an attendee cannot create an request message";
    ms_sCancelCalledByAttendee = "error: an attendee cannot create an cancel message";
    ms_sCounterCalledByOrganizer = "error: an organizer cannot create an counter message";
    ms_sRefreshCalledByOrganizer = "error: an organizer cannot create an resend message";
    ms_sReplyCalledByOrganizer = "error: an organizer cannot create an reply message";
    ms_sAddReplySequenceOutOfRange = "error: the sequence no. of this reply message is either too small or too big";
    ms_sDuplicatedProperty = "error: this property already defined in this component, overriding old property";
    ms_sDuplicatedParameter = "error: this parameter is already defined in this property, overriding old parameter value";
    ms_sConflictMethodAndStatus = "error: the status of this message conflicts with the method type";
    ms_sConflictCancelAndConfirmedTentative = "error: this cancel message does not have status CANCELLED";
    ms_sMissingUIDToMatchEvent = "error: this reply, counter, resend, cancel message has no uid";
    ms_sInvalidNumberFormat = "error: this string cannot be parsed into a number";
    ms_sDelegateRequestError = "error: cannot create delegate request message";
    ms_sInvalidRecurrenceIDRange = "error: recurrence-id range argument is invalid";
    ms_sUnknownRecurrenceID = "error: recurrence-id not found in event list, need to keep all cancel messages referring to it";
    ms_sPropertyValueTypeMismatch = "error: the value type of this property does not match the VALUE parameter value";
    ms_sInvalidRRule = "error: invalid rrule, ignoring rule";
    ms_sInvalidExRule = "error: invalid exrule, ignoring exrule";
    ms_sInvalidRDate = "error: invalid rdate, ignoring rdate";
    ms_sInvalidExDate = "error: invalid rdate, ignoring exdate";
    ms_sInvalidEvent = "error: invalid event, removing from event list";
    ms_sInvalidComponent = "error: invalid calendar component, removing from component list";
    ms_sInvalidAlarm = "error: invalid alarm, removing from alarm list";
    ms_sInvalidTZPart = "error: invalid tzpart, not adding to tzpart list";
    ms_sInvalidAlarmCategory = "error: invalid alarm category";
    ms_sInvalidAttendee = "error: invalid attendee, not adding attendee to list";
    ms_sInvalidFreebusy = "error: invalid freebusy, not adding freebusy to list";
    ms_sDurationAssertionFailed = "error: weeks variable and other date variables are not 0, violating duration assertion";
    ms_sDurationParseFailed = "error: duration parsing failed";
    ms_sPeriodParseFailed = "error: period parsing failed";
    ms_sPeriodStartInvalid = "error: period start date is before epoch";
    ms_sPeriodEndInvalid = "error: period end date is before epoch";
    ms_sPeriodEndBeforeStart = "error: period end date is before start date";
    ms_sPeriodDurationZero = "error: period duration is zero length";
    ms_sFreebusyPeriodInvalid = "error: freebusy period is invalid";
    ms_sOptParamInvalidPropertyValue = "error: invalid property value for optional parameter";
    ms_sOptParamInvalidPropertyName = "error: invalid optional parameter name";
    ms_sInvalidOptionalParam = "error: bad optional parameters";
    ms_sAbruptEndOfParsing = "error: terminated parsing of calendar component before reaching END:";
    ms_sLastModifiedBeforeCreated = "error: last-modified time comes before created";
    ms_sMultipleOwners = "error: more than one owner for this component";
    ms_sMultipleOrganizers = "error: more than one organizer for this component";
    ms_sMissingOwner = "error: no owner for this component";
    ms_sMissingDueTime = "error: due time not set in VTodo component";
    ms_sCompletedPercentMismatch = "error: completed not 100%, or not completed but 100% in VTodo";
    ms_sMissingFreqTagRecurrence = "error: rule must contain FREQ tag";
    ms_sFreqIntervalMismatchRecurrence = "error: frequency/interval inconsitencey";
    ms_sInvalidPercentCompleteValue = "error: setting bad value to percent complete"; 
    ms_sInvalidPriorityValue = "error: setting bad value to priority";
    ms_sInvalidByHourValue = "error: Invalid BYHOUR entry";
    ms_sInvalidByMinuteValue = "error: Invalid BYMINUTE entry";
    ms_sByDayFreqIntervalMismatch = "error: frequency/interval inconsistency";
    ms_sInvalidByMonthDayValue = "error: Invalid BYMONTHDAY entry";
    ms_sInvalidByYearDayValue = "error: Invalid BYYEARDAY entry";
    ms_sInvalidBySetPosValue = "error: Invalid BYSETPOS entry";
    ms_sInvalidByWeekNoValue = "error: Invalid BYWEEKNO entry";
    ms_sInvalidWeekStartValue = "error: Invalid week start value";
    ms_sInvalidByMonthValue = "error: Invalid BYMONTH entry";
    ms_sInvalidByDayValue = "error: Invalid BYDAY entry";
    ms_sInvalidFrequency = "error: Invalid Frequency type";
    ms_sInvalidDayArg = "error: Invalid Day arg";
    ms_sVerifyZeroError = "error: read in a zero arg in verifyIntList";
    ms_sRoundedPercentCompleteTo100 = "error: rounded percent complete value to 100";
    ms_sRS200 = "2.00;Success.";
    ms_sRS201 = "2.01;Success, but fallback taken on one or more property values.";
    ms_sRS202 = "2.02;Success, invalid property ignored.";
    ms_sRS203 = "2.03;Success, invalid property parameter ignored.";
    ms_sRS204 = "2.04;Success, unknown non-standard property ignored.";
    ms_sRS205 = "2.05;Success, unknown non-standard property value ignored.";
    ms_sRS206 = "2.06;Success, invalid calendar component ignored.";
    ms_sRS207 = "2.07;Success, request forwarded to calendar user.";
    ms_sRS208 = "2.08;Success, repeating event ignored. Scheduled as a single event.";
    ms_sRS209 = "2.09;Success, turncated end date/time to date boundary.";
    ms_sRS210 = "2.10;Success, repeating to-do ignored. Scehduled as a single to-do";
    ms_sRS300 = "3.00;Invalid property name.";
    ms_sRS301 = "3.01;Invalid property value.";
    ms_sRS302 = "3.02;Invalid property parameter.";
    ms_sRS303 = "3.03;Invalid property parameter value.";
    ms_sRS304 = "3.04;Invalid calendar component sequence.";
    ms_sRS305 = "3.05;Invalid date or time.";
    ms_sRS306 = "3.06;Invalid rule.";
    ms_sRS307 = "3.07;Invalid calendar user.";
    ms_sRS308 = "3.08;No authority.";
    ms_sRS309 = "3.09;Unsupported Version.";
    ms_sRS310 = "3.10;Request entry too large";
    ms_sRS311 = "3.11;Missing required property";
    ms_sRS312 = "3.12;Invalid calendar component;validity failure";
    ms_sRS400 = "4.00;Event conflict.  Date/time is busy.";
    ms_sRS500 = "5.00;Request not supported.";
    ms_sRS501 = "5.01;Service unavailable.";
    ms_sRS502 = "5.02;Invalid calendar service.";
    ms_sRS503 = "5.03;No-scheduling support for user.";
    ms_sMissingUIDGenerateDefault = "default: missing UID, generating new UID";
    ms_sMissingStartingTimeGenerateDefault = "default: missing start time, generating default start time";
    ms_sMissingEndingTimeGenerateDefault = "default: missing UID, generating default end time";
    ms_sNegativeSequenceNumberGenerateDefault = "default: negative or no sequence number, generating default seqence number";
    ms_sDefaultTBEDescription = "default: setting default description property in TimeBasedEvent to \"\"";
    ms_sDefaultTBEClass = "default: setting default class property in TimeBasedEvent to \"\"";
    ms_sDefaultTBEStatus = "default: setting default status property in TimeBasedEvent \"\"";
    ms_sDefaultTBETransp = "default: setting default transp property in TimeBasedEvent to OPAQUE";
    ms_sDefaultTBERequestStatus = "default: setting default request status property in TimeBasedEvent  2.00;Success";
    ms_sDefaultTBESummary = "default: setting default summary property in TimeBasedEvent to DESCRIPTION value";
    ms_sDefaultRecIDRange = "default: setting default range property in Recurrence-ID to \"\"";
    ms_sDefaultAttendeeRole = "default: setting default role property in Attendee to ATTENDEE";
    ms_sDefaultAttendeeType = "default: setting default type property in Attendee to UNKNOWN";
    ms_sDefaultAttendeeExpect = "default: setting default expect property in Attendee to FYI";
    ms_sDefaultAttendeeStatus = "default: setting default status property in Attendee to NEEDS-ACTION";
    ms_sDefaultAttendeeRSVP = "default: setting default RSVP property in Attendee to FALSE";
    ms_sDefaultAlarmRepeat = "default: setting default repeat property in Alarm to 1";
    ms_sDefaultAlarmDuration = "default: setting default duration property in Alarm to 15 minutes";
    ms_sDefaultAlarmCategories = "default: setting default categories property in Alarm to DISPLAY,AUDIO";
    ms_sDefaultFreebusyStatus = "default: setting default status property in Freebusy to BUSY";
    ms_sDefaultFreebusyType = "default: setting default type property in Freebusy to BUSY";
    ms_sDefaultDuration = "default: setting duration to zero-length duration (PT0H)";    
}

//---------------------------------------------------------------------

//---------------------------------------------------------------------
// JulianFormatString
//---------------------------------------------------------------------

JulianFormatString * JulianFormatString::m_Instance = 0;

//---------------------------------------------------------------------

JulianFormatString *
JulianFormatString::Instance()
{
    if (m_Instance == 0)
        m_Instance = new JulianFormatString();
    PR_ASSERT(m_Instance != 0);
    return m_Instance;
}

//---------------------------------------------------------------------

JulianFormatString::~JulianFormatString()
{
    /*
    if (m_Instance != 0)
    {
        delete m_Instance; m_Instance = 0;
    }
    */
}

//---------------------------------------------------------------------

JulianFormatString::JulianFormatString()
{
    ms_sDateTimeISO8601Pattern = "yyyyMMdd'T'HHmmss'Z'";
    ms_sDateTimeISO8601LocalPattern = "yyyyMMdd'T'HHmmss";
    ms_sDateTimeISO8601TimeOnlyPattern = "HHmmss";
    ms_sDateTimeISO8601DateOnlyPattern = "yyyyMMdd";
    ms_sDateTimeDefaultPattern = "MMM dd, yyyy h:mm a z";

    // added dir, sent-by, cn 3-23-98
    // added language 4-28-98
    ms_sAttendeeAllMessage = "%m%C%s%l%R%S%V%T%E%D%d%M%N";
    ms_sAttendeeDoneActionMessage = "%m%C%s%l%R%S%T%D%d%M%N";
    ms_sAttendeeDoneDelegateToOnly = "%m%C%s%l%R%S%T%D%M%N";
    ms_sAttendeeDoneDelegateFromOnly = "%m%C%s%l%R%S%T%d%M%N";
    ms_sAttendeeNeedsActionMessage = "%m%C%s%l%R%V%E%T%D%d%M%N";
    ms_sAttendeeNeedsActionDelegateToOnly = "%m%C%s%l%R%V%E%T%D%M%N";
    ms_sAttendeeNeedsActionDelegateFromOnly = "%m%C%s%l%R%V%E%T%d%M%N";

    ms_AttendeeStrDefaultFmt = "^N ^R ^z ^S";

    ms_OrganizerStrDefaultFmt = "^N ^z";

    ms_FreebusyStrDefaultFmt = "\tTYPE= ^T ^P\r\n";

    ms_TZPartStrDefaultFmt = "\r\n\t%(EEE MM/dd/yy hh:mm:ss z)B,%x%(EEE MM/dd/yy hh:mm:ss z)y, NAME: %n, FROM: %f, TO: %d";
    ms_sTZPartAllMessage = "%B%x%y%f%d%n%K";

    ms_VEventStrDefaultFmt = "[%(EEE MM/dd/yy hh:mm:ss a z)B - %(EEE MM/dd/yy hh:mm:ss a z)e (%D)]  UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm:ss a z)M %(ORG:^z)J\r\n %(\r\n\t^N ^R, ^z ^S)v %w";
    ms_sVEventAllPropertiesMessage = "%J%v%a%k%c%K%H%t%i%D%B%e%C%X%E%O%M%L%p%x%y%R%o%T%r%s%g%S%h%U%u%w%Z";
    ms_sVEventCancelMessage = "%J%K%R%U%s%C%g%Z" ;
    ms_sVEventRequestMessage = "%J%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    ms_sVEventRecurRequestMessage = "%J%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%o%r%s%g%S%h%U%u%w%Z" ;
    ms_sVEventCounterMessage = "%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    ms_sVEventDeclineCounterMessage = "%J%K%R%U%s%T%C%Z";
    ms_sVEventAddMessage = "%J%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    ms_sVEventRefreshMessage = "%v%R%U%C%K%H%Z";
    ms_sVEventReplyMessage = "%J%v%K%R%U%s%T%C%H%X%E";
    // 3-31-98 no attendees in publish anymore
    ms_sVEventPublishMessage = "%J%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    ms_sVEventRecurPublishMessage = "%J%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%o%r%s%g%S%h%U%u%w%Z";

    ms_sVEventDelegateRequestMessage = "%v%K%B%e%i%R%U%s%C";
    ms_sVEventRecurDelegateRequestMessage = "%v%K%B%e%i%U%s%C";

    // TODO: finish
    ms_VTodoStrDefaultFmt = "[%('START:' EEE MM/dd/yy hh:mm:ss a z)B %('DUE:' EEE MM/dd/yy hh:mm:ss z)F %('COMPLETED:' EEE MM/dd/yy hh:mm:ss z)G]  UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm:ss a z)M %(ORG:^z)J\r\n %(\r\n\t^N ^R, ^z ^S)v %w";
    ms_sVTodoAllPropertiesMessage = "%J%v%a%k%c%K%H%t%i%D%B%F%G%P%C%X%E%O%M%L%p%x%y%R%o%T%r%s%g%S%U%u%w%Z";
    ms_sVTodoCancelMessage = "%J%K%R%U%s%C%g%Z" ;
    ms_sVTodoRequestMessage = "%J%v%a%k%c%K%H%t%i%B%F%G%P%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVTodoRecurRequestMessage = "%J%v%a%k%c%K%H%t%i%B%F%G5P%C%X%E%O%M%L%p%x%y%o%r%s%g%S%U%u%w%Z" ;
    ms_sVTodoCounterMessage = "%v%a%k%c%K%H%t%i%B%F%G%P%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVTodoDeclineCounterMessage = "%J%K%R%U%s%T%C%Z";
    ms_sVTodoAddMessage = "%J%v%a%k%c%K%H%t%i%B%F%G%P%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVTodoRefreshMessage = "%v%R%U%C%K%H%Z";
    ms_sVTodoReplyMessage = "%J%v%K%R%U%s%T%C%H%X%E";
    // 4-22-98 no attendees in publish anymore
    ms_sVTodoPublishMessage = "%J%a%k%c%K%H%t%i%B%F%G%P%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVTodoRecurPublishMessage = "%J%a%k%c%K%H%t%i%B%F%G%P%C%X%E%O%M%L%p%x%y%o%r%s%g%S%U%u%w%Z";
    /*
    ms_VTodoStrDefaultFmt = "[%(EEE MM/dd/yy hh:mm:ss a z)B - %(EEE MM/dd/yy hh:mm:ss a z)F (%D)]  UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm:ss a z)M %(ORG:^z)J\r\n %(\r\n\t^N ^R, ^z ^S)v %w";
    ms_sVTodoAllPropertiesMessage = "%J%v%a%k%c%K%H%t%i%D%B%F%G%C%X%E%O%M%L%p%x%y%R%o%T%r%s%g%S%P%U%u%w%Z";
    ms_sVTodoCancelMessage = "%J%K%R%U%s%C%g%Z" ;
    ms_sVTodoRequestMessage = "%J%v%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%P%U%u%w%Z";
    ms_sVTodoRecurRequestMessage = "%J%v%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%o%r%s%g%S%P%U%u%w%Z" ;
    ms_sVTodoCounterMessage = "%v%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%P%U%u%w%Z";
    ms_sVTodoDeclineCounterMessage = "%J%K%R%U%s%T%C%Z";
    ms_sVTodoAddMessage = "%J%v%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%P%U%u%w%Z";
    ms_sVTodoRefreshMessage = "%v%R%U%C%K%H%Z";
    ms_sVTodoReplyMessage = "%J%v%K%R%U%s%T%C%H%X%E";
    // 4-22-98 no attendees in publish anymore
    ms_sVTodoPublishMessage = "%J%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%P%U%u%w%Z";
    ms_sVTodoRecurPublishMessage = "%J%a%k%c%K%H%t%i%B%F%G%C%X%E%O%M%L%p%x%y%o%r%s%g%S%P%U%u%w%Z";
*/
    //ms_sVTodoDelegateRequestMessage = "%v%K%B%F%G%i%R%U%s%C";
    //ms_sVTodoRecurDelegateRequestMessage = "%v%K%B%F%G%i%U%s%C";

    // TODO: finish
    ms_VJournalStrDefaultFmt = "[%(EEE MM/dd/yy hh:mm:ss a z)B]  UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm:ss a z)M %(ORG:^z)J\r\n %(\r\n\t^N ^R, ^z ^S)v %w";
    ms_sVJournalAllPropertiesMessage = "%J%v%a%k%c%K%H%t%i%D%B%C%X%E%O%M%L%p%x%y%R%o%T%r%s%g%S%U%u%w%Z";
    ms_sVJournalCancelMessage = "%J%K%R%U%s%C%g%Z" ;
    ms_sVJournalRequestMessage = "%J%v%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVJournalRecurRequestMessage = "%J%v%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%o%r%s%g%S%U%u%w%Z" ;
    ms_sVJournalCounterMessage = "%v%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVJournalDeclineCounterMessage = "%J%K%R%U%s%T%C%Z";
    ms_sVJournalAddMessage = "%J%v%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVJournalRefreshMessage = "%v%R%U%C%K%H%Z";
    ms_sVJournalReplyMessage = "%J%v%K%R%U%s%T%C%H%X%E";
    // 4-22-98 no attendees in publish anymore
    ms_sVJournalPublishMessage = "%J%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    ms_sVJournalRecurPublishMessage = "%J%a%k%c%K%H%t%i%B%C%X%E%O%M%L%p%x%y%o%r%s%g%S%U%u%w%Z";

    ms_sVFreebusyReplyMessage = "%J%v%Y%C%B%e%o%T%U%K%t%M%u%Z";

    // 3-31-98 no attendees in publish anymore
    ms_sVFreebusyPublishMessage = "%J%Y%H%B%e%C%o%U%K%t%M%Z";
    
    ms_sVFreebusyRequestMessage = "%J%v%C%B%e%s%U%Z";
    ms_sVFreebusyAllMessage = "%J%v%Y%C%B%e%D%o%T%U%K%t%M%u%s%Z";
    ms_VFreebusyStrDefaultFmt = "%[(EEE MM/dd/yy hh:mm:ss z)B - %(EEE MM/dd/yy hh:mm:ss z)e (%D)], \r\n\t %U, %s, \r\n%Y\r\n %(ORG:^z)J\r\n %(\r\n\t^N ^R, ^z ^S)v %w";

    ms_VTimeZoneStrDefaultFmt = "%I - Start: %(EEE MM/dd/yy hh:mm:ss z)M, %Q\t%V";
    ms_sVTimeZoneAllMessage = "%I%M%Q%V";

    /*
    ms_asDateTimePatterns = 
    {
        UnicodeString("yyyy MM dd HH m s z"),
        UnicodeString("yyyy MM dd HH m s"),
        UnicodeString("EEE MMM dd hh:mm:ss z yyyy"),
        UnicodeString("MMM dd, yyyy h:m a"),
        UnicodeString("MMM dd, yyyy h:m z"),
        UnicodeString("MMM dd, yyyy h:m:s a"),
        UnicodeString("MMM dd, yyyy h:m:s z"),
        UnicodeString("MMMM dd, yyyy h:m a"),
        UnicodeString("MMMM dd, yyyy h:m:s a"),
        UnicodeString("dd MMM yyyy h:m:s z"),
        UnicodeString("dd MMM yyyy h:m:s z"),
        UnicodeString("MM/dd/yy hh:mm:s a"),
        UnicodeString("MM/dd/yy hh:mm:s z"),
        UnicodeString("MM/dd/yy hh:mm z"),
        UnicodeString("MM/dd/yy hh:mm"),
        UnicodeString("hh:mm MM/d/y z")
    };
    */

        ms_asDateTimePatterns[0] = "yyyy MM dd HH m s z";
        ms_asDateTimePatterns[1] = "yyyy MM dd HH m s";
        ms_asDateTimePatterns[2] = "EEE MMM dd hh:mm:ss z yyyy";
        ms_asDateTimePatterns[3] = "MMM dd, yyyy h:m a";
        ms_asDateTimePatterns[4] = "MMM dd, yyyy h:m z";
        ms_asDateTimePatterns[5] = "MMM dd, yyyy h:m:s a";
        ms_asDateTimePatterns[6] = "MMM dd, yyyy h:m:s z";
        ms_asDateTimePatterns[7] = "MMMM dd, yyyy h:m a";
        ms_asDateTimePatterns[8] = "MMMM dd, yyyy h:m:s a";
        ms_asDateTimePatterns[9] = "dd MMM yyyy h:m:s z";
        ms_asDateTimePatterns[10] = "dd MMM yyyy h:m:s z";
        ms_asDateTimePatterns[11] = "MM/dd/yy hh:mm:s a";
        ms_asDateTimePatterns[12] = "MM/dd/yy hh:mm:s z";
        ms_asDateTimePatterns[13] = "MM/dd/yy hh:mm z";
        ms_asDateTimePatterns[14] = "MM/dd/yy hh:mm";
        ms_asDateTimePatterns[15] = "hh:mm MM/d/y z";
}
