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
	mac_xpidl.cpp
	
	Metrowerks Codewarrior IDL plugin.
	
	by Patrick C. Beard.
 */

/* standard headers */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new.h>
#include <setjmp.h>

/* system headers */
#include <Files.h>
#include <Errors.h>
#include <Strings.h>

#include "FullPath.h"
#include "MoreFilesExtras.h"

/* compiler headers */
#include "DropInCompilerLinker.h"
#include "CompilerMapping.h"
#include "CWPluginErrors.h"

/* local headers. */
#include "mac_xpidl.h"
#include "mac_console.h"
#include "mac_strings.h"
#include "mac_xpidl_panel.h"

/* prototypes of local functions */
static CWResult	Compile(CWPluginContext context);
static CWResult	Disassemble(CWPluginContext context);
static CWResult	LocateFile(CWPluginContext context, const char* filename, FSSpec& file);

/* external variables */
extern jmp_buf exit_jump;
extern int exit_status;

/* global variables */
CWPluginContext gPluginContext;

/* local variables */
static CWFileSpec gSourceFile;
static char* gSourcePath = NULL;
static CWFileSpec gOutputFile;

extern "C" {
pascal short xpidl_compiler(CWPluginContext context);
int xpidl_main(int argc, char* argv[]);
int xptdump_main(int argc, char* argv[]);

FILE * FSp_fopen(ConstFSSpecPtr spec, const char * open_mode);
}

pascal short xpidl_compiler(CWPluginContext context)
{
	long request;
	if (CWGetPluginRequest(context, &request) != cwNoErr)
		return cwErrRequestFailed;
	
	gPluginContext = context;
	short result = cwNoErr;
	
	/* dispatch on compiler request */
	switch (request) {
	case reqInitCompiler:
		/* compiler has just been loaded into memory */
		break;
		
	case reqTermCompiler:
		/* compiler is about to be unloaded from memory */
		break;
		
	case reqCompile:
		/* compile a source file */
		result = Compile(context);
		break;
	
	case reqCompDisassemble:
		/* disassemble a source file */
		result = Disassemble(context);
		break;
	
	default:
		result = cwErrRequestFailed;
		break;
	}
	
	/* is this necessary? */
	CWDonePluginRequest(context, result);
	
	/* return result code */
	return (result);
}

static char* full_path_to(const FSSpec& file)
{
	short len = 0;
	Handle fullPath = NULL;
	if (FSpGetFullPath(&file, &len, &fullPath) == noErr && fullPath != NULL) {
		char* path = new char[1 + len];
		if (path != NULL) {
			BlockMoveData(*fullPath, path, len);
			path[len] = '\0';
		}
		DisposeHandle(fullPath);
		return path;
	}
	return NULL;
}

static CWResult GetSettings(CWPluginContext context, XPIDLSettings& settings)
{
	CWMemHandle	settingsHand;
	CWResult err = CWGetNamedPreferences(context, kXPIDLPanelName, &settingsHand);
	if (!CWSUCCESS(err))
		return (err);
	
	XPIDLSettings* settingsPtr = NULL;
	err = CWLockMemHandle(context, settingsHand, false, (void**)&settingsPtr);
	if (!CWSUCCESS(err))
		return (err);
	
	settings = *settingsPtr;
	
	err = CWUnlockMemHandle(context, settingsHand);
	if (!CWSUCCESS(err))
		return (err);

	return noErr;
}

static CWResult	Compile(CWPluginContext context)
{
	CWResult err = CWGetMainFileSpec(context, &gSourceFile);
	if (!CWSUCCESS(err))
		return (err);

	long fileNum;
	err = CWGetMainFileNumber(context, &fileNum);
	if (!CWSUCCESS(err))
		return (err);

	// get the name of the source file to compile.
	gSourcePath = p2c_strdup(gSourceFile.name);
	if (gSourcePath == NULL)
		return cwErrOutOfMemory;
	
	// build an argument list and call the compiler.
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeHeader, false, false };
	GetSettings(context, settings);

#if 0	
	// if generating .xpt files, let the IDE tell us where to put the output file.
	// otherwise, put them in the project's output directory.
	if (settings.mode == kXPIDLModeTypelib)
		err = CWGetSuggestedObjectFileSpec(context, fileNum, &gOutputFile);
	else
		err = CWGetOutputFileDirectory(gPluginContext, &gOutputFile);
#else
	// always generate the output file into the project target's data directory.
	err = CWGetSuggestedObjectFileSpec(context, fileNum, &gOutputFile);
#endif
	if (!CWSUCCESS(err))
		return (err);
	
	int argc = 3;
	char* modes[] = { "header", "java", "typelib", "doc" };
	char* argv[] = { "xpidl", "-m", modes[settings.mode - 1], NULL, NULL, NULL, NULL, };
	if (settings.warnings) argv[argc++] = "-w";
	if (settings.verbose) argv[argc++] = "-v";
	argv[argc++] = gSourcePath;
	
	if (setjmp(exit_jump) == 0) {
		if (xpidl_main(argc, argv) != 0)
			err = cwErrRequestFailed;
	} else {
		// evidently the good old exit function got called.
		if (exit_status != 0)
			err = cwErrRequestFailed;
	}

	// if the compilation succeeded, tell CodeWarrior about the output file.
	// this ensures several things:  1. if the output file is deleted by the user,
	// then the IDE will know to recompile it, which is good for dirty builds,
	// where the output files may be hand deleted; 2. if the user elects to remove
	// objects, the output files are deleted. Thanks to robv@metrowerks.com for
	// pointing this new CWPro4 API out.
	if (err == cwNoErr) {
		CWObjectData objectData;
		BlockZero(&objectData, sizeof(objectData));
		
		// for fun, show how large the output file is in the data area.
		long dataSize, rsrcSize;
		if (FSpGetFileSize(&gOutputFile, &dataSize, &rsrcSize) == noErr)
			objectData.idatasize = dataSize;
		
		// tell the IDE that this file was generated by the compiler.
		objectData.objectfile = &gOutputFile;
		
		err = CWStoreObjectData(context, fileNum, &objectData);
	} else {
		// an error occured, delete the output file, which might be a partial file.
		if (gOutputFile.name[0] != 0) {
			::FSpDelete(&gOutputFile);
		}
	}

	delete[] gSourcePath;
	gSourcePath = NULL;

	return (err);
}

static CWResult	Disassemble(CWPluginContext context)
{
	// the disassembly code has moved to the linker.
	return noErr;
}

static CWResult	LocateFile(CWPluginContext context, const char* filename, FSSpec& file)
{
	/* prefill the CWFileInfo struct */
	CWFileInfo fileinfo;
	BlockZero(&fileinfo, sizeof(fileinfo));
	// memset(&fileinfo, 0, sizeof(fileinfo));
	fileinfo.fullsearch = true;
	fileinfo.suppressload = true;
	fileinfo.dependencyType = cwNormalDependency;
	fileinfo.isdependentoffile = kCurrentCompiledFile;

	/* locate the file name using the project's access paths */
	CWResult err = CWFindAndLoadFile(context, filename, &fileinfo);
	if (err == cwNoErr) {
		file = fileinfo.filespec;
	} else if (err == cwErrFileNotFound) {
		char errmsg[200];
		sprintf(errmsg, "Can't locate file \"%s\".", filename);
		CWResult callbackResult = CWReportMessage(context, 0, errmsg, 0, messagetypeError, 0);
	}
	
	return (err);
}

/**
 * Substitute for standard fopen, treats certain filenames specially,
 * and also considers the mode argument. If a file is being opened
 * for reading, the file is assumed to be locateable using CodeWarrior's
 * standard access paths. If it's for writing, the file is opened in
 * the current project's output directory.
 */
FILE* std::fopen(const char* filename, const char *mode)
{
	FSSpec filespec;
	CWResult err = noErr;
	do {
		if (filename == gSourcePath || strcmp(filename, gSourcePath) == 0) {
			// opening the main source file.
			filespec = gSourceFile;
		} else if (mode[0] == 'w') {
			// if an output file, open it in the current compilation's output directory.
			c2p_strcpy(filespec.name, filename);
			filespec.vRefNum = gOutputFile.vRefNum;
			filespec.parID = gOutputFile.parID;
			c2p_strcpy(gOutputFile.name, filename);
		} else {
			// an input file, use CodeWarrior's search paths to find the named source file.
			err = LocateFile(gPluginContext, filename, filespec);
		}
	} while (0);
	// if all went well, we have a file to open.
	return (err == noErr ? FSp_fopen(&filespec, mode) : NULL);
}

/**
 * Returns the length of a file, assuming it is always located in the
 * project's output directory.
 */
size_t mac_get_file_length(const char* filename)
{
	long dataSize= 0, rsrcSize = 0;
	FSSpec filespec;
	if (CWGetOutputFileDirectory(gPluginContext, &filespec) != noErr)
		return 0;
	c2p_strcpy(filespec.name, filename);
	if (FSpGetFileSize(&filespec, &dataSize, &rsrcSize) != noErr)
		return 0;
	return dataSize;
}

void mac_warning(const char* warning_message)
{
	CWReportMessage(gPluginContext, 0, warning_message, 0, messagetypeWarning, 0);
}

void mac_error(const char* error_message)
{
	CWReportMessage(gPluginContext, 0, error_message, 0, messagetypeError, 0);
}

// plugin compiler exports.

#if CW_USE_PRAGMA_EXPORT
#pragma export on
#endif

CWPLUGIN_ENTRY(CWPlugin_GetDropInFlags)(const DropInFlags** flags, long* flagsSize)
{
	static const DropInFlags sFlags = {
		kCurrentDropInFlagsVersion,
		CWDROPINCOMPILERTYPE,
		DROPINCOMPILERLINKERAPIVERSION,
		(kGeneratescode | /* kCandisassemble | */ kCompMultiTargAware | kCompAlwaysReload),
		Lang_MISC,
		DROPINCOMPILERLINKERAPIVERSION
	};
	
	*flags = &sFlags;
	*flagsSize = sizeof(sFlags);
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetDropInName)(const char** dropinName)
{
	static const char* sDropInName = "xpidl";
	
	*dropinName = sDropInName;
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetDisplayName)(const char** displayName)
{
	static const char* sDisplayName = "xpidl";
	
	*displayName = sDisplayName;
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetPanelList)(const CWPanelList** panelList)
{
	static const char* sPanelName = kXPIDLPanelName;
	static CWPanelList sPanelList = { kCurrentCWPanelListVersion, 1, &sPanelName };
	
	*panelList = &sPanelList;
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetTargetList)(const CWTargetList** targetList)
{
	static CWDataType sCPU = '****';
	static CWDataType sOS = '****';
	static CWTargetList sTargetList = { kCurrentCWTargetListVersion, 1, &sCPU, 1, &sOS };
	
	*targetList = &sTargetList;
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetDefaultMappingList)(const CWExtMapList** defaultMappingList)
{
	static CWExtensionMapping sExtension = { 'TEXT', ".idl", 0 };
	static CWExtMapList sExtensionMapList = { kCurrentCWExtMapListVersion, 1, &sExtension };
	
	*defaultMappingList = &sExtensionMapList;
	
	return cwNoErr;
}

#if CW_USE_PRAGMA_EXPORT
#pragma export off
#endif
