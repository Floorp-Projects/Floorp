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

/* global variables */
CWPluginContext gPluginContext;

/* local variables */
static CWFileSpec gSourceFile;
static char* gSourcePath = NULL;

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

	// the compiler only understands full path names.
	gSourcePath = p2c_strdup(gSourceFile.name);
	if (gSourcePath == NULL)
		return cwErrOutOfMemory;
	
	// build an argument list and call the compiler.
	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeHeader, false, false };
	GetSettings(context, settings);
	
	int argc = 3;
	char* modes[] = { "header", "stub", "typelib", "doc" };
	char* argv[] = { "xpidl", "-m", modes[settings.mode - 1], NULL, NULL, NULL, NULL, };
	if (settings.warnings) argv[argc++] = "-w";
	if (settings.verbose) argv[argc++] = "-v";
	argv[argc++] = gSourcePath;
	
	try {
		xpidl_main(argc, argv);
	} catch (int status) {
		// evidently the good old exit function got called.
		err = cwErrRequestFailed;
	}

	delete[] gSourcePath;
	gSourcePath = NULL;

	return (err);
}

// #define USE_FULL_PATH

static CWResult	Disassemble(CWPluginContext context)
{
#if 0
	CWFileSpec sourceFile;
	CWResult err = CWGetMainFileSpec(context, &sourceFile);
	if (!CWSUCCESS(err))
		return (err);

	char* sourceName = p2c_strdup(sourceFile.name);
	char* dot = strrchr(sourceName, '.');
	if (dot != NULL)
		strcpy(dot + 1, "xpt");

	err = CWGetOutputFileDirectory(gPluginContext, &gSourceFile);
	if (!CWSUCCESS(err))
		return (err);

	c2p_strcpy(gSourceFile.name, sourceName);

#ifdef USE_FULL_PATH	
	gSourcePath = full_path_to(gSourceFile);
#else
	gSourcePath = sourceName;
#endif

	XPIDLSettings settings = { kXPIDLSettingsVersion, kXPIDLModeTypelib, false, false };
	GetSettings(context, settings);

	// build an argument list and call xpt_dump.
	int argc = 1;
	char* argv[] = { "xpt_dump", NULL, NULL, NULL };
	if (settings.verbose) argv[argc++] = "-v";
	argv[argc++] = gSourcePath;
	
	try {
		xptdump_main(argc, argv);
	} catch (int status) {
		// evidently the good old exit function got called.
		err = cwErrRequestFailed;
	}

	delete[] gSourcePath;
	gSourcePath = NULL;

	if (err == noErr) {
		CWNewTextDocumentInfo info = {
			NULL,
			mac_console_handle,
			false
		};
		CWResizeMemHandle(context, mac_console_handle, mac_console_count);
		err = CWCreateNewTextDocument(context, &info);
	}

	return (err);
#else
	return noErr;
#endif
}

static CWResult	LocateFile(CWPluginContext context, const char* filename, FSSpec& file)
{
	/* prefill the CWFileInfo struct */
	CWFileInfo fileinfo;
	memset(&fileinfo, 0, sizeof(fileinfo));
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
		} else if (strncmp(mode, "w", 1) == 0) {
			// if an output file, try opening it in the current's project's 
			CWFileSpec outputDir;
			CWResult err = CWGetOutputFileDirectory(gPluginContext, &outputDir);
			if (err == noErr) {
				c2p_strcpy(filespec.name, filename);
				filespec.vRefNum = outputDir.vRefNum;
				Boolean isDirectory;
				err = FSpGetDirectoryID(&outputDir, &filespec.parID, &isDirectory);
			}
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
	CWReportMessage(gPluginContext, 0, warning_message, 0, messagetypeError, 0);
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
