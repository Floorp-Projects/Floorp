/*
	mac_xpidl_panel.h
 */

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
	kXPIDLModeStub,
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
