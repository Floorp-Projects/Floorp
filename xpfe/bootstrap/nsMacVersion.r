/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

// Macintosh version resources

#include "Types.r"

#define		APPLICATION_NAME		"Gecko"
#define		TRADEMARK_NAME			APPLICATION_NAME "ª "

#define		VERSION_CORP_STR		"mozilla.org"

#define     APPLICATION_LANGUAGE    "en"

#define		VERSION_MAJOR_STR		"5.0"
#define		VERSION_MINOR_STR		"m4 Gecko"
#define		VERSION_STRING			VERSION_MAJOR_STR VERSION_MINOR_STR
#define 	VERSION_LANG			"en"	// e.g. en, ja, de, fr
#define 	VERSION_COUNTRY			"_US"		// e.g.,  _JP, _DE, _FR, _US
#define 	VERSION_LOCALE			VERSION_LANG VERSION_COUNTRY
#define 	VERSION_MAJOR			5
#define		VERSION_MINOR			00		// =[0x00] This is really revision & fix in BCD
#define 	VERSION_MICRO			4		// This is really the internal stage
#define		VERSION_KIND			alpha
#define		COPYRIGHT_STRING		", Mozilla Open Source"


//#define		GETINFO_DESC			TRADEMARK_NAME VERSION_STRING
#define		GETINFO_DESC			VERSION_CORP_STR

#define		GETINFO_VERSION			VERSION_STRING COPYRIGHT_STRING

#ifdef MOZ_LITE
#define		USER_AGENT_PPC_STRING		"(Macintosh; %s; PPC, Nav)"
#define		USER_AGENT_68K_STRING		"(Macintosh; %s; 68K, Nav)"
#else
#define		USER_AGENT_PPC_STRING		"(Macintosh; %s; PPC)"
#define		USER_AGENT_68K_STRING		"(Macintosh; %s; 68K)"
#endif

#define		USER_AGENT_NAME			"Mozilla"

#ifdef	powerc
#define		USER_AGENT_VERSION		VERSION_STRING " [" VERSION_LOCALE "] " USER_AGENT_PPC_STRING
#else
#define		USER_AGENT_VERSION		VERSION_STRING " [" VERSION_LOCALE "] " USER_AGENT_68K_STRING
#endif

resource 'vers' (1, "Program") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	USER_AGENT_VERSION,
	GETINFO_VERSION
};

resource 'vers' (2, "Suite") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	USER_AGENT_VERSION,
	GETINFO_DESC
};
