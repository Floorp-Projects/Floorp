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

#ifndef _JULIAN_FormFactory_H
#define _JULIAN_FormFactory_H

class JULIAN_PUBLIC JulianFormFactory 
{
public:
		JulianFormFactory();
		JulianFormFactory(NSCalendar& imipCal);
		JulianFormFactory(NSCalendar& imipCal, JulianServerProxy* jsp);
		JulianFormFactory(NSCalendar& imipCal, JulianForm& hostForm, pJulian_Form_Callback_Struct callbacks);

		virtual ~JulianFormFactory();

		/*
		** Returns a new UnicodeString with html that is intended to be enclosed in a real html file.
		** Call is responable for disposing of the returned UnicodeString.
		*/
		XP_Bool			getHTML(UnicodeString* htmlstorage, XP_Bool want_detail = FALSE);

		/*
		** Sets the base NSCalendar.
		*/
		void			setNSCalendar(NSCalendar& newCalendar)		{ firstCalendar = &newCalendar; }

		/*
		** These are the different form button types. The call back url will list
		** as it's first thing the type = the name.
		*/
		static UnicodeString Buttons_Details_Type;
		static UnicodeString Buttons_Add_Type;
		static UnicodeString Buttons_Close_Type;		
		static UnicodeString Buttons_Accept_Type;
		static UnicodeString Buttons_Decline_Type;
		static UnicodeString Buttons_Tentative_Type;
		static UnicodeString Buttons_SendFB_Type;
		static UnicodeString Buttons_SendRefresh_Type;
		static UnicodeString Buttons_DelTo_Type;
		static UnicodeString CommentsFieldName;
		static UnicodeString SubjectSep;

private:
		NSCalendar*		firstCalendar;	// Base NSCalendar 
		ICalComponent*	thisICalComp;	// Current vEnvent or vFreeBusy that is being made into html
		ICalComponent*	serverICalComp;	// Current Database Version vEnvent or vFreeBusy that is being made into html
		XP_Bool			detail;
		UnicodeString*	s;				// Holds the html that is being built up.
		JulianForm*		jf;
		JulianServerProxy* ServerProxy;	// How to get to the server
		int32			maxical;		// # of vevetns otr vfreebusy in the array
		pJulian_Form_Callback_Struct cb;// How to call code to create urls for the buttons

		// These bools show what types of ical objects are being used in this message
		XP_Bool			isEvent;
		XP_Bool			isFreeBusy;

		void			SetDetail(XP_Bool newDetail)				{ detail = newDetail; };
		XP_Bool			isDetail()									{ return detail;};
		XP_Bool			isPreProcessed(UnicodeString icalpropstr)	{ return  (ICalPreProcess.indexOf(icalpropstr) >= 0);  };
		XP_Bool			isPostProcessed(UnicodeString icalpropstr)	{ return  (ICalPostProcess.indexOf(icalpropstr) >= 0); };

		UnicodeString*	doARef(UnicodeString& refText, UnicodeString& refTarget, UnicodeString* outputString);
		UnicodeString*	doFont(UnicodeString& fontText, UnicodeString* outputString);
		UnicodeString*	doItalic(UnicodeString& ItalicText, UnicodeString* outputString);
		UnicodeString*	doBold(UnicodeString& BoldText, UnicodeString* outputString);
		UnicodeString*	doFontFixed(UnicodeString& fontText, UnicodeString* outputString);
		void			doAddHTML(UnicodeString& moreHtml)			{ *s += moreHtml;	};
		void			doAddHTML(UnicodeString* moreHtml)			{ *s += *moreHtml;	};
		void			doAddHTML(char* moreHtml)					{ *s += moreHtml;	};
		void			doPreProcessing(UnicodeString icalpropstr);
		void			doPreProcessingDateTime(UnicodeString& icalpropstr, XP_Bool allday, DateTime &start, DateTime &end, ICalComponent &ic);
		void			doPreProcessingAttend(ICalComponent &ic);
		void			doPreProcessingOrganizer(ICalComponent &ic);
		void			doPostProcessing(UnicodeString icalpropstr);
		void			doDifferenceProcessing(UnicodeString icalpropstr);
		void			doDifferenceProcessingAttendees();
		void			doHeaderMuiltStart();
		void			doHeaderMuilt();
		void			doHeaderMuiltEnd();

		void			doProps(int32 labelCount, UnicodeString labels[], int32 dataCount, UnicodeString data[]);
		void			doHeader(UnicodeString HeaderText);
		void			doClose();
		void			doStatus();
		void			doSingleTableLine(UnicodeString* labelString, UnicodeString* dataString, XP_Bool addSpacer = TRUE);
		void			doCommentText();
		UnicodeString	doCreateButton(UnicodeString *InputType, UnicodeString *ButtonName, XP_Bool addtextField = FALSE);
		void			doAddGroupButton(UnicodeString GroupButton_HTML);
		void			doAddButton(UnicodeString SingleButton_HTML);
		void			doMakeFreeBusyTable();

		void			HandlePublishVEvent();
		void			HandlePublishVFreeBusy(XP_Bool isPublish);
		void			HandleRequestVEvent();
		void			HandleRequestVFreeBusy();
		void			HandleEventReplyVEvent();
		void			HandleEventCancelVEvent();
		void			HandleEventRefreshRequestVEvent();
		void			HandleEventCounterPropVEvent();
		void			HandleDeclineCounterVEvent();

		static UnicodeString Start_HTML;
		static UnicodeString End_HTML;
		static UnicodeString Props_Head_HTML;
		static UnicodeString Props_HTML_Before_Label;
		static UnicodeString Props_HTML_After_Label;
		static UnicodeString Props_HTML_After_Data;
		static UnicodeString Props_HTML_Empty_Label;
		static UnicodeString Props_End_HTML;
		static UnicodeString General_Header_Start_HTML;
		static UnicodeString General_Header_Status_HTML;
		static UnicodeString General_Header_End_HTML;
		static UnicodeString Head2_HTML;
		static UnicodeString Italic_Start_HTML;
		static UnicodeString Italic_End_HTML;
		static UnicodeString Bold_Start_HTML;
		static UnicodeString Bold_End_HTML;
		static UnicodeString Aref_Start_HTML;
		static UnicodeString Aref_End_HTML;
		static UnicodeString ArefTag_End_HTML;
		static UnicodeString nbsp;
		static UnicodeString Accepted_Gif_HTML;
		static UnicodeString Declined_Gif_HTML;
		static UnicodeString Delegated_Gif_HTML;
		static UnicodeString NeedsAction_Gif_HTML;
		static UnicodeString Question_Gif_HTML;
		static UnicodeString Line_3_HTML;
		static UnicodeString Cell_Start_HTML;
		static UnicodeString Cell_End_HTML;
		static UnicodeString Font_Fixed_Start_HTML;
		static UnicodeString Font_Fixed_End_HTML;

		static UnicodeString Start_Font;
		static UnicodeString Start_BIG_Font;
		static UnicodeString End_Font;

		static UnicodeString Buttons_Single_Start_HTML;
		static UnicodeString Buttons_Single_Mid_HTML;
		static UnicodeString Buttons_Single_End_HTML;
		static UnicodeString Buttons_Text_End_HTML;

		static UnicodeString Buttons_Details_Label;
		static UnicodeString Buttons_Add_Label;
		static UnicodeString Buttons_Close_Label;
		static UnicodeString Buttons_Accept_Label;
		static UnicodeString Buttons_Update_Label;
		static UnicodeString Buttons_Decline_Label;
		static UnicodeString Buttons_Tentative_Label;
		static UnicodeString Buttons_SendFB_Label;
		static UnicodeString Buttons_SendRefresh_Label;
		static UnicodeString Buttons_DelTo_Label;

		static UnicodeString Buttons_SaveDel_HTML;

		static UnicodeString Buttons_GroupStart_HTML;
		static UnicodeString Buttons_GroupEnd_HTML;
		static UnicodeString Buttons_GroupSingleStart_HTML;
		static UnicodeString Buttons_GroupSingleEnd_HTML;

		static UnicodeString Text_Label_Start_HTML;
		static UnicodeString Text_Label;
		static UnicodeString Text_Label_End_HTML;
		static UnicodeString Text_Field_HTML;

		static UnicodeString ICalPreProcess;
		static UnicodeString ICalPostProcess;

		static UnicodeString EventInSchedule;
		static UnicodeString EventNotInSchedule;
		static UnicodeString EventConflict;
		static UnicodeString EventNote;
		static UnicodeString EventTest;
		static UnicodeString Text_To;	
		static UnicodeString Text_AllDay;
		static UnicodeString Text_StartOn;
		static UnicodeString Text_Was;
		static UnicodeString MuiltEvent;
		static UnicodeString WhenStr;
		static UnicodeString WhatStr;

		static UnicodeString MuiltEvent_Header_HTML;
		static UnicodeString MuiltFB_Header_HTML;

		/*
		** Free/Busy Table
		*/
		static UnicodeString FBT_Start_HTML;
		static UnicodeString FBT_End_HTML;
		static UnicodeString FBT_NewRow_HTML;
		static UnicodeString FBT_EndRow_HTML;
		static UnicodeString FBT_TimeHead_HTML;
		static UnicodeString FBT_TimeHeadEnd_HTML;
		static UnicodeString FBT_TimeHour_HTML;
		static UnicodeString FBT_TimeHourEnd_HTML;
		static UnicodeString FBT_TD_HourColor_HTML;
		static UnicodeString FBT_TD_HourColorEnd_HTML;
		static UnicodeString FBT_TD_MinuteColor_HTML;
		static UnicodeString FBT_TD_MinuteColorEnd_HTML;
		static UnicodeString FBT_TDOffsetCell_HTML;
		static UnicodeString FBT_TickLong_HTML;
		static UnicodeString FBT_TickShort_HTML;
		static UnicodeString FBT_TimeMin_HTML;
		static UnicodeString FBT_TimeMinEnd_HTML;
		static UnicodeString FBT_HourStart;
		static UnicodeString FBT_HourEnd;

		static UnicodeString FBT_DayStart_HTML;
		static UnicodeString FBT_DayEnd_HTML;
		static UnicodeString FBT_DayEmptyCell_HTML;
		static UnicodeString FBT_DayFreeCell_HTML;
		static UnicodeString FBT_DayBusyCell_HTML;
		static UnicodeString FBT_EmptyRow_HTML;

		static UnicodeString FBT_Legend_Start_HTML;
		static UnicodeString FBT_Legend_Text1_HTML;
		static UnicodeString FBT_Legend_Text2_HTML;
		static UnicodeString FBT_Legend_Text3_HTML;
		static UnicodeString FBT_Legend_TextEnd_HTML;
		static UnicodeString FBT_Legend_End_HTML;

		static UnicodeString FBT_Legend_Title;
		static UnicodeString FBT_Legend_Free;
		static UnicodeString FBT_Legend_Busy;

		static UnicodeString FBT_AM;
		static UnicodeString FBT_PM;
		static UnicodeString FBT_TickMark1;
		static UnicodeString FBT_TickMark2;
		static UnicodeString FBT_TickMark3;
		static UnicodeString FBT_TickMark4;
		static UnicodeString FBT_TickSetting;
		static UnicodeString FBT_TickOffset;
		static UnicodeString FBT_DayFormat;

		/*
		** Publish
		*/
		static UnicodeString publish_Header_HTML;
		static int32		 publish_Fields_Labels_Length;
		static UnicodeString publish_Fields_Labels[];
		static int32		 publish_Fields_Data_HTML_Length;
		static UnicodeString publish_Fields_Data_HTML[];
		static UnicodeString publish_End_HTML;

		/*
		** Publish Detail
		*/
		static int32		 publish_D_Fields_Labels_Length;
		static UnicodeString publish_D_Fields_Labels[];
		static int32		 publish_D_Fields_Data_HTML_Length;
		static UnicodeString publish_D_Fields_Data_HTML[];

		/*
		** Publish VFreeBusy
		*/
		static UnicodeString publishFB_Header_HTML;
		static UnicodeString replyFB_Header_HTML;
		static int32		 publishFB_Fields_Labels_Length;
		static UnicodeString publishFB_Fields_Labels[];
		static int32		 publishFB_Fields_Data_HTML_Length;
		static UnicodeString publishFB_Fields_Data_HTML[];
		static UnicodeString publishFB_End_HTML;

		/*
		** Publish VFreeBusy Detail
		*/
		static int32		 publishFB_D_Fields_Labels_Length;
		static UnicodeString publishFB_D_Fields_Labels[];
		static int32		 publishFB_D_Fields_Data_HTML_Length;
		static UnicodeString publishFB_D_Fields_Data_HTML[];

		/*
		** Request
		*/
		static UnicodeString request_Header_HTML;
		static int32		 request_Fields_Labels_Length;
		static UnicodeString request_Fields_Labels[];
		static int32		 request_Fields_Data_HTML_Length;
		static UnicodeString request_Fields_Data_HTML[];
		// static UnicodeString request_Head2_HTML;
		static UnicodeString request_End_HTML;

		/*
		** Request Detail
		*/
		static int32		 request_D_Fields_Labels_Length;
		static UnicodeString request_D_Fields_Labels[];
		static int32		 request_D_Fields_Data_HTML_Length;
		static UnicodeString request_D_Fields_Data_HTML[];

		/*
		** Request VFreeBusy
		*/
		static UnicodeString request_FB_Header_HTML;			
		static int32		 request_FB_Fields_Labels_Length;	
		static UnicodeString request_FB_Fields_Labels[];		
		static int32		 request_FB_Fields_Data_HTML_Length;
		static UnicodeString request_FB_Fields_Data_HTML[];	
		static UnicodeString request_FB_End_HTML;				

		/*
		** Request VFreeBusy Detail
		*/
		static UnicodeString request_D_FB_Header_HTML;
		static int32		 request_D_FB_Fields_Labels_Length;
		static UnicodeString request_D_FB_Fields_Labels[];
		static int32		 request_D_FB_Fields_Data_HTML_Length;
		static UnicodeString request_D_FB_Fields_Data_HTML[];
		static UnicodeString request_D_FB_End_HTML;

		/*
		** Event Reply
		*/
		static UnicodeString eventreply_Header_HTML;
		static int32		 eventreply_Fields_Labels_Length;
		static UnicodeString eventreply_Fields_Labels[];
		static int32		 eventreply_Fields_Data_HTML_Length;
		static UnicodeString eventreply_Fields_Data_HTML[];
		static UnicodeString eventreply_End_HTML;
		
		/*
		** Event Reply Detail
		*/
		static int32		 eventreply_D_Fields_Labels_Length;
		static UnicodeString eventreply_D_Fields_Labels[];
		static int32		 eventreply_D_Fields_Data_HTML_Length;
		static UnicodeString eventreply_D_Fields_Data_HTML[];

		/*
		** Event Cancel
		*/
		static UnicodeString eventcancel_Header_HTML;
		static int32		 eventcancel_Fields_Labels_Length;
		static UnicodeString eventcancel_Fields_Labels[];
		static int32		 eventcancel_Fields_Data_HTML_Length;
		static UnicodeString eventcancel_Fields_Data_HTML[];
		static UnicodeString eventcancel_End_HTML;
		
		/*
		** Event Cancel Detail
		*/
		static int32		 eventcancel_D_Fields_Labels_Length;
		static UnicodeString eventcancel_D_Fields_Labels[];
		static int32		 eventcancel_D_Fields_Data_HTML_Length;
		static UnicodeString eventcancel_D_Fields_Data_HTML[];
		
		/*
		** Event Refresh Request
		*/
		static UnicodeString eventrefreshreg_Header_HTML;
		static int32		 eventrefreshreg_Fields_Labels_Length;
		static UnicodeString eventrefreshreg_Fields_Labels[];
		static int32		 eventrefreshreg_Fields_Data_HTML_Length;
		static UnicodeString eventrefreshreg_Fields_Data_HTML[];
		static UnicodeString eventrefreshreg_End_HTML;

		/*
		** Event Refresh Request Detail
		*/
		static int32		 eventrefreshreg_D_Fields_Labels_Length;
		static UnicodeString eventrefreshreg_D_Fields_Labels[];
		static int32		 eventrefreshreg_D_Fields_Data_HTML_Length;
		static UnicodeString eventrefreshreg_D_Fields_Data_HTML[];

		/*
		** Event Counter Proposal
		*/
		static UnicodeString eventcounterprop_Header_HTML;

		/*
		** Event Deline Counter
		*/
		static UnicodeString eventdelinecounter_Header_HTML;
		static int32		 eventdelinecounter_Fields_Labels_Length;
		static UnicodeString eventdelinecounter_Fields_Labels[];
		static int32		 eventdelinecounter_Fields_Data_HTML_Length;
		static UnicodeString eventdelinecounter_Fields_Data_HTML[];
};

#endif
