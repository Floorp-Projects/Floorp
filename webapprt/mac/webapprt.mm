/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//PLAN:

 // open my bundle, check for an override binary signature
 // find the newest firefox. open its bundle, get the version number.
 // if the firefox version is different than ours:
 //   delete our own binary,
 //   copy the newer webapprt binary from Firefox
 //   exec it, and quit

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

#include "nsXPCOMGlue.h"
#include "nsINIParser.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"

const char WEBAPPRT_EXECUTABLE[] = "webapprt-stub";
const char FXAPPINI_NAME[] = "application.ini";
const char WEBAPPINI_NAME[] = "webapp.ini";
const char WEBRTINI_NAME[] = "webapprt.ini";

//need the correct relative path here
const char APP_CONTENTS_PATH[] = "/Contents/MacOS/";

//the path to the WebappRT subdir within the Firefox app contents dir
const char WEBAPPRT_PATH[] = "webapprt/";

void ExecNewBinary(NSString* launchPath);

NSString *PathToWebRT(NSString* alternateBinaryID);

NSException* MakeException(NSString* name, NSString* message);

void DisplayErrorAlert(NSString* title, NSString* message);

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

const nsDynamicFunctionLoad kXULFuncs[] = {
  { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
  { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
  { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
  { "XRE_main", (NSFuncPtr*) &XRE_main },
  { nsnull, nsnull }
};

nsresult
AttemptGRELoad(char *greDir)
{
  nsresult rv;
  char xpcomDLLPath[MAXPATHLEN];
  snprintf(xpcomDLLPath, MAXPATHLEN, "%s%s", greDir, XPCOM_DLL);

  rv = XPCOMGlueStartup(xpcomDLLPath);

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return rv;
}

// Copied from toolkit/xre/nsAppData.cpp.
void
SetAllocatedString(const char *&str, const char *newvalue)
{
  NS_Free(const_cast<char*>(str));
  if (newvalue) {
    str = NS_strdup(newvalue);
  } else {
    str = nsnull;
  }
}


int
main(int argc, char **argv)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString *firefoxPath = nil;
  NSString *alternateBinaryID = nil;

  //this is our version, to be compared with the version of the binary we are asked to use
  NSString* myVersion = [NSString stringWithFormat:@"%s", NS_STRINGIFY(GRE_BUILDID)];

  NSLog(@"MY WEBAPPRT BUILDID: %@", myVersion);


  //I need to look in our bundle first, before deciding what firefox binary to use
  NSBundle* myBundle = [NSBundle mainBundle];
  NSString* myBundlePath = [myBundle bundlePath];
  alternateBinaryID = [myBundle objectForInfoDictionaryKey:@"FirefoxBinary"];
  NSLog(@"found override firefox binary: %@", alternateBinaryID);

  @try {
    //find a webapprt binary to launch with.  throws an exception with error dialog if none found.
    firefoxPath = PathToWebRT(alternateBinaryID);
    NSLog(@"USING FIREFOX : %@", firefoxPath);

    NSString *myWebRTPath = [myBundle pathForAuxiliaryExecutable: @"webapprt"];
    if (!myWebRTPath) {
      @throw MakeException(@"Missing WebRT Files", @"Cannot locate binary for this application");
    }

    //GET FIREFOX BUILD ID
    NSString *firefoxINIFilePath = [NSString stringWithFormat:@"%@%s%s", firefoxPath, APP_CONTENTS_PATH, FXAPPINI_NAME];
    nsINIParser ffparser;
    NSLog(@"Looking for firefox ini file here: %@", firefoxINIFilePath);

    if (NS_FAILED(ffparser.Init([firefoxINIFilePath UTF8String]))) {
      NSLog(@"Unable to locate Firefox application.ini");
      @throw MakeException(@"Error", @"Unable to parse environment files for application startup");
    }

    char ffVersChars[MAXPATHLEN];
    if (NS_FAILED(ffparser.GetString("App", "BuildID", ffVersChars, MAXPATHLEN))) {
      NSLog(@"Unable to retrieve Firefox BuildID");
      @throw MakeException(@"Error", @"Unable to determine Firefox version.");
    }
    NSString* firefoxVersion = [NSString stringWithFormat:@"%s", ffVersChars];

    NSLog(@"FIREFOX WEBAPPRT BUILDID: %@", firefoxVersion);
    //GOT FIREFOX BUILD ID

    //COMPARE MY BUILD ID AND FIREFOX BUILD ID
    if ([myVersion compare: firefoxVersion] != NSOrderedSame) {
      //we are going to assume that if they are different, we need to re-copy the webapprt, regardless of whether
      // it is newer or older.  If we don't find a webapprt, then the current Firefox must not be new enough to run webapps.
      NSLog(@"### This Application has an old webrt. Updating it.");
      NSLog(@"### My webapprt path: %@", myWebRTPath);

      NSFileManager* fileClerk = [[NSFileManager alloc] init];
      NSError *errorDesc = nil;

      //we know the firefox path, so copy the new webapprt here
      NSString *newWebRTPath = [NSString stringWithFormat: @"%@%s%s", firefoxPath, APP_CONTENTS_PATH, WEBAPPRT_EXECUTABLE];
      NSLog(@"### Firefox webapprt path: %@", newWebRTPath);
      if (![fileClerk fileExistsAtPath:newWebRTPath]) {
        NSString* msg = [NSString stringWithFormat: @"This version of Firefox (%@) cannot run web applications, because it is not recent enough or damaged", firefoxVersion];
        @throw MakeException(@"Missing WebRT Files", msg);
      }

      [fileClerk removeItemAtPath: myWebRTPath error: &errorDesc];
      if (errorDesc != nil) {
        NSLog(@"failed to unlink old binary file at path: %@ with error: %@", myWebRTPath, errorDesc);
        @throw MakeException(@"Unable To Update", @"Failed preparation for runtime update");
      }

      [fileClerk copyItemAtPath: newWebRTPath toPath: myWebRTPath error: &errorDesc];
      [fileClerk release];
      if (errorDesc != nil) {
        NSLog(@"failed to copy new webrt file: %@", errorDesc);
        @throw MakeException(@"Unable To Update", @"Failed to update runtime");
      } else {
        NSLog(@"### Successfully updated webapprt, relaunching");
      }

      //execv the new binary, and ride off into the sunset
      ExecNewBinary(myWebRTPath);

    } else {
      //we are ready to load XUL and such, and go go go

      NSLog(@"This Application has the newest webrt.  Launching!");

      int result = 0;
      char rtINIPath[MAXPATHLEN];

      // Set up our environment to know where webapp.ini was loaded from.
      char appEnv[MAXPATHLEN];
      snprintf(appEnv, MAXPATHLEN, "%s%s%s", [myBundlePath UTF8String], APP_CONTENTS_PATH, WEBAPPINI_NAME);
      if (setenv("XUL_APP_FILE", appEnv, 1)) {
        NSLog(@"Couldn't set XUL_APP_FILE to: %s", appEnv);
        @throw MakeException(@"Error", @"Unable to set webapp INI file.");
      }
      NSLog(@"Set XUL_APP_FILE to: %s", appEnv);

      //CONSTRUCT GREDIR AND CALL XPCOMGLUE WITH IT
      char greDir[MAXPATHLEN];
      snprintf(greDir, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], APP_CONTENTS_PATH);
      if (!NS_SUCCEEDED(AttemptGRELoad(greDir))) {
          @throw MakeException(@"Error", @"Unable to load XUL files for application startup");
      }

      // NOTE: The GRE has successfully loaded, so we can use XPCOM now

      NS_LogInit();
      { // Scope for any XPCOM stuff we create
        nsINIParser parser;
        if (NS_FAILED(parser.Init(appEnv))) {
          NSLog(@"%s was not found\n", appEnv);
          @throw MakeException(@"Error", @"Unable to parse environment files for application startup");
        }

        // Get the path to the runtime directory.
        char rtDir[MAXPATHLEN];
        snprintf(rtDir, MAXPATHLEN, "%s%s%s", [firefoxPath UTF8String], APP_CONTENTS_PATH, WEBAPPRT_PATH);

        // Get the path to the runtime's INI file.  This is in the runtime
        // directory.
        snprintf(rtINIPath, MAXPATHLEN, "%s%s%s%s", [firefoxPath UTF8String], APP_CONTENTS_PATH, WEBAPPRT_PATH, WEBRTINI_NAME);
        NSLog(@"WebappRT application.ini path: %s", rtINIPath);
        if (![[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithFormat:@"%s", rtINIPath]]) {
          NSString* msg = [NSString stringWithFormat: @"This copy of Firefox (%@) cannot run web applications, because it is missing important files", firefoxVersion];
          @throw MakeException(@"Missing WebRT Files", msg);
        }

        // Load the runtime's INI from its path.
        nsCOMPtr<nsILocalFile> rtINI;
        if (NS_FAILED(XRE_GetFileFromPath(rtINIPath, getter_AddRefs(rtINI)))) {
          NSLog(@"Runtime INI path not recognized: '%s'\n", rtINIPath);
          @throw MakeException(@"Error", @"Incorrect path to base INI file.");
        }

        if (!rtINI) {
          NSLog(@"Error: missing WebappRT application.ini");
          @throw MakeException(@"Error", @"Missing base INI file.");
        }

        nsXREAppData *webShellAppData;
        if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) {
          NSLog(@"Couldn't read WebappRT application.ini: %s", rtINIPath);
          @throw MakeException(@"Error", @"Unable to parse base INI file.");
        }

        char profile[MAXPATHLEN];
        if (NS_FAILED(parser.GetString("Webapp", "Profile", profile, MAXPATHLEN))) {
          NSLog(@"Unable to retrieve profile from web app INI file");
          @throw MakeException(@"Error", @"Unable to retrieve installation profile.");
        }
        NSLog(@"setting app profile: %s", profile);
        SetAllocatedString(webShellAppData->profile, profile);

        nsCOMPtr<nsILocalFile> directory;
        if (NS_FAILED(XRE_GetFileFromPath(rtDir, getter_AddRefs(directory)))) {
          NSLog(@"Unable to open app dir");
          @throw MakeException(@"Error", @"Unable to open application directory.");
        }

        nsCOMPtr<nsILocalFile> xreDir;
        if (NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(xreDir)))) {
          NSLog(@"Unable to open XRE dir");
          @throw MakeException(@"Error", @"Unable to open application XRE directory.");
        }

        xreDir.forget(&webShellAppData->xreDirectory);
        NS_IF_RELEASE(webShellAppData->directory);
        directory.forget(&webShellAppData->directory);

        // There is only XUL.
        result = XRE_main(argc, argv, webShellAppData, 0);

        XRE_FreeAppData(webShellAppData);
      }
      NS_LogTerm();

      return result;
    }

  }
  @catch (NSException *e) {
    NSLog(@"got exception: %@", e);
    DisplayErrorAlert([e name], [e reason]);
  }
  @finally {
    [pool drain];
  }
  return 0;
}  //end main


NSException*
MakeException(NSString* name, NSString* message)
{
  NSException* myException = [NSException
        exceptionWithName:name
        reason:message
        userInfo:nil];
  return myException;
}

void
DisplayErrorAlert(NSString* title, NSString* message)
{
  CFUserNotificationDisplayNotice(0, kCFUserNotificationNoteAlertLevel,
    NULL, NULL, NULL,
    (CFStringRef)title,
    (CFStringRef)message,
    CFSTR("Quit")
    );
}

/* Find the currently installed Firefox, if any, and return
 * an absolute path to it. may return nil */
NSString
*PathToWebRT(NSString* alternateBinaryID)
{
  //default is firefox
  NSString *binaryPath = nil;

  //we look for these flavors of Firefox, in this order
  NSArray* launchBinarySearchList = [NSArray arrayWithObjects: @"org.mozilla.nightly",
                                                                @"org.mozilla.aurora",
                                                                @"org.mozilla.firefox", nil];

  //if they provided a manual override, use that.  If they made an error, it will fail to launch
  if (alternateBinaryID != nil && ([alternateBinaryID length] > 0)) {
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:alternateBinaryID];
    if (binaryPath == nil || [binaryPath length] == 0) {
      @throw MakeException(@"WebappRT Not Found",
                            [NSString stringWithFormat:@"Failed to locate specified override runtime with signature '%@'", alternateBinaryID]);
    }
    return binaryPath;
  }

  //No override found, loop through the various flavors of firefox we have
  for (NSString* signature in launchBinarySearchList) {
    NSLog(@"SEARCHING for webapprt, trying: %@", signature);
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:signature];
    if (binaryPath && [binaryPath length] > 0) {
      return binaryPath;
    }
  }

  NSLog(@"unable to find a valid webrt path");
  @throw MakeException(@"Missing Runtime", @"Mozilla Apps require Firefox to be installed");

  return nil;
}

void
ExecNewBinary(NSString* launchPath)
{
  NSLog(@" launching webrt at path: %@\n", launchPath);

  const char *const newargv[] = {[launchPath UTF8String], NULL};

  NSLog(@"COMMAND LINE: '%@ %s'", launchPath, newargv[0]);
  execv([launchPath UTF8String], (char **)newargv);
}
