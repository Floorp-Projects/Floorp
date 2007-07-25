/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Toolkit Crash Reporter
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#include "crashreporter.h"
#include "crashreporter_osx.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

using std::string;

static NSAutoreleasePool* gMainPool;
static CrashReporterUI* gUI = 0;
static string gDumpFile;
static StringTable gQueryParameters;
static string gSendURL;


#define NSSTR(s) [NSString stringWithUTF8String:(s).c_str()]

static NSString *
Str(const char* aName)
{
  string str = gStrings[aName];
  if (str.empty()) str = "?";
  return NSSTR(str);
}

@implementation CrashReporterUI

-(void)awakeFromNib
{
  gUI = self;
  [window center];

  [window setTitle:[[NSBundle mainBundle]
                objectForInfoDictionaryKey:@"CFBundleName"]];
  [descriptionLabel setStringValue:Str(ST_CRASHREPORTERDESCRIPTION)];
  [disableReportingButton setTitle:Str(ST_RADIODISABLE)];

  [closeButton setTitle:Str(ST_CLOSE)];
  [errorCloseButton setTitle:Str(ST_CLOSE)];
}

-(void)showDefaultUI
{
  [dontSendButton setFrame:[sendButton frame]];
  [dontSendButton setTitle:Str(ST_CLOSE)];
  [dontSendButton setKeyEquivalent:@"\r"];
  [sendButton removeFromSuperview];

  [window makeKeyAndOrderFront:nil];
}

-(void)showCrashUI:(const string&)dumpfile
   queryParameters:(const StringTable&)queryParameters
           sendURL:(const string&)sendURL
{
  gDumpFile = dumpfile;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;

  [sendButton setTitle:Str(ST_SEND)];
  [sendButton setKeyEquivalent:@"\r"];
  [dontSendButton setTitle:Str(ST_DONTSEND)];

  [window makeKeyAndOrderFront:nil];
}

-(void)showErrorUI:(const string&)message
{
  [errorLabel setStringValue: NSSTR(message)];

  [self setView: window newView: errorView animate: NO];
  [window makeKeyAndOrderFront:nil];
}

-(IBAction)closeClicked:(id)sender
{
  [NSApp terminate: self];
}

-(IBAction)sendClicked:(id)sender
{
  [self setView: window newView: uploadingView animate: YES];
  [progressBar startAnimation: self];
  [progressLabel setStringValue:Str(ST_SENDTITLE)];

  if (![self setupPost])
    [NSApp terminate];

  [NSThread detachNewThreadSelector:@selector(uploadThread:)
            toTarget:self
            withObject:mPost];
}

-(void)setView:(NSWindow*)w newView: (NSView*)v animate: (BOOL)animate
{
  NSRect frame = [w frame];

  NSRect oldViewFrame = [[w contentView] frame];
  NSRect newViewFrame = [v frame];

  frame.origin.y += oldViewFrame.size.height - newViewFrame.size.height;
  frame.size.height -= oldViewFrame.size.height - newViewFrame.size.height;

  frame.origin.x += oldViewFrame.size.width - newViewFrame.size.width;
  frame.size.width -= oldViewFrame.size.width - newViewFrame.size.width;

  [w setContentView: v];
  [w setFrame: frame display: true animate: animate];
}

-(bool)setupPost
{
  NSURL* url = [NSURL URLWithString:NSSTR(gSendURL)];
  if (!url) return false;

  mPost = [[HTTPMultipartUpload alloc] initWithURL: url];
  if (!mPost) return false;

  NSMutableDictionary* parameters =
    [[NSMutableDictionary alloc] initWithCapacity: gQueryParameters.size()];
  if (!parameters) return false;

  StringTable::const_iterator end = gQueryParameters.end();
  for (StringTable::const_iterator i = gQueryParameters.begin();
       i != end;
       i++) {
    NSString* key = NSSTR(i->first);
    NSString* value = NSSTR(i->second);
    [parameters setObject: value forKey: key];
  }

  [mPost addFileAtPath: NSSTR(gDumpFile) name: @"upload_file_minidump"];
  [mPost setParameters: parameters];

  return true;
}

-(void)uploadComplete:(id)data
{
  [progressBar stopAnimation: self];

  NSHTTPURLResponse* response = [mPost response];

  NSString* status;
  bool success;
  string reply;
  if (!data || !response || [response statusCode] != 200) {
    status = Str(ST_SUBMITFAILED);
    success = false;
    reply = "";
  } else {
    status = Str(ST_SUBMITSUCCESS);
    success = true;

    NSString* encodingName = [response textEncodingName];
    NSStringEncoding encoding;
    if (encodingName) {
      encoding = CFStringConvertEncodingToNSStringEncoding(
        CFStringConvertIANACharSetNameToEncoding((CFStringRef)encodingName));
    } else {
      encoding = NSISOLatin1StringEncoding;
    }
    NSString* r = [[NSString alloc] initWithData: data encoding: encoding];
    reply = [r UTF8String];
  }

  [progressLabel setStringValue: status];
  [closeButton setEnabled: true];
  [closeButton setKeyEquivalent:@"\r"];

  CrashReporterSendCompleted(success, reply);
}

-(void)uploadThread:(id)post
{
  NSAutoreleasePool* autoreleasepool = [[NSAutoreleasePool alloc] init];
  NSError* error = nil;
  NSData* data = [post send: &error];
  if (error)
    data = nil;

  [self performSelectorOnMainThread: @selector(uploadComplete:)
        withObject: data
        waitUntilDone: YES];

  [autoreleasepool release];
}

@end

/* === Crashreporter UI Functions === */

bool UIInit()
{
  gMainPool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

  return true;
}

void UIShutdown()
{
  [gMainPool release];
}

void UIShowDefaultUI()
{
  [gUI showDefaultUI];
  [NSApp run];
}

void UIShowCrashUI(const string& dumpfile,
                   const StringTable& queryParameters,
                   const string& sendURL)
{
  [gUI showCrashUI: dumpfile
       queryParameters: queryParameters
       sendURL: sendURL];
  [NSApp run];
}

void UIError(const string& message)
{
  if (!gUI) {
    // UI failed to initialize, printing is the best we can do
    printf("Error: %s\n", message.c_str());
    return;
  }

  [gUI showErrorUI: message];
  [NSApp run];
}

bool UIGetIniPath(string& path)
{
  path = gArgv[0];
  path.append(".ini");

  return true;
}

bool UIGetSettingsPath(const string& vendor,
                       const string& product,
                       string& settingsPath)
{
  NSArray* paths;
  paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                              NSUserDomainMask,
                                              YES);
  if ([paths count] < 1)
    return false;

  NSString* destPath = [paths objectAtIndex:0];

  // Note that MacOS ignores the vendor when creating the profile hierarchy -
  // all application preferences directories live alongside one another in
  // ~/Library/Application Support/
  destPath = [destPath stringByAppendingPathComponent: NSSTR(product)];
  destPath = [destPath stringByAppendingPathComponent: @"Crash Reports"];

  settingsPath = [destPath UTF8String];

  if (!UIEnsurePathExists(settingsPath))
    return false;

  return true;
}

bool UIEnsurePathExists(const string& path)
{
  int ret = mkdir(path.c_str(), S_IRWXU);
  int e = errno;
  if (ret == -1 && e != EEXIST)
    return false;

  return true;
}

bool UIMoveFile(const string& file, const string& newfile)
{
  return (rename(file.c_str(), newfile.c_str()) != -1);
}

bool UIDeleteFile(const string& file)
{
  return (unlink(file.c_str()) != -1);
}
