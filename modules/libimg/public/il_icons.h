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

/* -*- Mode: C; tab-width: 4 -*-
 *  il_icons.h --- Image icon enumeration.
 *  $Id: il_icons.h,v 3.2 1998/07/27 16:09:05 hardts%netscape.com Exp $
 */


#ifndef _IL_ICONS_H_
#define _IL_ICONS_H_

/* Fixed image icons */

#define IL_IMAGE_FIRST	                 0x11
#define IL_IMAGE_DELAYED	         0x11
#define IL_IMAGE_NOT_FOUND	         0x12
#define IL_IMAGE_BAD_DATA	         0x13
#define IL_IMAGE_INSECURE	         0x14
#define IL_IMAGE_EMBED		         0x15
#define IL_IMAGE_LAST		         0x15

#define IL_NEWS_FIRST			 0x21
#define IL_NEWS_CATCHUP 		 0x21
#define IL_NEWS_CATCHUP_THREAD 		 0x22
#define IL_NEWS_FOLLOWUP 		 0x23
#define IL_NEWS_GOTO_NEWSRC 		 0x24
#define IL_NEWS_NEXT_ART 		 0x25
#define IL_NEWS_NEXT_ART_GREY 		 0x26
#define IL_NEWS_NEXT_THREAD 		 0x27
#define IL_NEWS_NEXT_THREAD_GREY 	 0x28
#define IL_NEWS_POST 			 0x29
#define IL_NEWS_PREV_ART 		 0x2A
#define IL_NEWS_PREV_ART_GREY		 0x2B
#define IL_NEWS_PREV_THREAD 		 0x2C
#define IL_NEWS_PREV_THREAD_GREY 	 0x2D
#define IL_NEWS_REPLY 			 0x2E
#define IL_NEWS_RTN_TO_GROUP 		 0x2F
#define IL_NEWS_SHOW_ALL_ARTICLES 	 0x30
#define IL_NEWS_SHOW_UNREAD_ARTICLES     0x31
#define IL_NEWS_SUBSCRIBE 		 0x32
#define IL_NEWS_UNSUBSCRIBE 		 0x33
#define IL_NEWS_FILE			 0x34
#define IL_NEWS_FOLDER			 0x35
#define IL_NEWS_FOLLOWUP_AND_REPLY	 0x36
#define IL_NEWS_LAST			 0x36

#define IL_GOPHER_FIRST		         0x41
#define IL_GOPHER_TEXT	         	 0x41
#define IL_GOPHER_IMAGE	         	 0x42
#define IL_GOPHER_BINARY        	 0x43
#define IL_GOPHER_SOUND	        	 0x44
#define IL_GOPHER_MOVIE	        	 0x45
#define IL_GOPHER_FOLDER        	 0x46
#define IL_GOPHER_SEARCHABLE	         0x47
#define IL_GOPHER_TELNET	         0x48
#define IL_GOPHER_UNKNOWN       	 0x49
#define IL_GOPHER_LAST	        	 0x49

#define IL_EDIT_FIRST                    0x60
#define IL_EDIT_NAMED_ANCHOR             0x61
#define IL_EDIT_FORM_ELEMENT             0x62
#define IL_EDIT_UNSUPPORTED_TAG          0x63
#define IL_EDIT_UNSUPPORTED_END_TAG      0x64
#define IL_EDIT_JAVA                     0x65
#define IL_EDIT_PLUGIN                   0x66
#define IL_EDIT_LAST                     0x66

/* Security Advisor and S/MIME icons */
#define IL_SA_FIRST						0x70
#define IL_SA_SIGNED					0x71
#define IL_SA_ENCRYPTED					0x72
#define IL_SA_NONENCRYPTED				0x73
#define IL_SA_SIGNED_BAD				0x74
#define IL_SA_ENCRYPTED_BAD				0x75
#define IL_SMIME_ATTACHED				0x76
#define IL_SMIME_SIGNED					0x77
#define IL_SMIME_ENCRYPTED				0x78
#define IL_SMIME_ENC_SIGNED				0x79
#define IL_SMIME_SIGNED_BAD				0x7A
#define IL_SMIME_ENCRYPTED_BAD			0x7B
#define IL_SMIME_ENC_SIGNED_BAD			0x7C
#define IL_SA_LAST						0x7C

#define IL_MSG_FIRST					0x80
#define IL_MSG_ATTACH					0x80
#define IL_MSG_LAST						0x80

#endif /* _IL_ICONS_H_ */
