/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#include "crashreporter.h"
#include "crashreporter_osx.h"
#include <crt_externs.h>
#include <spawn.h>
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
static StringTable gFiles;
static Json::Value gQueryParameters;
static string gURLParameter;
static string gSendURL;
static vector<string> gRestartArgs;
static bool gDidTrySend = false;
static bool gRTLlayout = false;

static cpu_type_t pref_cpu_types[2] = {
#if defined(__i386__)
    CPU_TYPE_X86,
#elif defined(__x86_64__)
    CPU_TYPE_X86_64,
#elif defined(__ppc__)
    CPU_TYPE_POWERPC,
#elif defined(__aarch64__)
    CPU_TYPE_ARM64,
#endif
    CPU_TYPE_ANY};

#define NSSTR(s) [NSString stringWithUTF8String:(s).c_str()]

static NSString* Str(const char* aName) {
  string str = gStrings[aName];
  if (str.empty()) str = "?";
  return NSSTR(str);
}

static bool RestartApplication() {
  vector<char*> argv(gRestartArgs.size() + 1);

  posix_spawnattr_t spawnattr;
  if (posix_spawnattr_init(&spawnattr) != 0) {
    return false;
  }

  // Set spawn attributes.
  size_t attr_count = sizeof(pref_cpu_types) / sizeof(pref_cpu_types[0]);
  size_t attr_ocount = 0;
  if (posix_spawnattr_setbinpref_np(&spawnattr, attr_count, pref_cpu_types, &attr_ocount) != 0 ||
      attr_ocount != attr_count) {
    posix_spawnattr_destroy(&spawnattr);
    return false;
  }

  unsigned int i;
  for (i = 0; i < gRestartArgs.size(); i++) {
    argv[i] = (char*)gRestartArgs[i].c_str();
  }
  argv[i] = 0;

  char** env = NULL;
  char*** nsEnv = _NSGetEnviron();
  if (nsEnv) env = *nsEnv;
  int result = posix_spawnp(NULL, argv[0], NULL, &spawnattr, &argv[0], env);

  posix_spawnattr_destroy(&spawnattr);

  return result == 0;
}

@implementation CrashReporterUI

- (void)awakeFromNib {
  gUI = self;
  [mWindow center];

  [mWindow setTitle:[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"]];
  [NSApp activateIgnoringOtherApps:YES];
}

- (void)showCrashUI:(const StringTable&)files
    queryParameters:(const Json::Value&)queryParameters
            sendURL:(const string&)sendURL {
  gFiles = files;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;

  if (gAutoSubmit) {
    gDidTrySend = true;
    [self sendReport];
    return;
  }

  [mWindow setTitle:Str(ST_CRASHREPORTERTITLE)];
  [mHeaderLabel setStringValue:Str(ST_CRASHREPORTERHEADER)];

  NSRect viewReportFrame = [mViewReportButton frame];
  [mViewReportButton setTitle:Str(ST_VIEWREPORT)];
  [mViewReportButton sizeToFit];
  if (gRTLlayout) {
    // sizeToFit will keep the left side fixed, so realign
    float oldWidth = viewReportFrame.size.width;
    viewReportFrame = [mViewReportButton frame];
    viewReportFrame.origin.x += oldWidth - viewReportFrame.size.width;
    [mViewReportButton setFrame:viewReportFrame];
  }

  [mSubmitReportButton setTitle:Str(ST_CHECKSUBMIT)];
  [mIncludeURLButton setTitle:Str(ST_CHECKURL)];
  [mViewReportOkButton setTitle:Str(ST_OK)];

  [mCommentText setPlaceholder:Str(ST_COMMENTGRAYTEXT)];
  if (gRTLlayout) [mCommentText toggleBaseWritingDirection:self];

  if (gQueryParameters.isMember("URL")) {
    // save the URL value in case the checkbox gets unchecked
    gURLParameter = gQueryParameters["URL"].asString();
  } else {
    // no URL specified, hide checkbox
    [mIncludeURLButton removeFromSuperview];
    // shrink window to fit
    NSRect frame = [mWindow frame];
    NSRect includeURLFrame = [mIncludeURLButton frame];
    NSRect emailFrame = [mEmailMeButton frame];
    int buttonMask = [mViewReportButton autoresizingMask];
    int checkMask = [mSubmitReportButton autoresizingMask];
    int commentScrollMask = [mCommentScrollView autoresizingMask];

    [mViewReportButton setAutoresizingMask:NSViewMinYMargin];
    [mSubmitReportButton setAutoresizingMask:NSViewMinYMargin];
    [mCommentScrollView setAutoresizingMask:NSViewMinYMargin];

    // remove all the space in between
    frame.size.height -= includeURLFrame.origin.y - emailFrame.origin.y;
    [mWindow setFrame:frame display:true animate:NO];

    [mViewReportButton setAutoresizingMask:buttonMask];
    [mSubmitReportButton setAutoresizingMask:checkMask];
    [mCommentScrollView setAutoresizingMask:commentScrollMask];
  }

  // resize some buttons horizontally and possibly some controls vertically
  [self doInitialResizing];

  // load default state of submit checkbox
  // we don't just do this via IB because we want the default to be
  // off a certain percentage of the time
  BOOL submitChecked = YES;
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  if (nil != [userDefaults objectForKey:@"submitReport"]) {
    submitChecked = [userDefaults boolForKey:@"submitReport"];
  } else {
    [userDefaults setBool:submitChecked forKey:@"submitReport"];
  }
  [mSubmitReportButton setState:(submitChecked ? NSOnState : NSOffState)];

  [self updateSubmit];
  [self updateURL];
  [self updateEmail];

  [mWindow makeKeyAndOrderFront:nil];
}

- (void)showErrorUI:(const string&)message {
  [self setView:mErrorView animate:NO];

  [mErrorHeaderLabel setStringValue:Str(ST_CRASHREPORTERHEADER)];
  [self setStringFitVertically:mErrorLabel string:NSSTR(message) resizeWindow:YES];
  [mErrorCloseButton setTitle:Str(ST_OK)];

  [mErrorCloseButton setKeyEquivalent:@"\r"];
  [mWindow makeFirstResponder:mErrorCloseButton];
  [mWindow makeKeyAndOrderFront:nil];
}

- (void)showReportInfo {
  NSDictionary* boldAttr =
      [NSDictionary dictionaryWithObject:[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]
                                  forKey:NSFontAttributeName];
  NSDictionary* normalAttr =
      [NSDictionary dictionaryWithObject:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]
                                  forKey:NSFontAttributeName];

  [mViewReportTextView setString:@""];
  for (Json::ValueConstIterator iter = gQueryParameters.begin(); iter != gQueryParameters.end();
       ++iter) {
    NSAttributedString* key = [[NSAttributedString alloc] initWithString:NSSTR(iter.name() + ": ")
                                                              attributes:boldAttr];
    string str;
    if (iter->isString()) {
      str = iter->asString();
    } else {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      str = writeString(builder, *iter);
    }
    NSAttributedString* value = [[NSAttributedString alloc] initWithString:NSSTR(str + "\n")
                                                                attributes:normalAttr];
    [[mViewReportTextView textStorage] appendAttributedString:key];
    [[mViewReportTextView textStorage] appendAttributedString:value];
    [key release];
    [value release];
  }

  NSAttributedString* extra =
      [[NSAttributedString alloc] initWithString:NSSTR("\n" + gStrings[ST_EXTRAREPORTINFO])
                                      attributes:normalAttr];
  [[mViewReportTextView textStorage] appendAttributedString:extra];
  [extra release];
}

- (void)maybeSubmitReport {
  if ([mSubmitReportButton state] == NSOnState) {
    [self setStringFitVertically:mProgressText string:Str(ST_REPORTDURINGSUBMIT) resizeWindow:YES];
    // disable all the controls
    [self enableControls:NO];
    [mSubmitReportButton setEnabled:NO];
    [mRestartButton setEnabled:NO];
    [mCloseButton setEnabled:NO];
    [mProgressIndicator startAnimation:self];
    gDidTrySend = true;
    [self sendReport];
  } else {
    [NSApp terminate:self];
  }
}

- (void)closeMeDown:(id)unused {
  [NSApp terminate:self];
}

- (IBAction)submitReportClicked:(id)sender {
  [self updateSubmit];
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setBool:([mSubmitReportButton state] == NSOnState) forKey:@"submitReport"];
  [userDefaults synchronize];
}

- (IBAction)viewReportClicked:(id)sender {
  [self showReportInfo];
  [NSApp beginSheet:mViewReportWindow
      modalForWindow:mWindow
       modalDelegate:nil
      didEndSelector:nil
         contextInfo:nil];
}

- (IBAction)viewReportOkClicked:(id)sender {
  [mViewReportWindow orderOut:nil];
  [NSApp endSheet:mViewReportWindow];
}

- (IBAction)closeClicked:(id)sender {
  [self maybeSubmitReport];
}

- (IBAction)restartClicked:(id)sender {
  RestartApplication();
  [self maybeSubmitReport];
}

- (IBAction)includeURLClicked:(id)sender {
  [self updateURL];
}

- (void)textDidChange:(NSNotification*)aNotification {
  // update comment parameter
  if ([[[mCommentText textStorage] mutableString] length] > 0)
    gQueryParameters["Comments"] = [[[mCommentText textStorage] mutableString] UTF8String];
  else
    gQueryParameters.removeMember("Comments");
}

// Limit the comment field to 500 bytes in UTF-8
- (BOOL)textView:(NSTextView*)aTextView
    shouldChangeTextInRange:(NSRange)affectedCharRange
          replacementString:(NSString*)replacementString {
  // current string length + replacement text length - replaced range length
  if (([[aTextView string] lengthOfBytesUsingEncoding:NSUTF8StringEncoding] +
       [replacementString lengthOfBytesUsingEncoding:NSUTF8StringEncoding] -
       [[[aTextView string] substringWithRange:affectedCharRange]
           lengthOfBytesUsingEncoding:NSUTF8StringEncoding]) > MAX_COMMENT_LENGTH) {
    return NO;
  }
  return YES;
}

- (void)doInitialResizing {
  NSRect windowFrame = [mWindow frame];
  NSRect restartFrame = [mRestartButton frame];
  NSRect closeFrame = [mCloseButton frame];
  // resize close button to fit text
  float oldCloseWidth = closeFrame.size.width;
  [mCloseButton setTitle:Str(ST_QUIT)];
  [mCloseButton sizeToFit];
  closeFrame = [mCloseButton frame];
  // move close button left if it grew
  if (!gRTLlayout) {
    closeFrame.origin.x -= closeFrame.size.width - oldCloseWidth;
  }

  if (gRestartArgs.size() == 0) {
    [mRestartButton removeFromSuperview];
    if (!gRTLlayout) {
      closeFrame.origin.x =
          restartFrame.origin.x + (restartFrame.size.width - closeFrame.size.width);
    } else {
      closeFrame.origin.x = restartFrame.origin.x;
    }
    [mCloseButton setFrame:closeFrame];
    [mCloseButton setKeyEquivalent:@"\r"];
  } else {
    [mRestartButton setTitle:Str(ST_RESTART)];
    // resize "restart" button
    float oldRestartWidth = restartFrame.size.width;
    [mRestartButton sizeToFit];
    restartFrame = [mRestartButton frame];
    if (!gRTLlayout) {
      // move left by the amount that the button grew
      restartFrame.origin.x -= restartFrame.size.width - oldRestartWidth;
      closeFrame.origin.x -= restartFrame.size.width - oldRestartWidth;
    } else {
      // shift the close button right in RTL
      closeFrame.origin.x += restartFrame.size.width - oldRestartWidth;
    }
    [mRestartButton setFrame:restartFrame];
    [mCloseButton setFrame:closeFrame];
    // possibly resize window if both buttons no longer fit
    // leave 20 px from either side of the window, and 12 px
    // between the buttons
    float neededWidth = closeFrame.size.width + restartFrame.size.width + 2 * 20 + 12;

    if (neededWidth > windowFrame.size.width) {
      windowFrame.size.width = neededWidth;
      [mWindow setFrame:windowFrame display:true animate:NO];
    }
    [mRestartButton setKeyEquivalent:@"\r"];
  }

  NSButton* checkboxes[] = {mSubmitReportButton, mIncludeURLButton};

  for (auto checkbox : checkboxes) {
    NSRect frame = [checkbox frame];
    [checkbox sizeToFit];
    if (gRTLlayout) {
      // sizeToFit will keep the left side fixed, so realign
      float oldWidth = frame.size.width;
      frame = [checkbox frame];
      frame.origin.x += oldWidth - frame.size.width;
      [checkbox setFrame:frame];
    }
    // keep existing spacing on left side, + 20 px spare on right
    float neededWidth = frame.origin.x + frame.size.width + 20;
    if (neededWidth > windowFrame.size.width) {
      windowFrame.size.width = neededWidth;
      [mWindow setFrame:windowFrame display:true animate:NO];
    }
  }

  // do this down here because we may have made the window wider
  // up above
  [self setStringFitVertically:mDescriptionLabel
                        string:Str(ST_CRASHREPORTERDESCRIPTION)
                  resizeWindow:YES];

  // now pin all the controls (except quit/submit) in place,
  // if we lengthen the window after this, it's just to lengthen
  // the progress text, so nothing above that text should move.
  NSView* views[] = {mSubmitReportButton, mViewReportButton,  mCommentScrollView,
                     mIncludeURLButton,   mProgressIndicator, mProgressText};
  for (auto view : views) {
    [view setAutoresizingMask:NSViewMinYMargin];
  }
}

- (float)setStringFitVertically:(NSControl*)control
                         string:(NSString*)str
                   resizeWindow:(BOOL)resizeWindow {
  // hack to make the text field grow vertically
  NSRect frame = [control frame];
  float oldHeight = frame.size.height;

  frame.size.height = 10000;
  NSSize oldCellSize = [[control cell] cellSizeForBounds:frame];
  [control setStringValue:str];
  NSSize newCellSize = [[control cell] cellSizeForBounds:frame];

  float delta = newCellSize.height - oldCellSize.height;
  frame.origin.y -= delta;
  frame.size.height = oldHeight + delta;
  [control setFrame:frame];

  if (resizeWindow) {
    NSRect frame = [mWindow frame];
    frame.origin.y -= delta;
    frame.size.height += delta;
    [mWindow setFrame:frame display:true animate:NO];
  }

  return delta;
}

- (void)setView:(NSView*)v animate:(BOOL)animate {
  NSRect frame = [mWindow frame];

  NSRect oldViewFrame = [[mWindow contentView] frame];
  NSRect newViewFrame = [v frame];

  frame.origin.y += oldViewFrame.size.height - newViewFrame.size.height;
  frame.size.height -= oldViewFrame.size.height - newViewFrame.size.height;

  frame.origin.x += oldViewFrame.size.width - newViewFrame.size.width;
  frame.size.width -= oldViewFrame.size.width - newViewFrame.size.width;

  [mWindow setContentView:v];
  [mWindow setFrame:frame display:true animate:animate];
}

- (void)enableControls:(BOOL)enabled {
  [mViewReportButton setEnabled:enabled];
  [mIncludeURLButton setEnabled:enabled];
  [mCommentText setEnabled:enabled];
  [mCommentScrollView setHasVerticalScroller:enabled];
}

- (void)updateSubmit {
  if ([mSubmitReportButton state] == NSOnState) {
    [self setStringFitVertically:mProgressText string:Str(ST_REPORTPRESUBMIT) resizeWindow:YES];
    [mProgressText setHidden:NO];
    // enable all the controls
    [self enableControls:YES];
  } else {
    // not submitting, disable all the controls under
    // the submit checkbox, and hide the status text
    [mProgressText setHidden:YES];
    [self enableControls:NO];
  }
}

- (void)updateURL {
  if ([mIncludeURLButton state] == NSOnState && !gURLParameter.empty()) {
    gQueryParameters["URL"] = gURLParameter;
  } else {
    gQueryParameters.removeMember("URL");
  }
}

- (void)updateEmail {
  // In order to remove the email fields, we have to edit the .nib files which
  // we can't do with current xcode so we make them hidden; updating the
  // crashreporter interface for mac is covered in bug #1696164
  [mEmailMeButton setHidden:YES];
  [mEmailText setHidden:YES];
}

- (void)sendReport {
  if (![self setupPost]) {
    LogMessage("Crash report submission failed: could not set up POST data");

    if (gAutoSubmit) {
      [NSApp terminate:self];
    }

    [self setStringFitVertically:mProgressText string:Str(ST_SUBMITFAILED) resizeWindow:YES];
    // quit after 5 seconds
    [self performSelector:@selector(closeMeDown:) withObject:nil afterDelay:5.0];
  }

  [NSThread detachNewThreadSelector:@selector(uploadThread:) toTarget:self withObject:mPost];
}

- (bool)setupPost {
  NSURL* url =
      [NSURL URLWithString:[NSSTR(gSendURL)
                               stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
  if (!url) return false;

  mPost = [[HTTPMultipartUpload alloc] initWithURL:url];
  if (!mPost) return false;

  for (StringTable::const_iterator i = gFiles.begin(); i != gFiles.end(); i++) {
    [mPost addFileAtPath:NSSTR(i->second) name:NSSTR(i->first)];
  }

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  string output = writeString(builder, gQueryParameters).append("\r\n");
  NSMutableString* parameters = [[NSMutableString alloc] initWithUTF8String:output.c_str()];

  [mPost setParameters:parameters];
  [parameters release];

  return true;
}

- (void)uploadComplete:(NSData*)data {
  NSHTTPURLResponse* response = [mPost response];
  [mPost release];

  bool success;
  string reply;
  if (!data || !response || [response statusCode] != 200) {
    success = false;
    reply = "";

    // if data is nil, we probably logged an error in uploadThread
    if (data != nil && response != nil) {
      ostringstream message;
      message << "Crash report submission failed: server returned status " << [response statusCode];
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
    NSString* r = [[NSString alloc] initWithData:data encoding:encoding];
    reply = [r UTF8String];
    [r release];
  }

  SendCompleted(success, reply);

  if (gAutoSubmit) {
    [NSApp terminate:self];
  }

  [mProgressIndicator stopAnimation:self];
  if (success) {
    [self setStringFitVertically:mProgressText string:Str(ST_REPORTSUBMITSUCCESS) resizeWindow:YES];
  } else {
    [self setStringFitVertically:mProgressText string:Str(ST_SUBMITFAILED) resizeWindow:YES];
  }
  // quit after 5 seconds
  [self performSelector:@selector(closeMeDown:) withObject:nil afterDelay:5.0];
}

- (void)uploadThread:(HTTPMultipartUpload*)post {
  NSAutoreleasePool* autoreleasepool = [[NSAutoreleasePool alloc] init];
  NSError* error = nil;
  NSData* data = [post send:&error];
  if (error) {
    data = nil;
    NSString* errorDesc = [error localizedDescription];
    string message = [errorDesc UTF8String];
    LogMessage("Crash report submission failed: " + message);
  }

  [self performSelectorOnMainThread:@selector(uploadComplete:) withObject:data waitUntilDone:YES];

  [autoreleasepool release];
}

// to get auto-quit when we close the window
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication {
  return YES;
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  // since we use [NSApp terminate:] we never return to main,
  // so do our cleanup here
  if (!gDidTrySend) DeleteDump();
}

@end

@implementation TextViewWithPlaceHolder

- (BOOL)becomeFirstResponder {
  [self setNeedsDisplay:YES];
  return [super becomeFirstResponder];
}

- (void)drawRect:(NSRect)rect {
  [super drawRect:rect];
  if (mPlaceHolderString && [[self string] isEqualToString:@""] &&
      self != [[self window] firstResponder])
    [mPlaceHolderString drawInRect:[self frame]];
}

- (BOOL)resignFirstResponder {
  [self setNeedsDisplay:YES];
  return [super resignFirstResponder];
}

- (void)setPlaceholder:(NSString*)placeholder {
  NSColor* txtColor = [NSColor disabledControlTextColor];
  NSDictionary* txtDict =
      [NSDictionary dictionaryWithObjectsAndKeys:txtColor, NSForegroundColorAttributeName, nil];
  mPlaceHolderString = [[NSMutableAttributedString alloc] initWithString:placeholder
                                                              attributes:txtDict];
  if (gRTLlayout)
    [mPlaceHolderString setAlignment:NSTextAlignmentRight
                               range:NSMakeRange(0, [placeholder length])];
}

- (void)insertTab:(id)sender {
  // don't actually want to insert tabs, just tab to next control
  [[self window] selectNextKeyView:sender];
}

- (void)insertBacktab:(id)sender {
  [[self window] selectPreviousKeyView:sender];
}

- (void)setEnabled:(BOOL)enabled {
  [self setSelectable:enabled];
  [self setEditable:enabled];
  if (![[self string] isEqualToString:@""]) {
    NSAttributedString* colorString;
    NSColor* txtColor;
    if (enabled)
      txtColor = [NSColor textColor];
    else
      txtColor = [NSColor disabledControlTextColor];
    NSDictionary* txtDict =
        [NSDictionary dictionaryWithObjectsAndKeys:txtColor, NSForegroundColorAttributeName, nil];
    colorString = [[NSAttributedString alloc] initWithString:[self string] attributes:txtDict];
    [[self textStorage] setAttributedString:colorString];
    [self setInsertionPointColor:txtColor];
    [colorString release];
  }
}

- (void)dealloc {
  [mPlaceHolderString release];
  [super dealloc];
}

@end

/* === Crashreporter UI Functions === */

bool UIInit() {
  gMainPool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];

  if (gStrings.find("isRTL") != gStrings.end() && gStrings["isRTL"] == "yes") gRTLlayout = true;

  if (gAutoSubmit) {
    gUI = [[CrashReporterUI alloc] init];
  } else {
    [[NSBundle mainBundle] loadNibNamed:(gRTLlayout ? @"MainMenuRTL" : @"MainMenu")
                                  owner:NSApp
                        topLevelObjects:nil];
  }

  return true;
}

void UIShutdown() { [gMainPool release]; }

void UIShowDefaultUI() {
  [gUI showErrorUI:gStrings[ST_CRASHREPORTERDEFAULT]];
  [NSApp run];
}

bool UIShowCrashUI(const StringTable& files, const Json::Value& queryParameters,
                   const string& sendURL, const vector<string>& restartArgs) {
  gRestartArgs = restartArgs;

  [gUI showCrashUI:files queryParameters:queryParameters sendURL:sendURL];
  [NSApp run];

  return gDidTrySend;
}

void UIError_impl(const string& message) {
  if (!gUI) {
    // UI failed to initialize, printing is the best we can do
    printf("Error: %s\n", message.c_str());
    return;
  }

  [gUI showErrorUI:message];
  [NSApp run];
}

bool UIGetIniPath(string& path) {
  NSString* tmpPath = [NSString stringWithUTF8String:gArgv[0]];
  NSString* iniName = [tmpPath lastPathComponent];
  iniName = [iniName stringByAppendingPathExtension:@"ini"];
  tmpPath = [tmpPath stringByDeletingLastPathComponent];
  tmpPath = [tmpPath stringByDeletingLastPathComponent];
  tmpPath = [tmpPath stringByAppendingPathComponent:@"Resources"];
  tmpPath = [tmpPath stringByAppendingPathComponent:iniName];
  path = [tmpPath UTF8String];
  return true;
}

bool UIGetSettingsPath(const string& vendor, const string& product, string& settingsPath) {
  FSRef foundRef;
  OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &foundRef);
  if (err != noErr) return false;

  unsigned char path[PATH_MAX];
  FSRefMakePath(&foundRef, path, sizeof(path));
  NSString* destPath = [NSString stringWithUTF8String:reinterpret_cast<char*>(path)];

  // Note that MacOS ignores the vendor when creating the profile hierarchy -
  // all application preferences directories live alongside one another in
  // ~/Library/Application Support/
  destPath = [destPath stringByAppendingPathComponent:NSSTR(product)];
  // Thunderbird stores its profile in ~/Library/Thunderbird,
  // but we're going to put stuff in ~/Library/Application Support/Thunderbird
  // anyway, so we have to ensure that path exists.
  string tempPath = [destPath UTF8String];
  if (!UIEnsurePathExists(tempPath)) return false;

  destPath = [destPath stringByAppendingPathComponent:@"Crash Reports"];

  settingsPath = [destPath UTF8String];

  return true;
}

bool UIMoveFile(const string& file, const string& newfile) {
  if (!rename(file.c_str(), newfile.c_str())) return true;
  if (errno != EXDEV) return false;

  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* source = [fileManager stringWithFileSystemRepresentation:file.c_str()
                                                              length:file.length()];
  NSString* dest = [fileManager stringWithFileSystemRepresentation:newfile.c_str()
                                                            length:newfile.length()];
  if (!source || !dest) return false;

  [fileManager moveItemAtPath:source toPath:dest error:NULL];
  return UIFileExists(newfile);
}
