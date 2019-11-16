/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRASHREPORTER_OSX_H__
#define CRASHREPORTER_OSX_H__

#include <Cocoa/Cocoa.h>
#include "HTTPMultipartUpload.h"
#include "crashreporter.h"
#include "json/json.h"

// Defined below
@class TextViewWithPlaceHolder;

@interface CrashReporterUI : NSObject {
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

- (void)showCrashUI:(const StringTable&)files
    queryParameters:(const Json::Value&)queryParameters
            sendURL:(const std::string&)sendURL;
- (void)showErrorUI:(const std::string&)message;
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

- (void)controlTextDidChange:(NSNotification*)note;
- (void)textDidChange:(NSNotification*)aNotification;
- (BOOL)textView:(NSTextView*)aTextView
    shouldChangeTextInRange:(NSRange)affectedCharRange
          replacementString:(NSString*)replacementString;

- (void)doInitialResizing;
- (float)setStringFitVertically:(NSControl*)control
                         string:(NSString*)str
                   resizeWindow:(BOOL)resizeWindow;
- (void)setView:(NSView*)v animate:(BOOL)animate;
- (void)enableControls:(BOOL)enabled;
- (void)updateSubmit;
- (void)updateURL;
- (void)updateEmail;
- (void)sendReport;
- (bool)setupPost;
- (void)uploadThread:(HTTPMultipartUpload*)post;
- (void)uploadComplete:(NSData*)data;

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication;
- (void)applicationWillTerminate:(NSNotification*)aNotification;

@end

/*
 * Subclass NSTextView to provide a text view with placeholder text.
 * Also provide a setEnabled implementation.
 */
@interface TextViewWithPlaceHolder : NSTextView {
  NSMutableAttributedString* mPlaceHolderString;
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
