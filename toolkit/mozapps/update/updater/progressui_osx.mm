/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Josh Aas <josh@mozilla.com>
 *  Darin Fisher <darin@meer.net>
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
#include <stdio.h>
#include <unistd.h>
#include "progressui.h"
#include "readstrings.h"
#include "errors.h"

#define TIMER_INTERVAL 0.2

static float sProgressVal;  // between 0 and 100
static BOOL sQuit = FALSE;
static StringTable sLabels;
static const char *sUpdatePath;

@interface UpdaterUI : NSObject
{
  IBOutlet NSProgressIndicator *progressBar;
  IBOutlet NSTextField *progressTextField;
}
@end

@implementation UpdaterUI

-(void)awakeFromNib
{
  NSWindow *w = [progressBar window];
  
  [w setTitle:[NSString stringWithUTF8String:sLabels.title]];
  [progressTextField setStringValue:[NSString stringWithUTF8String:sLabels.info]];

  NSRect origTextFrame = [progressTextField frame];
  [progressTextField sizeToFit];

  int widthAdjust = progressTextField.frame.size.width - origTextFrame.size.width;

  if (widthAdjust > 0) {
    NSRect f;
    f.size.width  = w.frame.size.width + widthAdjust;
    f.size.height = w.frame.size.height;
    [w setFrame:f display:YES];
  }

  [w center];

  [progressBar setIndeterminate:NO];
  [progressBar setDoubleValue:0.0];

  [[NSTimer scheduledTimerWithTimeInterval:TIMER_INTERVAL target:self
                                  selector:@selector(updateProgressUI:)
                                  userInfo:nil repeats:YES] retain];

  // Make sure we are on top initially
  [NSApp activateIgnoringOtherApps:YES];
}

// called when the timer goes off
-(void)updateProgressUI:(NSTimer *)aTimer
{
  if (sQuit) {
    [aTimer invalidate];
    [aTimer release];

    // It seems to be necessary to activate and hide ourselves before we stop,
    // otherwise the "run" method will not return until the user focuses some
    // other app.  The activate step is necessary if we are not the active app.
    // This is a big hack, but it seems to do the trick.
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp hide:self];
    [NSApp stop:self];
  }
  
  float progress = sProgressVal;
  
  [progressBar setDoubleValue:(double)progress];
}

// leave this as returning a BOOL instead of NSApplicationTerminateReply
// for backward compatibility
- (BOOL)applicationShouldTerminate:(NSApplication *)sender
{
  return sQuit;
}

@end

int
InitProgressUI(int *pargc, char ***pargv)
{
  sUpdatePath = (*pargv)[1];
  
  return 0;
}

int
ShowProgressUI()
{
  // Only show the Progress UI if the process is taking significant time.
  // Here we measure significant time as taking more than one second.
  
  usleep(500000);
  
  if (sQuit || sProgressVal > 50.0f)
    return 0;

  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/updater.ini", sUpdatePath);
  if (ReadStrings(path, &sLabels) != OK)
    return -1;

  // Continue the update without showing the Progress UI if any of the supplied
  // strings are larger than MAX_TEXT_LEN (Bug 628829).
  if (!(strlen(sLabels.title) < MAX_TEXT_LEN - 1 &&
        strlen(sLabels.info) < MAX_TEXT_LEN - 1))
    return -1;
  
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
  [NSApp run];

  return 0;
}

// Called on a background thread
void
QuitProgressUI()
{
  sQuit = TRUE;
}

// Called on a background thread
void
UpdateProgressUI(float progress)
{
  sProgressVal = progress;  // 32-bit writes are atomic
}
