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
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
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

#ifndef CRASHREPORTER_OSX_H__
#define CRASHREPORTER_OSX_H__

#include <Cocoa/Cocoa.h>
#include "HTTPMultipartUpload.h"
#include "crashreporter.h"

// Defined below
@class TextViewWithPlaceHolder;

@interface CrashReporterUI : NSObject
{
    IBOutlet NSWindow* mWindow;

    /* Crash reporter view */
    IBOutlet NSTextField* mHeaderLabel;
    IBOutlet NSTextField* mDescriptionLabel;
    IBOutlet NSButton* mViewReportButton;
    IBOutlet NSScrollView* mCommentScrollView;
    IBOutlet TextViewWithPlaceHolder* mCommentText;
    IBOutlet NSButton* mSubmitReportButton;
    IBOutlet NSButton* mIncludeURLButton;
    IBOutlet NSButton* mEmailMeButton;
    IBOutlet NSTextField* mEmailText;
    IBOutlet NSButton* mCloseButton;
    IBOutlet NSButton* mRestartButton;
    IBOutlet NSProgressIndicator* mProgressIndicator;
    IBOutlet NSTextField* mProgressText;

    /* Error view */
    IBOutlet NSView* mErrorView;
    IBOutlet NSTextField* mErrorHeaderLabel;
    IBOutlet NSTextField* mErrorLabel;
    IBOutlet NSButton* mErrorCloseButton;

    /* For "show info" alert */
    IBOutlet NSWindow* mViewReportWindow;
    IBOutlet NSTextView* mViewReportTextView;
    IBOutlet NSButton* mViewReportOkButton;

    HTTPMultipartUpload* mPost;
}

- (void)showCrashUI:(const std::string&)dumpfile
    queryParameters:(const StringTable&)queryParameters
            sendURL:(const std::string&)sendURL;
- (void)showErrorUI:(const std::string&)dumpfile;
- (void)showReportInfo;
- (void)maybeSubmitReport;
- (void)closeMeDown:(id)unused;

- (IBAction)submitReportClicked:(id)sender;
- (IBAction)viewReportClicked:(id)sender;
- (IBAction)viewReportOkClicked:(id)sender;
- (IBAction)closeClicked:(id)sender;
- (IBAction)restartClicked:(id)sender;
- (IBAction)includeURLClicked:(id)sender;
- (IBAction)emailMeClicked:(id)sender;

- (void)controlTextDidChange:(NSNotification *)note;
- (void)textDidChange:(NSNotification *)aNotification;
- (BOOL)textView:(NSTextView *)aTextView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString;

- (void)doInitialResizing;
- (float)setStringFitVertically:(NSControl*)control
                         string:(NSString*)str
                   resizeWindow:(BOOL)resizeWindow;
- (void)setView:(NSView*)v animate: (BOOL) animate;
- (void)enableControls:(BOOL)enabled;
- (void)updateSubmit;
- (void)updateURL;
- (void)updateEmail;
- (void)sendReport;
- (bool)setupPost;
- (void)uploadThread:(HTTPMultipartUpload*)post;
- (void)uploadComplete:(NSData*)data;

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication;
-(void)applicationWillTerminate:(NSNotification *)aNotification;

@end

/*
 * Subclass NSTextView to provide a text view with placeholder text.
 * Also provide a setEnabled implementation.
 */
@interface TextViewWithPlaceHolder : NSTextView {
  NSMutableAttributedString *mPlaceHolderString;
}

- (BOOL)becomeFirstResponder;
- (void)drawRect:(NSRect)rect;
- (BOOL)resignFirstResponder;  
- (void)setPlaceholder:(NSString*)placeholder;
- (void)insertTab:(id)sender;
- (void)insertBacktab:(id)sender;
- (void)setEnabled:(BOOL)enabled;
- (void)dealloc;

@end

#endif
