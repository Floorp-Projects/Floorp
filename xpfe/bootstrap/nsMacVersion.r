/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// Macintosh version resources

#include "Types.r"


// Version Numbers //

#define		VERSION_MAJOR			0
#define		VERSION_MINOR			0x95	// revision & fix in BCD
#define		VERSION_KIND			alpha	// alpha, beta, or final
#define		VERSION_MICRO			0		// internal stage: alpha or beta number


// Version Strings (Finder's Get Info dialog box) //

#define		VERSION_STRING			"0.9.5+"
//#define 	VERSION_LANG			"en"	// e.g. en, ja, de, fr
//#define 	VERSION_COUNTRY			"_US"	// e.g.,  _JP, _DE, _FR, _US
//#define	VERSION_LOCALE			"[" VERSION_LANG "_" VERSION_COUNTRY "]"

#define		COPYRIGHT_STRING		"© 1998-2001 The Mozilla Organization"
#define		GETINFO_VERSION			VERSION_STRING ", " COPYRIGHT_STRING
#define		PACKAGE_NAME			"Mozilla " VERSION_STRING


// Resources definition

resource 'vers' (1, "Program") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	VERSION_STRING,
	GETINFO_VERSION
};

resource 'vers' (2, "Suite") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	VERSION_STRING,
	PACKAGE_NAME
};

resource 'STR#' (1000, "CanRunStrings") {
  {
    "You cannot run Ò" PACKAGE_NAME "Ó while another copy of Mozilla or Netscape 6 is running. "
    "Ò" PACKAGE_NAME "Ó will now quit."
  }
};
