MoreFiles

A collection of File Manager and related routines

by Jim Luther (Apple Macintosh Developer Technical Support Emeritus)
with significant code contributions by Nitin Ganatra (Apple Macintosh Developer Technical Support Emeritus)
Copyright © 1992-1998 Apple Computer, Inc.
Portions copyright © 1995 Jim Luther
All rights reserved.

You may incorporate this sample code into your applications without restriction, though the sample code has been provided "AS IS" and the responsibility for its operation is 100% yours. However, what you are not permitted to do is to redistribute the source as "DSC Sample Code" after having made changes. If you're going to redistribute the source, we require that you make it clear in the source that the code was descended from Apple Sample Code, but that you've made changes.


What is MoreFiles?

MoreFiles is a collection of high-level routines written over the last couple of years to answer File Manager questions developers have sent to Apple Developer Technical Support and to answer questions on various online services and the internet. The routines have been tested (but not stress-tested), documented, code-reviewed, and used in both my own programs and in many commercial products.


Important Note

These routines are meant to be used from an application environment. In particular, some routines use static variables which require an A5 world (if you’re building 68K code) and almost all routines make calls that are unsafe at interrupt time (i.e., synchronous File Manager calls and Memory Manager calls). If you plan to use these routines from stand-alone 68K code modules, you may need to make some modifications to the code that uses static variables. Don't even think about using most of these routines from code that executes at interrupt time.

Note: If you need to use the routines in FSpCompat in stand-alone 68K code, set GENERATENODATA to 1 in FSpCompat.c (or pass it in) so globals (static variables) are not used.


Build Environments

MoreFiles is built using the latest release C development environments I have access to. You might have to make some minor changes to compile or link with MoreFiles. You also might have to convert IDE project files to work with the latest IDE versions. I've tried hard to write C code that compiles with no warnings using all current development environments.

I used Universal Interfaces v3.1, but also tested with Universal Interfaces v3.0.1.

The Pascal interfaces to MoreFiles are maintained, but have not been tested with all available Pascal development environments.


Files in the MoreFiles Package

MoreFiles.c - the “glue code” for high-level and FSSpec style routines that were never implemented or added to the standard interface for one reason or another. If you're uncomfortable filling in parameter blocks, you'll find the code in this file very useful.

MoreFiles.h and MoreFiles.p - the documentation and header files for calling the routines included in MoreFiles.c.

MoreFilesExtras.c - a collection of useful File Manager utility routines.

MoreFilesExtras.h and MoreFilesExtras.p - the documentation and header files for calling the routines included in MoreFilesExtras.c. I recommend that you browse through the header files - you'll probably find a routine that you've always wanted.

MoreDesktopMgr.c - a collection of useful high-level Desktop Manager routines. If the Desktop Manager isn't available, the "Desktop" file is used for 'read' operations.

MoreDesktopMgr.h and MoreDesktopMgr.p - the documentation and header files for calling the routines included in MoreDesktopMgr.c.

FileCopy.c - a robust, general purpose file copy routine that does everything the right way. Copying a file on the Macintosh isn't as simple as you'd think if you want to handle all cases (i.e., file server drop boxes, preflighting disk space, etc.). This routine does it all and is fast, too.

FileCopy.h and FileCopy.p - the documentation and header files for calling the routines included in FileCopy.c.

DirectoryCopy.c - a general purpose recursive directory copy routine with a hook for your error handling routine and a hook for filtering what you want or don't want copied.

DirectoryCopy.h and DirectoryCopy.p - the documentation and header files for calling the routines included in DirectoryCopy.c.

FSpCompat.c - the “glue code” for FSSpec style routines that will work under System 6 as much as possible. If you still need to support System 6, you'll find the code in this file very useful.

FSpCompat.h and FSpCompat.p - the documentation and header files for calling the routines included in FSpCompat.c.

Search.c - IndexedSearch and PBCatSearchSyncCompat. IndexedSearch performs an indexed search in and below the specified directory using the same parameters as is passed to PBCatSearch. PBCatSearchSyncCompat uses PBCatSearch (if available) or IndexedSearch (if PBCatSearch is not available) to search a volume. Also included are a couple of examples of how to call PBCatSearch: one that searches by creator and file type, and one that searches by file name.

Search.h and Search.p - the documentation and header files for calling the routines included in Search.c.

FullPath.c - Routines for dealing with full pathnames… if you really must use them. Read the warning at the top of the interface files for reasons why you shouldn't use them most of the time.

FullPath.h and FullPath.p - the documentation and header files for calling the routines included in FullPath.c.

IterateDirectory.c - Routines that perform a recursive iteration (scan) of a specified directory and call your IterateFilterProc function once	for each file and directory found. IterateDirectory lets you decide how deep you want the function to dive into the directory tree - anything from 1 level or all levels. This routine is very useful when you want to do something to each item in a directory.

IterateDirectory.h and IterateDirectory.p - the documentation and header files for calling the routines included in IterateDirectory.c.

MoreFiles.68K.π - the THINK C project file you can use to build a 68K THINK library.

MoreFiles.PPC.π - the Symantec C++ project file you can use to build a PowerPC Symantec library.

MoreFiles.68K.µ - the Metrowerks C project file you can use to build a 68K CodeWarrior library.

MoreFiles.PPC.µ - the Metrowerks C project file you can use to build a PowerPC CodeWarrior library.

BuildMoreFiles and MoreFilesLib.make - the MPW script and make file used to compile and build the MPW versions of the libraries.

Note: __MACOSSEVENFIVEONEORLATER, __MACOSSEVENFIVEORLATER, and __MACOSSEVENORLATER are by default defined to 0 so all compatibility code is included by default.


How to use MoreFiles

You can compile the code, put it in a library, and link it into your programs. You can cut and paste it into your programs. You can use it as example code. Feel free to rip it off and modify it in whatever ways you find work best for you.

All exported routines use Pascal calling conventions so this code can be used from either C or Pascal. (Note: Pascal calling conventions can be turned off starting with MoreFiles 1.4.2. BE CAREFUL WITH THE CALLBACKS if you use this option!)

You'll also notice that all routines that make other File Manager calls return an OSErr result. I always check for error results and you should too. (Well, there's a few places where I purposely ignore errors, but they're commented in the source.)


Documentation

The documentation for the routines can be found in the C header files. There, you'll find a short description of each call and a complete listing of all input and output parameters. I've also included a list of possible result codes for each routine. For example, here's the documentation for one of the routines, GetFileLocation.

/*****************************************************************************/
 
    pascal  OSErr   GetFileLocation(short refNum,
                                    short *vRefNum,
                                    long *dirID,
                                    StringPtr fileName);
    /*  ¶ Get the location of an open file.
        The GetFileLocation function gets the location (volume reference number,
        directory ID, and fileName) of an open file.
       
        refNum      input:  The file reference number of an open file.
        vRefNum     output: The volume reference number.
        dirID       output: The parent directory ID.
        fileName    input:  Points to a buffer (minimum Str63) where the
                            filename is to be returned or must be nil.
                    output:	The filename.
 	
        Result Codes
            noErr        0  No error
            nsvErr     -35  Specified volume doesn’t exist
            fnOpnErr   -38  File not open
            rfNumErr   -51  Reference number specifies nonexistent
                            access path
        __________
	   
        See also:   FSpGetFileLocation
    */
   
/*****************************************************************************/

Some routines have very few comments in their code because they simply make a single File Manager call. For those routines, the routine description is the comment. However, you'll find that more complicated routines have plenty of comments to clarify what and why the code is doing something. If something isn't clear, take a look at "Inside Macintosh: Files" and the TechNote "Inside Macintosh: Files - Errata" for further reference.


Release Notes

v1.0     09/14/93
v1.1     01/22/94
v1.1.1  02/01/94
v1.2     07/20/94
v1.2.1  08/09/94
v1.2.2  11/01/94
v1.3     04/17/95
v1.3.1  06/17/95
________________________________________

v1.4 12/21/95

New Routines:
• Added directory scanning functions, IterateDirectory and FSpIterateDirectory in a new files IterateDirectory.c, IterateDirectory.h and IterateDirectory.p. Check these routines out. I think you'll find them useful.
• Added SetDefault and RestoreDefault to MoreFileExtras. These routines let you set the default directory for those times when you're stuck calling standard C I/O or a library that takes only a filename.
• Added XGetVInfo to MoreFilesExtras. Like GetVInfo, this routine returns the volume name, volume reference number, and free bytes on the volume; like HGetVInfo, XGetVInfo also returns the total number of bytes on the volume. Both freeBytes and totalBytes are UnsignedWide values so this routine can report volume size information for volumes up to 2 terabytes. XGetVInfo's glue code calls through to HGetVInfo if needed to provide backwards compatibility.

Bugs fixed:
• Fixed bug where FileCopy could pass NULL to CopyFileMgrAttributes if PBHCopyFile was used.
• Added 68K alignment to MoreDesktopMgr.c structs.
• Added 68K alignment to MoreFilesExtras.p records.

Other changes:
• Conditionalized FSpCompat.c with SystemSevenOrLater and SystemSevenFiveOrLater so the FSSpec compatibility code is only included when needed. It makes the code a little harder to read, but it removes 2K-3K of code and the overhead of additional calls when SystemSevenOrLater and SystemSevenFiveOrLater are true. See ConditionalMacros.h for an explanation of SystemSevenOrLater and SystemSevenFiveOrLater.
• Moved the code to create a full pathname from GetFullPath to FSpGetFullPath. This accomplished two goals: (1) when FSpGetFullPath is used, the input FSSpec isn't passed to FSMakeFSSpecCompat (it was already an FSSpec) and (2) the code is now smaller. While I was in the code, I changed a couple of calls from Munger to PtrToHand.
• Changed GetDirID to GetDirectoryID so that MoreFiles could be used by MacApp programs (There's a MacApp method named GetDirID).
• Changed DirIDFromFSSpec to FSpGetDirectoryID to be consistent in naming conventions.
• Added macros wrapped with "#if OLDROUTINENAMES … #endif" so GetDirID and DirIDFromFSSpec will still work with your old code.
• Changed alignment #if's to use PRAGMA_ALIGN_SUPPORTED instead of GENERATINGPOWERPC (will they ever stop changing?).
• Changed all occurances of filler2 to ioACUser to match the change in Universal Interfaces 2.1.
• Added type-casts from "void *" to "Ptr" in various places to get rid of compiler warnings.
• Added CallCopyErrProc to DirectoryCopy.h and DirectoryCopy.c (it just looks better that way).
• Added the "__cplusplus" conditional code around all .h header files so they'll work with C++.
• Built libraries with Metrowerks, THINK C, and MPW so everyone can link.

________________________________________

v1.4.1 1/6/96

Bugs fixed:
• Fixed the conditionalized code FSpCreateCompat.
• Fixed the conditionalized code FSpExchangeFilesCompat.
• Fixed the conditionalized code FSpCreateResFileCompat.

Other changes:
• Changed PBStatus(&pb, false) to PBStatusSync(&pb) in GetDiskBlocks.
________________________________________

v1.4.2 3/25/96

New Routines:
• Added FSpResolveFileIDRef to MoreFiles.
• Added GetIOACUser and FSpGetIOACUser to MoreFilesExtras. These routines let you get a directory's access privileges for the current user.
• Added bit masks, macros, and function for testing ioACUser values to MoreFilesExtras.h and MoreFilesExtras.p.
• Added GetVolumeInfoNoName to MoreFilesExtras to put common calls to PBHGetVInfo in one place. Functions that call GetVolumeInfoNoName are: (in DirectoryCopy.c) PreflightDirectoryCopySpace, (in FileCopy.c) PreflightFileCopySpace, (in MoreFilesExtras.c) DetermineVRefNum, CheckVolLock, FindDrive, UnmountAndEject, (in Search.c) CheckVol.
• Added GetCatInfoNoName to MoreFilesExtras to put common calls to PBGetCatInfo in one place. Functions that call GetCatInfoNoName are: (in FileCopy.c) GetDestinationDirInfo, (in MoreDesktopMgr.c) GetCommentID, (in MoreFilesExtras.c) GetDInfo, GetDirectoryID, CheckObjectLock.
• Added TruncPString to MoreFilesExtras. This lets you shorten a Pascal string without breaking the string in the middle of a multi-byte character.
• Added FilteredDirectoryCopy and FSpFilteredDirectoryCopy to DirectoryCopy. FilteredDirectoryCopy and FSpFilteredDirectoryCopy work just like DirectoryCopy and FSpDirectoryCopy only they both take an optional CopyFilterProc parameter which can point to routine you supply. The CopyFilterProc lets your code decide what files or directories are copied to the destination. DirectoryCopy and FSpDirectoryCopy now call through to FilteredDirectoryCopy with a NULL CopyFilterProc.

Bugs fixed:
• Fixed minor bug in GetDiskBlocks where driveQElementPtr->dQRefNum was checked when driveQElementPtr could be NULL.
• DirectoryCopy didn't handle error conditions correctly. In some cases, DirectoryCopy would return noErr when there was a problem and in other cases, the CopyErrProc wasn't called and the function immediately failed.
• The result of DirectoryCopy's CopyErrProc was documented incorrectly.

Other changes and improvements:
• Added result codes to function descriptions in the C header files (these probably aren't a perfect list of possible errors, but they should catch most of the results you'll ever see).
• Removed most of the function descriptions in Pascal interface files since they haven't been completely in sync with the C headers for some time and I don't have time to keep the documentation in both places up to date.
• Rewrote HMoveRenameCompat so it doesn't use the Temporary Items folder.
• Added parameter checking to OnLine so that it doesn't allow the volIndex parameter to be less than or equal to 0.
• Added parameter checking to GetDirItems so that it doesn't allow the itemIndex parameter to be less than or equal to 0.
• FSpExchangeFilesCompat now returns diffVolErr (instead of paramErr)	if the source and the destination are on different volumes.
• Changed GetDirName's name parameter to Str31 and added parameter checking so that it doesn't allow a NULL name parameter.
• Forced errors returned by MoreDesktopMgr routines to be closer to what would be expected if the low-level Desktop Manager calls were used.
• Added conditionalized changes from Fabrizio Oddone so that Pascal calling conventions can be easily disabled. Disabling Pascal calling conventions reduces the code size slightly and allows C compilers to optimize parameter passing. NOTE: If you disable Pascal calling conventions, you'll have to remove the "pascal" keyword from all of the MoreFiles callbacks you've defined in your code.
• Changed DirectoryCopy so that you can copy the source directory's content to a disk's root directory.
• Added a build script and a make file for MPW libraries.
• Added a build script for Metrowerks CodeWarrior libraries.
• Added a build script for Symantec THINK Project Manager and Symantec Project Manager libraries.
• Renamed the Symantec and Metrowerks project files.
• Changed MoreFiles directory structure so that C headers, Pascal interfaces, and the source code aren't in the main directory.

Thanks to Fabrizio Oddone for supplying the conditionalized changes that optionally remove Pascal calling conventions. Thanks to Byron Han for beating the bugs out of DirectoryCopy and for suggesting and prototyping the changes needed for the "copy to root directory" option and the FilteredDirectoryCopy routine in DirectoryCopy.
________________________________________

v1.4.3 8/24/96

Bugs fixed:
• Fixed bug in GetDriverName where dctlDriver is a handle. It was not dereferenced correctly.
• Fixed the MPW build file, BuildMoreFiles, so it would correctly pass options to MoreFilesLib.make.
• GetFullPath now returns fullPathLength = 0 and fullPath = NULL as documented.

Other changes and improvements:
• Added PBXGetVolInfoSync glue code to MoreFilesExtras.c when GENERATINGCFM is true. This allows building with no link errors where PBXGetVolInfoSync isn't included in your development system's standard libraries. This routine shouldn't be needed in MoreFiles at some point in the future after it has been added to your development system's standard libraries.
• Changed BuildMoreFiles and MoreFilesLib.make to use the MrC compiler instead of the PPCC compiler.
• Removed “BuildMoreFiles Symantec” because it's much simpler to include MoreFiles as a subproject in your Symantec project instead of building a library and then including it.
• Removed non-strict ANSI comment from PascalElim.h and used conditional code instead.
• GetFullPath and FSpGetFullPath return full pathname to directories with a trailing colon character. For example, they now return "MyVolume:MyDirectory:" instead of "MyVolume:MyDirectory".
• Changed the MoreFiles feedback email address (below) in this Read Me file.
________________________________________

v1.4.4 12/18/96

Bugs fixed:
• Added "| REGISTER_RESULT_LOCATION(kRegisterD0)" to uppFSDispatchProcInfo in PBXGetVolInfoSync. (the code produced is the same since REGISTER_RESULT_LOCATION(kRegisterD0) happens to be 0).
• Initialized ioDTReqCount before calling PBDTGetComment. See the comment in DTGetComment in MoreDesktopMgr.c for the reasons why.
• Fixed paramErr check in GetDirName in MoreFilesExtras.c so it actually works.
• Rewrote CopyDirectoryAccess in MoreFilesExtras.c for better error handling.
• Fixed error handling in GetAPPLFromDesktopFile in MoreDesktopMgr.c.
• Fixed off by one errors in GetLocalIDFromFREF and GetIconRsrcIDFromLocalID in MoreDesktopMgr.c.
• Changed IterateDirectoryLevel so that it continues iterating when it finds a directory that cannot be accessed due to an afpAccessDenied error.

Other changes and improvements:
• Added const type qualifier to input-only pointer parameters. That includes changing many StringPtr parameters to ConstStr255Param (thanks to Stephen C. Gilardi for starting this project).
• General cleanup to improve readability and code generation.
• Added DTXGetAPPL to MoreDesktopMgr. DTXGetAPPL works like DTGetAPPL only it has an additional input parameter, searchCatalog. If searchCatalog is set to false, the catalog search is skipped if the	application isn't found in the desktop database or desktop file. This is useful if you need to find the application quickly (the catalog search can be quite time consuming).
• Removed "BuildMoreFiles Metrowerks" script. I've decided that it's too hard to keep the scripts working with current IDEs.
• Removed pre-v1.4 release notes from this file because it was too big for SimpleText.
________________________________________

v1.4.5 12/20/96

Bugs fixed:
• Fixed logic bug in HOpenAware and HOpenRFAware. Those two functions and FileCopy (which calls them) DO NOT work in MoreFiles version 1.4.4.
________________________________________

v1.4.6 2/15/97

Bugs fixed:
• Fixed bug in PreflightFileCopySpace (FileCopy.c) introduced in MoreFiles version 1.4.4.
• Fixed problem between PBXGetVolInfoSync and __WANTPASCALELIMINATION conditional.
• HMoveRenameCompat from v1.4.2 through v1.4.5 worked with files but not folders, and comments were not being moved along with the file. Reverted HMoveRenameCompat to the v1.4.1 source and then fixed the possible collision in the Temporary Items folder by creating a new uniquely named subfolder in the Temporary Items folder.

Other changes and improvements:
• Added GetVolState function to MoreFilesExtras. Use GetVolState to determine a volume’s online and eject state and the volume driver’s need for eject requests.
• Added GetVolFileSystemID function to MoreFilesExtras.
• Renamed PascalElim.h to Optimization.h since it now contains additional optimization directives.
• Added OptimizationEnd.h.
• Checks for __WANTPASCALELIMINATION now use #if instead of #ifdef to be consistent with other conditionals in MoreFiles and in Apple interfaces.
• Added “#pragma internal on” if “__MWERKS__” is defined to Optimization.h and OptimizationEnd.h to produce better code under Metrowerks compilers.
• In Optimization.h, define __WANTPASCALELIMINATION to 0 if not already defined so __WANTPASCALELIMINATION can be passed in from the command line.
• In FSpCompat.c, define GENERATENODATA to 0 if not already defined so GENERATENODATA can be passed in from the command line.
• Added FSpDTXGetAPPL to MoreDesktopMgr.
• Added echo lines to MoreFilesLib.make so the build process can be monitored.
________________________________________

v1.4.7 3/1/98

Bugs fixed:
• Fixed DirectoryCopy.p header (FSpFilteredDirectoryCopy wasn't there; FSpDirectoryCopy was there twice).
• PreflightFileCopySpace, PreflightGlobals, and PreflightDirectoryCopySpace now use unsigned long values for block counts to support Mac OS Enhanced volumes which can have more than 64K of allocation blocks, and to fix problems with large files.

Other changes and improvements:
• __USEPRAGMAINTERNAL is now used to control the "#pragma internal on" optimization and it defaults to off in Optimization.h.
• In most cases, output parameters are not changed if error occurs (for example, see ResolveFileIDRef).
• Added __MACOSSEVENFIVEONEORLATER, __MACOSSEVENFIVEORLATER, and __MACOSSEVENORLATER defines so that compatibility and bug fix code can be removed when specific versions of Mac OS are required. (FSpCompat.c is affected the most.)
• Reconditionalized FSpCompat.c so that more compatibility code can be stripped.
• Added XGetVolumeInfoNoName to MoreFilesExtras.c.
• GetFullPath and FSpGetFullPath now allow creation of full pathnames to files or directories that don't exist (all directories up to that file/directory must exist).
• Added BuildAFPXVolMountInfo and RetrieveAFPXVolMountInfo to work with new AppleShare IP clients (AppleShare client versions 3.7 and later).
• Changed API of BuildAFPVolMountInfo to closely match BuildAFPXVolMountInfo. It now allocates the space for the AFPVolMountInfo record.
• RetrieveAFPVolMountInfo now checks offsets and doesn't use them if they are zero.
• Conditionally added volume mount constants and structures (from Files.h in Universal Interfaces 3.0.1 and later) to MoreFilesExtras.h and .p.
• Added XVolumeParam to UniversalFMPB.
• Changed zoneName and serverName fields in MyAFPVolMountInfo to Str32 since AppleTalk names can be 32 characters.
• Added MyAFPXVolMountInfo to MoreFilesExtras.h.
• Added XGetVolumeInfoNoName to MoreFilesExtras.h.
• Removed Sharing.h, Sharing.p, and UpperString.p. The routines defined by these files have been defined for several years, so it's time for them to go.
• Removed FindFolderGlue.o library. The code in the FindFolderGlue.o library is supplied by all recent development environments, so it's time for it to go.
________________________________________

v1.4.8 3/24/98

Bugs fixed:
• Fixed conditional errors introduced in v1.4.7 in FSpCompat.c.
• Initialized realResult to noErr in FSpGetFullPath.
• Changed comment in IterateDirectory.p for MyIterateFilterProc - the CInfoPBRec parameter should be {CONST} VAR.
________________________________________

v1.4.9 9/2/98

Bugs fixed:
• Changed parameter order in PBXGetVolInfoSync to match that in InterfaceLib.
• CreateFileIDRef and FSpCreateFileIDRef now return the file ID when it already exists and PBCreateFileID has returned fidExists or afpIDExists. In other words, you can use the file ID if either function returns noErr, fidExists, or afpIDExists.
________________________________________

Thanks again (you know who you are) for the bug reports and suggestions that helped improve MoreFiles since the last version(s). If you find any more bugs or have any comments, please let us know at:

Internet: dts@apple.com

Please put "Attn: MoreFiles" in the message title.
