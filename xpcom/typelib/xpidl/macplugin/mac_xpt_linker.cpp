/*
 *  Sample Linker.c	- public interface to Sample Linker
 *
 *  Copyright © 1993 metrowerks inc.  All rights reserved.
 *
 */

/* standard headers */
#include <stdio.h>
#include <string.h>

/* system headers */
#include <Files.h>
#include <Strings.h>

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

/* global variables */
CWPluginContext gPluginContext;

/* local variables */
static CWFileSpec gOutputDirectory;

/*
 *	main	-	main entry-point for Drop-In Sample linker
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

struct SamplePref {};

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
 * project's output directory.
 */
size_t mac_get_file_length(const char* filename)
{
	FSSpec filespec = { gOutputDirectory.vRefNum, gOutputDirectory.parID };
	c2p_strcpy(filespec.name, filename);
	long dataSize, rsrcSize;
	if (FSpGetFileSize(&filespec, &dataSize, &rsrcSize) != noErr)
		dataSize = 0;
	return dataSize;
}

/**
 * replaces standard fopen, assuming the file is always located in the
 * project's output directory.
 */
FILE* std::fopen(const char* filename, const char *mode)
{
	FSSpec filespec = { gOutputDirectory.vRefNum, gOutputDirectory.parID };
	c2p_strcpy(filespec.name, filename);
	return FSp_fopen(&filespec, mode);
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

// #define USING_FULL_PATHS

static CWResult	Link(CWPluginContext context)
{
	long		index;
	CWResult	err;
	long		filecount;

	/* load the relevant prefs */
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	err = GetSettings(context, settings);
	if (err != cwNoErr)
		return (err);

	/*
	 *	Once all initialization has been done, the principal interaction 
	 *	between the linker and the IDE occurs in a loop where the linker 
	 *	processes all files in the project.
	 */
	 
	err = CWGetProjectFileCount(context, &filecount);
	if (err != cwNoErr)
		return (err);

	// assemble the argument list.
	// { "xpt_link", outputFile, inputFile1, ..., inputFileN, NULL }
	char** argv = new char*[2 + filecount + 1];
	int argc = 0;
	argv[argc++] = "xpt_link";

	// get the full path to the output directory.
	FSSpec& outputDir = gOutputDirectory;
	err = CWGetOutputFileDirectory(context, &outputDir);
	if (!CWSUCCESS(err))
		return (err);

#ifdef USING_FULL_PATHS	
	char* outputPrefix = full_path_to(outputDir.vRefNum, outputDir.parID);
	if (outputPrefix == NULL)
		return cwErrOutOfMemory;
	size_t outputPrefixLen = strlen(outputPrefix);
	
	// full path to output file.
	char* outputFilePath = new char[outputPrefixLen + settings.output[0] + 1];
	if (outputFilePath == NULL)
		return cwErrOutOfMemory;
	strcpy(outputFilePath, outputPrefix);
	p2c_strcpy(outputFilePath + outputPrefixLen, settings.output);
	argv[argc++] = outputFilePath;
#else
	// push the output file name.
	argv[argc++] = p2c_strdup(settings.output);
#endif

	for (index = 0; (err == cwNoErr) && (index < filecount); index++) {
		CWProjectFileInfo	fileInfo;
		
		/* first, get info about the file */
		err = CWGetFileInfo(context, index, false, &fileInfo);
		if (err != cwNoErr)
			continue;
		
		// create file spec for the resulting .xpt file.
		// idea:  compiler could just store an alias to the darn file in each object code entry.
		FSSpec& xptFile = fileInfo.filespec;
		xptFile.vRefNum = outputDir.vRefNum;
		xptFile.parID = outputDir.parID;
		
		// change the extension from .idl to .xpt.
		size_t len = xptFile.name[0] - 2;
		xptFile.name[len++] = 'x';
		xptFile.name[len++] = 'p';
		xptFile.name[len++] = 't';

#ifdef USING_FULL_PATHS
		// construct a full path to the .xpt file.
		char* xptFilePath = new char[outputPrefixLen + xptFile.name[0] + 1];
		if (xptFilePath == NULL)
			return cwErrOutOfMemory;
		strcpy(xptFilePath, outputPrefix);
		p2c_strcpy(xptFilePath + outputPrefixLen, xptFile.name);

		argv[argc++] = xptFilePath;
#else
		argv[argc++] = p2c_strdup(xptFile.name);
#endif

#if 0		
		/* determine if we need to process this file */
		if (!fileInfo.hasobjectcode && !fileInfo.hasresources && !fileInfo.isresourcefile)
			continue;
		
		if (fileInfo.isresourcefile) {
			/* handle resource files here */
		} else {
			/*
			 *	Other kinds of files store stuff in the project, either in the form 
			 *	of object code or of a resource fork image.  We handle those here.
			 */
			
			/* load the object data */
			CWMemHandle objectData;
			err = CWLoadObjectData(context, index, &objectData);
			if (err != cwNoErr)
				continue;
			
			if (fileInfo.hasobjectcode) {
				/* link the object code */
			}
			
			if (fileInfo.hasresources) {
				/* copy resources */
			}
			
			/* release the object code when done */
			err = CWFreeObjectData(context, index, objectData);
		}
#endif
	}
	
	try {
		xptlink_main(argc, argv);
	} catch (int status) {
		// evidently the good old exit function got called.
		err = cwErrRequestFailed;
	}
	
	return (err);
}

static CWResult	Disassemble(CWPluginContext context)
{
	CWResult err = noErr;

	long fileNum;
	err = CWGetMainFileNumber(context, &fileNum);
	if (!CWSUCCESS(err))
		return (err);

	CWProjectFileInfo fileInfo;
	err = CWGetFileInfo(context, fileNum, false, &fileInfo);
	if (!CWSUCCESS(err))
		return (err);

	CWFileSpec sourceFile = fileInfo.filespec;
	char* sourceName = p2c_strdup(sourceFile.name);
	char* dot = strrchr(sourceName, '.');
	if (dot != NULL)
		strcpy(dot + 1, "xpt");

	err = CWGetOutputFileDirectory(gPluginContext, &gOutputDirectory);
	if (!CWSUCCESS(err))
		return (err);

	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	GetSettings(context, settings);

	// build an argument list and call xpt_dump.
	int argc = 1;
	char* argv[] = { "xpt_dump", NULL, NULL, NULL };
	if (settings.verbose) argv[argc++] = "-v";
	argv[argc++] = sourceName;
	
	try {
		xptdump_main(argc, argv);
	} catch (int status) {
		// evidently the good old exit function got called.
		err = cwErrRequestFailed;
	}

	delete[] sourceName;

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

	return (err);
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
	
	/* load the relevant prefs */
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	err = GetSettings(context, settings);
	if (err != cwNoErr)
		return (err);
	
#if CWPLUGIN_HOST == CWPLUGIN_HOST_MACOS
	targ.outfileCreator		= 'MOSS';
	targ.outfileType		= 'TYPL';
	targ.debuggerCreator	= kDebuggerCreator;	/* so IDE can locate our debugger	*/

	/* we put output file in same folder as project, but with name stored in prefs */
	// targ.outfile.name[0] = strlen(prefsData.outfile);
	// BlockMoveData(prefsData.outfile, targ.outfile.name + 1, targ.outfile.name[0]);
	
	/* we put SYM file in same folder as project, but with name stored in prefs */
	BlockMoveData(targ.outfile.name, targ.symfile.name, StrLength(targ.outfile.name)+1);
	{
		char* cstr;
		cstr = p2cstr(targ.symfile.name);
		strcat(cstr, ".SYM");
		c2pstr(cstr);
	}
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
