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
#include "crashreporter_osx.h"

#include <string>
#include <map>
#include <fstream>

using std::string;
using std::map;
using std::ifstream;

typedef map<string,string> StringTable;
typedef map<string,StringTable> StringTableSections;

static string               kExtraDataExtension = ".extra";

static string               gMinidumpPath;
static string               gExtraDataPath;
static StringTableSections  gStrings;
static StringTable          gExtraData;

static BOOL                 gSendFailed;

static NSString *
Str(const char *aName, const char *aSection="Strings")
{
  string str = gStrings[aSection][aName];
  if (str.empty()) str = "?";
  return [NSString stringWithUTF8String:str.c_str()];
}

static bool
ReadStrings(const string &aPath, StringTableSections *aSections)
{
  StringTableSections &sections = *aSections;

  ifstream f(aPath.c_str());
  if (!f.is_open()) return false;

  string currentSection;
  while (!f.eof()) {
    string line;
    std::getline(f, line);
    if (line[0] == ';') continue;

    if (line[0] == '[') {
      int close = line.find(']');
      if (close >= 0)
        currentSection = line.substr(1, close - 1);
      continue;
    }

    int sep = line.find('=');
    if (sep >= 0)
      sections[currentSection][line.substr(0, sep)] = line.substr(sep + 1);
  }

  return f.eof();
}

@implementation CrashReporterUI

-(void)awakeFromNib
{
  NSWindow *w = [descriptionLabel window];
  [w center];

  [w setTitle:[[NSBundle mainBundle]
                objectForInfoDictionaryKey:@"CFBundleName"]];
  [descriptionLabel setStringValue:Str("CrashReporterDescription")];
  [disableReportingButton setTitle:Str("RadioDisable")];

  if (!gMinidumpPath.empty()) {
    [sendButton setTitle:Str("Send")];
    [sendButton setKeyEquivalent:@"\r"];
    [dontSendButton setTitle:Str("DontSend")];
  } else {
    [dontSendButton setFrame:[sendButton frame]];
    [dontSendButton setTitle:Str("Close")];
    [dontSendButton setKeyEquivalent:@"\r"];
    [sendButton removeFromSuperview];
  }

  [closeButton setTitle:Str("Close")];

  [w makeKeyAndOrderFront:nil];
}

-(IBAction)closeClicked:(id)sender
{
  [NSApp terminate: self];
}

-(IBAction)sendClicked:(id)sender
{
  NSWindow *w = [descriptionLabel window];

  [progressBar startAnimation: self];
  [progressLabel setStringValue:Str("SendTitle")];

  [self setupPost];

  if (mPost) {
    [self setView: w newView: uploadingView animate: YES];
    [progressBar startAnimation: self];

    [NSThread detachNewThreadSelector:@selector(uploadThread:)
              toTarget:self
              withObject:nil];
  }
}

-(void)setView:(NSWindow *)w newView: (NSView *)v animate: (BOOL)animate
{
  NSRect frame = [w frame];

  NSRect oldViewFrame = [[w contentView] frame];
  NSRect newViewFrame = [uploadingView frame];

  frame.origin.y += oldViewFrame.size.height - newViewFrame.size.height;
  frame.size.height -= oldViewFrame.size.height - newViewFrame.size.height;

  frame.origin.x += oldViewFrame.size.width - newViewFrame.size.width;
  frame.size.width -= oldViewFrame.size.width - newViewFrame.size.width;

  [w setContentView: v];
  [w setFrame: frame display: true animate: animate];
}

-(void)setupPost
{
  NSURL *url = [NSURL URLWithString:Str("URL", "Settings")];
  if (!url) return;

  mPost = [[HTTPMultipartUpload alloc] initWithURL: url];
  if (!mPost) return;

  NSMutableDictionary *parameters =
    [[NSMutableDictionary alloc] initWithCapacity: gExtraData.size()];

  StringTable::const_iterator end = gExtraData.end();
  for (StringTable::const_iterator i = gExtraData.begin(); i != end; i++) {
    NSString *key = [NSString stringWithUTF8String: i->first.c_str()];
    NSString *value = [NSString stringWithUTF8String: i->second.c_str()];
    [parameters setObject: value forKey: key];
  }

  [mPost setParameters: parameters];

  [mPost addFileAtPath: [NSString stringWithUTF8String: gMinidumpPath.c_str()]
        name: @"upload_file_minidump"];
}

-(void)uploadComplete:(id)error
{
  [progressBar stopAnimation: self];

  NSHTTPURLResponse *response = [mPost response];

  NSString *status;
  if (error || !response || [response statusCode] != 200) {
    status = Str("SubmitFailed");
    gSendFailed = YES;
  } else
    status = Str("SubmitSuccess");

  [progressLabel setStringValue: status];
  [closeButton setEnabled: true];
  [closeButton setKeyEquivalent:@"\r"];
}

-(void)uploadThread:(id)post
{
  NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
  NSError *error = nil;
  [mPost send: &error];

  [self performSelectorOnMainThread: @selector(uploadComplete:)
        withObject: error
        waitUntilDone: nil];

  [autoreleasepool release];
}

@end

string
GetExtraDataFilename(const string& dumpfile)
{
  string filename(dumpfile);
  int dot = filename.rfind('.');
  if (dot < 0)
    return "";

  filename.replace(dot, filename.length() - dot, kExtraDataExtension);
  return filename;
}

int
main(int argc, char *argv[])
{
  NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
  string iniPath(argv[0]);
  iniPath.append(".ini");
  if (!ReadStrings(iniPath, &gStrings)) {
    printf("couldn't read strings\n");
    return -1;
  }

  if (argc > 1) {
    gMinidumpPath = argv[1];
    gExtraDataPath = GetExtraDataFilename(gMinidumpPath);
    if (!gExtraDataPath.empty()) {
      StringTableSections table;
      ReadStrings(gExtraDataPath, &table);
      gExtraData = table[""];
    }
  }

  gSendFailed = NO;

  int ret = NSApplicationMain(argc,  (const char **) argv);

  string deleteSetting = gStrings["Settings"]["Delete"];
  if (!gSendFailed &&
      (deleteSetting.empty() || atoi(deleteSetting.c_str()) > 0)) {
    remove(gMinidumpPath.c_str());
    if (!gExtraDataPath.empty())
      remove(gExtraDataPath.c_str());
  }

  [autoreleasepool release];

  return ret;
}
