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
	mac_xpt_linker.cpp
	
	CodeWarrior plugin linker, links together multiple .xpt files.
	
	by Patrick C. Beard.
 */

/* standard headers */
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

/* system headers */
#include <Files.h>
#include <Strings.h>
#include <Aliases.h>
#include <Resources.h>

/* compiler headers */
#include "DropInCompilerLinker.h"
#include "CompilerMapping.h"
#include "CWPluginErrors.h"

/* project headers */
#include "mac_xpidl_panel.h"
#include "mac_console.h"
#include "mac_strings.h"
#include "FullPath.h"
#include "MoreFilesExtras.h"

/* use standard CodeWarrior debugger */
#define kDebuggerCreator	'MWDB'

/* prototypes of local functions */
static CWResult	Link(CWPluginContext context);
static CWResult	Disassemble(CWPluginContext context);
static CWResult	GetTargetInfo(CWPluginContext context);

extern "C" {
pascal short xpt_linker(CWPluginContext context);
int xptlink_main(int argc, char* argv[]);
int xptdump_main(int argc, char* argv[]);

FILE * FSp_fopen(ConstFSSpecPtr spec, const char * open_mode);
size_t mac_get_file_length(const char* filename);
}

/* external variables */
extern jmp_buf exit_jump;
extern int exit_status;

/* global variables */
CWPluginContext gPluginContext;

/* local variables */
static CWFileSpec gOutputDirectory;
static CWFileSpec gObjectCodeDirectory;

/*
 *	xpt_linker	-	main entry-point for linker plugin
 *
 */
pascal short xpt_linker(CWPluginContext context)
{
	long request;
	if (CWGetPluginRequest(context, &request) != cwNoErr)
		return cwErrRequestFailed;
	
	gPluginContext = context;
	short result = cwNoErr;
		
	/* dispatch on linker request */
	switch (request) {
	case reqInitLinker:
		/* linker has just been loaded into memory */
		break;
		
	case reqTermLinker:
		/* linker is about to be unloaded from memory */
		break;
		
	case reqLink:
		/* build the final executable */
		result = Link(context);
		break;
		
	case reqDisassemble:
		/* disassemble object code for a given project file */
		result = Disassemble(context);
		break;
	
	case reqTargetInfo:
		/* return info describing target characteristics */
		result = GetTargetInfo(context);
		break;
		
	default:
		result = cwErrRequestFailed;
		break;
	}
	
	result = CWDonePluginRequest(context, result);
	
	/* return result code */
	return result;
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

/**
 * Provides the full path name to a given directory.
 */
static char* full_path_to(short vRefNum, long dirID)
{
	long parID;
	if (GetParentID(vRefNum, dirID, NULL, &parID) == noErr) {
		FSSpec dirSpec = { vRefNum, parID };
		if (GetDirName(vRefNum, dirID, dirSpec.name) == noErr) {
			return full_path_to(dirSpec);
		}
	}
	return NULL;
}

/**
 * Returns the length of a file, assuming it is always located in the
 * project's object code directory.
 */
size_t mac_get_file_length(const char* filename)
{
	FSSpec fileSpec = { gObjectCodeDirectory.vRefNum, gObjectCodeDirectory.parID };
	c2p_strcpy(fileSpec.name, filename);
	long dataSize, rsrcSize;
	if (FSpGetFileSize(&fileSpec, &dataSize, &rsrcSize) != noErr)
		dataSize = 0;
	return dataSize;
}

/**
 * replaces standard fopen -- opens files for writing in the project's output directory,
 * and files for reading in the object code directory.
 */
FILE* std::fopen(const char* filename, const char *mode)
{
	CWFileSpec& fileDir = (mode[0] == 'r' ? gObjectCodeDirectory : gOutputDirectory);
	FSSpec fileSpec = { fileDir.vRefNum, fileDir.parID };
	c2p_strcpy(fileSpec.name, filename);
	return FSp_fopen(&fileSpec, mode);
}

static CWResult GetSettings(CWPluginContext context, XPIDLSettings& settings)
{
	CWMemHandle	settingsHand;
	CWResult err = CWGetNamedPreferences(context, kXPIDLPanelName, &settingsHand);
	if (!CWSUCCESS(err))
		return err;
	
	XPIDLSettings* settingsPtr = NULL;
	err = CWLockMemHandle(context, settingsHand, false, (void**)&settingsPtr);
	if (!CWSUCCESS(err))
		return err;
	
	settings = *settingsPtr;
	
	err = CWUnlockMemHandle(context, settingsHand);
	if (!CWSUCCESS(err))
		return err;

	return cwNoErr;
}

static CWResult LinkHeaders(CWPluginContext context, XPIDLSettings& settings)
{
	// find out how many files there are to link.
	long fileCount = 0;
	CWResult err = CWGetProjectFileCount(context, &fileCount);
	if (err != cwNoErr || fileCount == 0)
		return err;

	// get the output directory.
	FSSpec outputDir;
	err = CWGetOutputFileDirectory(context, &outputDir);
	if (!CWSUCCESS(err))
		return err;
	
	// enumerate all of the output header files, and make aliases to them in
	// the output directory.
	for (long index = 0; (err == cwNoErr) && (index < fileCount); index++) {
		// get the name of each output file.
		CWFileSpec outputFile;
		err = CWGetStoredObjectFileSpec(context, index, &outputFile);
		if (err == cwNoErr) {
			FInfo info;
			err = FSpGetFInfo(&outputFile, &info);
			
			FSSpec aliasFile = { outputDir.vRefNum, outputDir.parID };
			BlockMoveData(outputFile.name, aliasFile.name, 1 + outputFile.name[0]);
			
			AliasHandle alias = NULL;
			if (NewAliasMinimal(&outputFile, &alias) == noErr) {
				// recreate the alias file from scratch.
				FSpDelete(&aliasFile);
				FSpCreateResFile(&aliasFile, info.fdCreator, info.fdType, smRoman);
				short refNum = FSpOpenResFile(&aliasFile, fsRdWrPerm);
				if (refNum != -1) {
					UseResFile(refNum);
					AddResource(Handle(alias), rAliasType, 0, aliasFile.name);
					ReleaseResource(Handle(alias));
					UpdateResFile(refNum);
					CloseResFile(refNum);
				}
				// finally, mark the newly created file as an alias file.
				FSpGetFInfo(&aliasFile, &info);
				info.fdFlags |= kIsAlias;
				FSpSetFInfo(&aliasFile, &info);
			}
		}
	}
	
	// create the target file in the output directory.
	BlockMoveData(settings.output, outputDir.name, 1 + settings.output[0]);
	FILE* outputFile = FSp_fopen(&outputDir, "w");
	if (outputFile != NULL) fclose(outputFile);

	return err;
}

static CWResult LinkTypeLib(CWPluginContext context, XPIDLSettings& settings)
{
	// find out how many files there are to link.
	long fileCount = 0;
	CWResult err = CWGetProjectFileCount(context, &fileCount);
	if (err != cwNoErr || fileCount == 0)
		return err;

	// assemble the argument list.
	// { "xpt_link", outputFile, inputFile1, ..., inputFileN, NULL }
	char** argv = new char*[2 + fileCount + 1];
	int argc = 0;
	argv[argc++] = "xpt_link";

	// get the output directory.
	err = CWGetOutputFileDirectory(context, &gOutputDirectory);
	if (!CWSUCCESS(err))
		return err;
	
	// get the object code directory.
	err = CWGetStoredObjectFileSpec(context, 0, &gObjectCodeDirectory);
	if (!CWSUCCESS(err))
		return err;

	// push the output file name.
	if ((argv[argc++] = p2c_strdup(settings.output)) == NULL)
		return cwErrOutOfMemory;

	for (long index = 0; (err == cwNoErr) && (index < fileCount); index++) {
		// get the name of each output file.
		CWFileSpec outputFile;
		err = CWGetStoredObjectFileSpec(context, index, &outputFile);
		if (err == cwNoErr) {
			if ((argv[argc++] = p2c_strdup(outputFile.name)) == NULL) {
				err = cwErrOutOfMemory;
				break;
			}
		}
	}
	
	if (err != cwNoErr)
		return err;
	
	// trap calls to exit, which longjmp back to here.
	if (setjmp(exit_jump) == 0) {
		if (xptlink_main(argc, argv) != 0)
			err = cwErrRequestFailed;
	} else {
		// evidently the good old exit function got called.
		if (exit_status != 0)
			err = cwErrRequestFailed;
	}
	
	return err;
}

static CWResult	Link(CWPluginContext context)
{
	// load the relevant prefs.
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	CWResult err = GetSettings(context, settings);
	if (err != cwNoErr)
		return err;

	switch (settings.mode) {
	case kXPIDLModeHeader:
		return LinkHeaders(context, settings);
	case kXPIDLModeTypelib:
		return LinkTypeLib(context, settings);
	default:
		return cwNoErr;
	}
}

static CWResult	Disassemble(CWPluginContext context)
{
	CWResult err = noErr;

	// cache the project's output directory.
	err = CWGetOutputFileDirectory(gPluginContext, &gOutputDirectory);
	if (!CWSUCCESS(err))
		return err;

	long fileNum;
	err = CWGetMainFileNumber(context, &fileNum);
	if (!CWSUCCESS(err))
		return err;

	// get the output file's location from the stored object data.
	err = CWGetStoredObjectFileSpec(context, fileNum, &gObjectCodeDirectory);
	if (!CWSUCCESS(err))
		return err;
	
	char* outputName = p2c_strdup(gObjectCodeDirectory.name);
	if (outputName == NULL)
		return cwErrOutOfMemory;

	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	GetSettings(context, settings);

	// build an argument list and call xpt_dump.
	int argc = 1;
	char* argv[] = { "xpt_dump", NULL, NULL, NULL };
	if (settings.verbose) argv[argc++] = "-v";
	argv[argc++] = outputName;
	
	// trap calls to exit, which longjmp back to here.
	if (setjmp(exit_jump) == 0) {
		if (xptdump_main(argc, argv) != 0)
			err = cwErrRequestFailed;
	} else {
		// evidently the good old exit function got called.
		if (exit_status != 0)
			err = cwErrRequestFailed;
	}

	delete[] outputName;

	if (err == noErr) {
		// display the disassembly in its own fresh text window.
		CWNewTextDocumentInfo info = {
			NULL,
			mac_console_handle,
			false
		};
		CWResizeMemHandle(context, mac_console_handle, mac_console_count);
		err = CWCreateNewTextDocument(context, &info);
	}

	return err;
}

static CWResult	GetTargetInfo(CWPluginContext context)
{
	CWTargetInfo targ;
	memset(&targ, 0, sizeof(targ));
	
	CWResult err = CWGetOutputFileDirectory(context, &targ.outfile);
	targ.outputType = linkOutputFile;
	targ.symfile = targ.outfile;	/* location of SYM file */
	targ.linkType = exelinkageFlat;
	targ.targetCPU = '****';
	targ.targetOS = '****';
	
	// load the relevant settings.
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	err = GetSettings(context, settings);
	if (err != cwNoErr)
		return err;
	
#if CWPLUGIN_HOST == CWPLUGIN_HOST_MACOS
	// tell the IDE about the output file.
	targ.outfileCreator		= 'MMCH';
	targ.outfileType		= 'CWIE';
	targ.debuggerCreator	= kDebuggerCreator;	/* so IDE can locate our debugger	*/

	BlockMoveData(settings.output, targ.outfile.name, 1 + settings.output[0]);
	targ.symfile.name[0] = 0;
#endif

#if CWPLUGIN_HOST == CWPLUGIN_HOST_WIN32
	targ.debugHelperIsRegKey = true;
	*(long*)targ.debugHelperName = kDebuggerCreator;
	targ.debugHelperName[4] = 0;
	strcat(targ.outfile.path, "\\");
	strcat(targ.outfile.path, prefsData.outfile);
	strcpy(targ.symfile.path, targ.outfile.path);
	strcat(targ.symfile.path, ".SYM");
#endif

	targ.runfile			= targ.outfile;
	targ.linkAgainstFile	= targ.outfile;

	/* we can only run applications */
	// targ.canRun = (prefsData.projtype == kProjTypeApplication);
	
	/* we can only debug if we have a SYM file */
	// targ.canDebug = prefsData.linksym;	
	
	err = CWSetTargetInfo(context, &targ);
	
	return err;
}

#if 0

#if CW_USE_PRAGMA_EXPORT
#pragma export on
#endif

CWPLUGIN_ENTRY(CWPlugin_GetDropInFlags)(const DropInFlags** flags, long* flagsSize)
{
	static const DropInFlags sFlags = {
		kCurrentDropInFlagsVersion,
		CWDROPINLINKERTYPE,
		DROPINCOMPILERLINKERAPIVERSION_7,
		(linkMultiTargAware | linkAlwaysReload),
		0,
		DROPINCOMPILERLINKERAPIVERSION
	};
	
	*flags = &sFlags;
	*flagsSize = sizeof(sFlags);
	
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetDropInName)(const char** dropinName)
{
	static const char* sDropInName = "xpt Linker";
	*dropinName = sDropInName;
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetDisplayName)(const char** displayName)
{
	static const char* sDisplayName = "xpt Linker";
	*displayName = sDisplayName;
	return cwNoErr;
}

CWPLUGIN_ENTRY(CWPlugin_GetPanelList)(const CWPanelList** panelList)
{
	// +++Turn this on when the sample panel has been converted!
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
	static CWExtensionMapping sExtension = { 'MMCH', ".xpt", 0 };
	static CWExtMapList sExtensionMapList = { kCurrentCWExtMapListVersion, 1, &sExtension };
	
	*defaultMappingList = &sExtensionMapList;
	
	return cwNoErr;
}

CWPLUGIN_ENTRY (CWPlugin_GetFamilyList)(const CWFamilyList** familyList)
{
	static CWFamily sFamily = { 'XIDL', "xpidl Settings" };
	static CWFamilyList sFamilyList = { kCurrentCWFamilyListVersion, 0, &sFamily };
	
	*familyList = &sFamilyList;
	
	return cwNoErr;
}

#if CW_USE_PRAGMA_EXPORT
#pragma export off
#endif

#endif
