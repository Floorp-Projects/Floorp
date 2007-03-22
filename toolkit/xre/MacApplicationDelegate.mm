/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XUL Toolkit.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stan Shebs <shebs@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NSApplication delegate for Mac OS X Cocoa API.

// As of 10.4 Tiger, the system can send six kinds of Apple Events to an application;
// a well-behaved XUL app should have some kind of handling for all of them.
//
// See http://developer.apple.com/documentation/Cocoa/Conceptual/ScriptableCocoaApplications/SApps_handle_AEs/chapter_11_section_3.html for details.

#import <Cocoa/Cocoa.h>

#include "nsCOMPtr.h"
#include "nsINativeAppSupport.h"
#include "nsAppRunner.h"
#include "nsComponentManagerUtils.h"
#include "nsCommandLineServiceMac.h"

@interface MacApplicationDelegate : NSObject
{
}

@end

// Something to call from non-objective code.

void
SetupMacApplicationDelegate()
{
  // Create the delegate. This should be around for the lifetime of the app.
  MacApplicationDelegate *delegate = [[MacApplicationDelegate alloc] init];
  [[NSApplication sharedApplication] setDelegate:delegate];
}

@implementation MacApplicationDelegate

// Opening the application is handled specially elsewhere,
// don't define applicationOpenUntitledFile: .

// The method that NSApplication calls upon a request to reopen, such as when
// the Dock icon is clicked and no windows are open.

- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApp hasVisibleWindows:(BOOL)flag
{
  // If there are windows already, nothing to do.
  if (flag)
    return NO;
 
  nsCOMPtr<nsINativeAppSupport> nas = do_CreateInstance(NS_NATIVEAPPSUPPORT_CONTRACTID);
  NS_ENSURE_TRUE(nas, NO);

  // Go to the common Carbon/Cocoa reopen method.
  nsresult rv = nas->ReOpen();
  NS_ENSURE_SUCCESS(rv, NO);

  // NO says we don't want NSApplication to do anything else for us.
  return NO;
}

// The method that NSApplication calls when documents are requested to be opened.
// It will be called once for each selected document.

- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename
{
  FSRef ref;
  FSSpec spec;
  // The cast is kind of freaky, but apparently it's what all the beautiful people do.
  OSStatus status = FSPathMakeRef((UInt8 *)[filename fileSystemRepresentation], &ref, NULL);
  if (status != noErr) {
    NS_WARNING("FSPathMakeRef in openFile failed, skipping file open");
    return NO;
  }
  status = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
  if (status != noErr) {
    NS_WARNING("FSGetCatalogInfo in openFile failed, skipping file open");
    return NO;
  }

  // Take advantage of the existing "command line" code for Macs.
  nsMacCommandLine& cmdLine = nsMacCommandLine::GetMacCommandLine();
  // We don't actually care about Mac filetypes in this context, just pass a placeholder.
  cmdLine.HandleOpenOneDoc(spec, 'abcd');

  return YES;
}

// The method that NSApplication calls when documents are requested to be printed
// from the Finder (under the "File" menu).
// It will be called once for each selected document.

- (BOOL)application:(NSApplication*)theApplication printFile:(NSString*)filename
{
  FSRef ref;
  FSSpec spec;
  // The cast is kind of freaky, but apparently it's what all the beautiful people do.
  OSStatus status = FSPathMakeRef((UInt8 *)[filename fileSystemRepresentation], &ref, NULL);
  if (status != noErr) {
    NS_WARNING("FSPathMakeRef in printFile failed, skipping printing");
    return NO;
  }
  status = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
  if (status != noErr) {
    NS_WARNING("FSGetCatalogInfo in printFile failed, skipping printing");
    return NO;
  }

  // Take advantage of the existing "command line" code for Macs.
  nsMacCommandLine& cmdLine = nsMacCommandLine::GetMacCommandLine();
  // We don't actually care about Mac filetypes in this context, just pass a placeholder.
  cmdLine.HandlePrintOneDoc(spec, 'abcd');

  return YES;
}

// The open contents Apple Event 'ocon' (new in 10.4) does not have a delegate method
// associated with it; it would need Carbon event code to handle.

// Quitting is handled specially, don't define applicationShouldTerminate: .

@end

