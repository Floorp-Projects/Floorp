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
#include <sstream>

using std::string;
using std::vector;
using std::ostringstream;

using namespace CrashReporter;

static NSAutoreleasePool* gMainPool;
static CrashReporterUI* gUI = 0;
static string gDumpFile;
static StringTable gQueryParameters;
static string gSendURL;
static vector<string> gRestartArgs;

#define NSSTR(s) [NSString stringWithUTF8String:(s).c_str()]

static NSString* Str(const char* aName)
{
  string str = gStrings[aName];
  if (str.empty()) str = "?";
  return NSSTR(str);
}

static bool RestartApplication()
{
  char** argv = reinterpret_cast<char**>(
    malloc(sizeof(char*) * (gRestartArgs.size() + 1)));

  if (!argv) return false;

  unsigned int i;
  for (i = 0; i < gRestartArgs.size(); i++) {
    argv[i] = (char*)gRestartArgs[i].c_str();
  }
  argv[i] = 0;

  pid_t pid = fork();
  if (pid == -1)
    return false;
  else if (pid == 0) {
    (void)execv(argv[0], argv);
    _exit(1);
  }

  free(argv);

  return true;
}

@implementation CrashReporterUI

-(void)awakeFromNib
{
  gUI = self;
  [window center];

  [window setTitle:[[NSBundle mainBundle]
                      objectForInfoDictionaryKey:@"CFBundleName"]];
}

-(void)showCrashUI:(const string&)dumpfile
   queryParameters:(const StringTable&)queryParameters
           sendURL:(const string&)sendURL
{
  gDumpFile = dumpfile;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;

  [headerLabel setStringValue:Str(ST_CRASHREPORTERHEADER)];

  [self setStringFitVertically:descriptionLabel
                        string:Str(ST_CRASHREPORTERDESCRIPTION)
                  resizeWindow:YES];
  [viewReportLabel setStringValue:Str(ST_VIEWREPORT)];
  [submitReportButton setTitle:Str(ST_CHECKSUBMIT)];
  [emailMeButton setTitle:Str(ST_CHECKEMAIL)];
  [closeButton setTitle:Str(ST_CLOSE)];

  [viewReportScrollView retain];
  [viewReportScrollView removeFromSuperview];

  NSRect restartFrame = [restartButton frame];
  NSRect closeFrame = [closeButton frame];
  if (gRestartArgs.size() == 0) {
    [restartButton removeFromSuperview];
    closeFrame.origin.x = restartFrame.origin.x +
      (restartFrame.size.width - closeFrame.size.width);
    [closeButton setFrame: closeFrame];
    [closeButton setKeyEquivalent:@"\r"];
  } else {
    [restartButton setTitle:Str(ST_RESTART)];
    // shuffle buttons around to fit, since the strings could be
    // a different width
    int oldRestartWidth = restartFrame.size.width;
    [restartButton sizeToFit];
    restartFrame = [restartButton frame];
    // move left by the amount that the button grew
    restartFrame.origin.x -= restartFrame.size.width - oldRestartWidth;
    closeFrame.origin.x -= restartFrame.size.width - oldRestartWidth;
    [restartButton setFrame: restartFrame];
    [closeButton setFrame: closeFrame];
    [restartButton setKeyEquivalent:@"\r"];
  }

  [self updateEmail];
  [self showReportInfo];

  [window makeKeyAndOrderFront:nil];
}

-(void)showErrorUI:(const string&)message
{
  [self setView: errorView animate: NO];

  [errorHeaderLabel setStringValue:Str(ST_CRASHREPORTERHEADER)];
  [self setStringFitVertically:errorLabel
                        string:NSSTR(message)
                  resizeWindow:YES];
  [errorCloseButton setTitle:Str(ST_CLOSE)];

  [window makeKeyAndOrderFront:nil];
}

-(void)showReportInfo
{
  NSDictionary* boldAttr = [NSDictionary
                            dictionaryWithObject:[NSFont boldSystemFontOfSize:0]
                                          forKey:NSFontAttributeName];
  NSDictionary* normalAttr = [NSDictionary
                              dictionaryWithObject:[NSFont systemFontOfSize:0]
                                            forKey:NSFontAttributeName];

  [viewReportTextView setString:@""];
  for (StringTable::iterator iter = gQueryParameters.begin();
       iter != gQueryParameters.end();
       iter++) {
    [[viewReportTextView textStorage]
     appendAttributedString: [[NSAttributedString alloc]
                              initWithString:NSSTR(iter->first + ": ")
                                  attributes:boldAttr]];
    [[viewReportTextView textStorage]
     appendAttributedString: [[NSAttributedString alloc]
                              initWithString:NSSTR(iter->second + "\n")
                                  attributes:normalAttr]];
  }

  [[viewReportTextView textStorage]
   appendAttributedString: [[NSAttributedString alloc]
                            initWithString:NSSTR("\n" + gStrings[ST_EXTRAREPORTINFO])
                            attributes:normalAttr]];
}

-(IBAction)viewReportClicked:(id)sender
{
  NSRect frame = [window frame];
  NSRect scrolledFrame = [viewReportScrollView frame];

  float delta = scrolledFrame.size.height + 5; // FIXME

  if ([viewReportButton state] == NSOnState) {
    [[window contentView] addSubview:viewReportScrollView];
  } else {
    delta = 0 - delta;
  }

  frame.origin.y -= delta;
  frame.size.height += delta;

  int buttonMask = [viewReportButton autoresizingMask];
  int textMask = [viewReportLabel autoresizingMask];
  int reportMask = [viewReportScrollView autoresizingMask];

  [viewReportButton setAutoresizingMask:NSViewMinYMargin];
  [viewReportLabel setAutoresizingMask:NSViewMinYMargin];
  [viewReportScrollView setAutoresizingMask:NSViewMinYMargin];

  [window setFrame: frame display: true animate: NO];

  if ([viewReportButton state] == NSOffState) {
    [viewReportScrollView removeFromSuperview];
  }

  [viewReportButton setAutoresizingMask:buttonMask];
  [viewReportLabel setAutoresizingMask:textMask];
  [viewReportScrollView setAutoresizingMask:reportMask];
}

-(IBAction)closeClicked:(id)sender
{
  [NSApp terminate: self];
}

-(IBAction)closeAndSendClicked:(id)sender
{
  if ([submitReportButton state] == NSOnState) {
    // Hide the dialog after "closing", but leave it around to coordinate
    // with the upload thread
    [window orderOut:nil];
    [self sendReport];
  } else {
    [NSApp terminate:self];
  }
}

-(IBAction)restartClicked:(id)sender
{
  RestartApplication();
  if ([submitReportButton state] == NSOnState) {
    // Hide the dialog after "closing", but leave it around to coordinate
    // with the upload thread
    [window orderOut:nil];
    [self sendReport];
  } else {
    [NSApp terminate:self];
  }
}

-(IBAction)emailMeClicked:(id)sender
{
  [self updateEmail];
  [self showReportInfo];
}

-(void)controlTextDidChange:(NSNotification *)note
{
  // Email text changed, assume they want the "Email me" checkbox
  // updated appropriately
  if ([[emailText stringValue] length] > 0)
    [emailMeButton setState:NSOnState];
  else
    [emailMeButton setState:NSOffState];

  [self updateEmail];
  [self showReportInfo];
}

-(float)setStringFitVertically:(NSControl*)control
                        string:(NSString*)str
                  resizeWindow:(BOOL)resizeWindow
{
  // hack to make the text field grow vertically
  NSRect frame = [control frame];
  float oldHeight = frame.size.height;

  frame.size.height = 10000;
  NSSize oldCellSize = [[control cell] cellSizeForBounds: frame];
  [control setStringValue: str];
  NSSize newCellSize = [[control cell] cellSizeForBounds: frame];

  float delta = newCellSize.height - oldCellSize.height;
  frame.origin.y -= delta;
  frame.size.height = oldHeight + delta;
  [control setFrame: frame];

  if (resizeWindow) {
    NSRect frame = [window frame];
    frame.origin.y -= delta;
    frame.size.height += delta;
    [window setFrame:frame display: true animate: NO];
  }

  return delta;
}

-(void)setView: (NSView*)v animate: (BOOL)animate
{
  NSRect frame = [window frame];

  NSRect oldViewFrame = [[window contentView] frame];
  NSRect newViewFrame = [v frame];

  frame.origin.y += oldViewFrame.size.height - newViewFrame.size.height;
  frame.size.height -= oldViewFrame.size.height - newViewFrame.size.height;

  frame.origin.x += oldViewFrame.size.width - newViewFrame.size.width;
  frame.size.width -= oldViewFrame.size.width - newViewFrame.size.width;

  [window setContentView:v];
  [window setFrame:frame display:true animate:animate];
}

-(void)updateEmail
{
  if ([emailMeButton state] == NSOnState) {
    NSString* email = [emailText stringValue];
    gQueryParameters["Email"] = [email UTF8String];
  } else {
    gQueryParameters.erase("Email");
  }
}

-(void)sendReport
{
  if (![self setupPost])
    [NSApp terminate:self];

  [NSThread detachNewThreadSelector:@selector(uploadThread:)
            toTarget:self
            withObject:mPost];
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
  NSHTTPURLResponse* response = [mPost response];

  bool success;
  string reply;
  if (!data || !response || [response statusCode] != 200) {
    success = false;
    reply = "";

    // if data is nil, we probably logged an error in uploadThread
    if (data != nil && response != nil) {
      ostringstream message;
      message << "Crash report submission failed: server returned status "
              << [response statusCode];
      LogMessage(message.str());
    }
  } else {
    success = true;
    LogMessage("Crash report submitted successfully");

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

  SendCompleted(success, reply);

  if (success) {
    [NSApp terminate:self];
  } else {
    [self showErrorUI:gStrings[ST_SUBMITFAILED]];
  }
}

-(void)uploadThread:(id)post
{
  NSAutoreleasePool* autoreleasepool = [[NSAutoreleasePool alloc] init];
  NSError* error = nil;
  NSData* data = [post send: &error];
  if (error) {
    data = nil;
    NSString* errorDesc = [error localizedDescription];
    string message = [errorDesc UTF8String];
    LogMessage("Crash report submission failed: " + message);
  }

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
  [gUI showErrorUI: gStrings[ST_CRASHREPORTERDEFAULT]];
  [NSApp run];
}

void UIShowCrashUI(const string& dumpfile,
                   const StringTable& queryParameters,
                   const string& sendURL,
                   const vector<string>& restartArgs)
{
  gRestartArgs = restartArgs;

  [gUI showCrashUI: dumpfile
       queryParameters: queryParameters
       sendURL: sendURL];
  [NSApp run];
}

void UIError_impl(const string& message)
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
  FSRef foundRef;
  OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType,
                           kCreateFolder, &foundRef);
  if (err != noErr)
    return false;

  unsigned char path[PATH_MAX];
  FSRefMakePath(&foundRef, path, sizeof(path));
  NSString* destPath = [NSString stringWithUTF8String:reinterpret_cast<char*>(path)];

  // Note that MacOS ignores the vendor when creating the profile hierarchy -
  // all application preferences directories live alongside one another in
  // ~/Library/Application Support/
  destPath = [destPath stringByAppendingPathComponent: NSSTR(product)];
  // Thunderbird stores its profile in ~/Library/Thunderbird,
  // but we're going to put stuff in ~/Library/Application Support/Thunderbird
  // anyway, so we have to ensure that path exists.
  string tempPath = [destPath UTF8String];
  if (!UIEnsurePathExists(tempPath))
    return false;

  destPath = [destPath stringByAppendingPathComponent: @"Crash Reports"];

  settingsPath = [destPath UTF8String];

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

bool UIFileExists(const string& path)
{
  struct stat sb;
  int ret = stat(path.c_str(), &sb);
  if (ret == -1 || !(sb.st_mode & S_IFREG))
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

std::ifstream* UIOpenRead(const string& filename)
{
  return new std::ifstream(filename.c_str(), std::ios::in);
}

std::ofstream* UIOpenWrite(const string& filename, bool append) // append=false
{
  return new std::ofstream(filename.c_str(),
                           append ? std::ios::out | std::ios::app
                                  : std::ios::out);
}
