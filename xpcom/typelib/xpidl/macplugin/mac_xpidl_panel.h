/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	mac_xpidl_panel.h
 */

#pragma once

#ifndef __MAC_XPIDL_PANEL__
#define __MAC_XPIDL_PANEL__

#ifndef __TYPES__
#include <Types.h>
#endif

#pragma options align=mac68k

/* this is the name of the panel, as shown in the Finder */
#define kXPIDLPanelName	"xpidl Settings"

/*
 *	AppleScript dictionary info.  As a rule of thumb, dropin panels should use the 
 *	same terminology and numeric code in their 'aete' that the IDE uses if there 
 *	is already a similar item in the IDE's 'aete'.  That is the case here, so we 
 *	merely duplicate applicable 68K Project and 68K Linker user terms below.
 */

enum {
/*	Symbolic Name				   Code		AETE Terminology		*/
	class_XPIDL					= 'XIDL',

	prefsPR_ProjectType			= 'PR01',	/* Project Type			*/
	prefsPR_FileName			= 'PR02',	/* File Name			*/
	prefsLN_GenerateSymFile		= 'LN02',	/* Generate SYM File	*/
	
	/* enumeration for project type */
	enumeration_ProjectType		= 'PRPT',
	enum_Project_Application	= 'PRPA',	/* application			*/
	enum_Project_Library		= 'PRPL',	/* library				*/
	enum_Project_SharedLibrary	= 'PRPS',	/* shared library		*/
	enum_Project_CodeResource	= 'PRPC',	/* code resource		*/
	enum_Project_MPWTool		= 'PRPM'	/* MPW tool				*/
};

enum {
	kXPIDLModeHeader = 1,
	kXPIDLModeJava,
	kXPIDLModeTypelib,
	kXPIDLModeDoc
};

/*	This is the structure that is manipulated by the panel.  The sample 
 *	compiler & linker both "know" about this structure.
 */

enum {
	kXPIDLSettingsVersion = 0x0100
};

struct XPIDLSettings {
	short		version;			/* version # of settings data	*/
	short		mode;				/* one of kXPIDLModeHeader, ...	*/
	Boolean		warnings;			/* generate warnings.			*/
	Boolean		verbose;			/* verbose mode					*/
	Str32Field	output;				/* name of the output file		*/
};

typedef struct XPIDLSettings XPIDLSettings, **XPIDLSettingsHandle;

#pragma options align=reset

#endif	/* __MAC_XPIDL_PANEL__ */
