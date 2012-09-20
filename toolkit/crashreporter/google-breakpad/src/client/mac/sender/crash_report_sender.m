// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import <pwd.h>
#import <sys/stat.h>
#import <unistd.h>

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>

#import "common/mac/HTTPMultipartUpload.h"

#import "crash_report_sender.h"
#import "common/mac/GTMLogger.h"


#define kLastSubmission @"LastSubmission"
const int kMinidumpFileLengthLimit = 800000;
const int kUserCommentsMaxLength = 1500;
const int kEmailMaxLength = 64;

#define kApplePrefsSyncExcludeAllKey \
  @"com.apple.PreferenceSync.ExcludeAllSyncKeys"

NSString *const kGoogleServerType = @"google";
NSString *const kSocorroServerType = @"socorro";
NSString *const kDefaultServerType = @"google";

#pragma mark -

@interface NSView (ResizabilityExtentions)
// Shifts the view vertically by the given amount.
- (void)breakpad_shiftVertically:(CGFloat)offset;

// Shifts the view horizontally by the given amount.
- (void)breakpad_shiftHorizontally:(CGFloat)offset;
@end

@implementation NSView (ResizabilityExtentions)
- (void)breakpad_shiftVertically:(CGFloat)offset {
  NSPoint origin = [self frame].origin;
  origin.y += offset;
  [self setFrameOrigin:origin];
}

- (void)breakpad_shiftHorizontally:(CGFloat)offset {
  NSPoint origin = [self frame].origin;
  origin.x += offset;
  [self setFrameOrigin:origin];
}
@end

@interface NSWindow (ResizabilityExtentions)
// Adjusts the window height by heightDelta relative to its current height,
// keeping all the content at the same size.
- (void)breakpad_adjustHeight:(CGFloat)heightDelta;
@end

@implementation NSWindow (ResizabilityExtentions)
- (void)breakpad_adjustHeight:(CGFloat)heightDelta {
  [[self contentView] setAutoresizesSubviews:NO];

  NSRect windowFrame = [self frame];
  windowFrame.size.height += heightDelta;
  [self setFrame:windowFrame display:YES];
  // For some reason the content view is resizing, but not adjusting its origin,
  // so correct it manually.
  [[self contentView] setFrameOrigin:NSMakePoint(0, 0)];

  [[self contentView] setAutoresizesSubviews:YES];
}
@end

@interface NSTextField (ResizabilityExtentions)
// Grows or shrinks the height of the field to the minimum required to show the
// current text, preserving the existing width and origin.
// Returns the change in height.
- (CGFloat)breakpad_adjustHeightToFit;

// Grows or shrinks the width of the field to the minimum required to show the
// current text, preserving the existing height and origin.
// Returns the change in width.
- (CGFloat)breakpad_adjustWidthToFit;
@end

@implementation NSTextField (ResizabilityExtentions)
- (CGFloat)breakpad_adjustHeightToFit {
  NSRect oldFrame = [self frame];
  // Starting with the 10.5 SDK, height won't grow, so make it huge to start.
  NSRect presizeFrame = oldFrame;
  presizeFrame.size.height = MAXFLOAT;
  // sizeToFit will blow out the width rather than making the field taller, so
  // we do it manually.
  NSSize newSize = [[self cell] cellSizeForBounds:presizeFrame];
  NSRect newFrame = NSMakeRect(oldFrame.origin.x, oldFrame.origin.y,
                               NSWidth(oldFrame), newSize.height);
  [self setFrame:newFrame];

  return newSize.height - NSHeight(oldFrame);
}

- (CGFloat)breakpad_adjustWidthToFit {
  NSRect oldFrame = [self frame];
  [self sizeToFit];
  return NSWidth([self frame]) - NSWidth(oldFrame);
}
@end

@interface NSButton (ResizabilityExtentions)
// Resizes to fit the label using IB-style size-to-fit metrics and enforcing a
// minimum width of 70, while preserving the right edge location.
// Returns the change in width.
- (CGFloat)breakpad_smartSizeToFit;
@end

@implementation NSButton (ResizabilityExtentions)
- (CGFloat)breakpad_smartSizeToFit {
  NSRect oldFrame = [self frame];
  [self sizeToFit];
  NSRect newFrame = [self frame];
  // sizeToFit gives much worse results that IB's Size to Fit option. This is
  // the amount of padding IB adds over a sizeToFit, empirically determined.
  const float kExtraPaddingAmount = 12;
  const float kMinButtonWidth = 70; // The default button size in IB.
  newFrame.size.width = NSWidth(newFrame) + kExtraPaddingAmount;
  if (NSWidth(newFrame) < kMinButtonWidth)
    newFrame.size.width = kMinButtonWidth;
  // Preserve the right edge location.
  newFrame.origin.x = NSMaxX(oldFrame) - NSWidth(newFrame);
  [self setFrame:newFrame];
  return NSWidth(newFrame) - NSWidth(oldFrame);
}
@end

#pragma mark -


@interface Reporter(PrivateMethods)
+ (uid_t)consoleUID;

- (id)initWithConfigurationFD:(int)fd;

- (NSString *)readString;
- (NSData *)readData:(ssize_t)length;

- (BOOL)readConfigurationData;
- (BOOL)readMinidumpData;
- (BOOL)readLogFileData;

// Returns YES if it has been long enough since the last report that we should
// submit a report for this crash.
- (BOOL)reportIntervalElapsed;

// Returns YES if we should send the report without asking the user first.
- (BOOL)shouldSubmitSilently;

// Returns YES if the minidump was generated on demand.
- (BOOL)isOnDemand;

// Returns YES if we should ask the user to provide comments.
- (BOOL)shouldRequestComments;

// Returns YES if we should ask the user to provide an email address.
- (BOOL)shouldRequestEmail;

// Shows UI to the user to ask for permission to send and any extra information
// we've been instructed to request. Returns YES if the user allows the report
// to be sent.
- (BOOL)askUserPermissionToSend;

// Returns the short description of the crash, suitable for use as a dialog
// title (e.g., "The application Foo has quit unexpectedly").
- (NSString*)shortDialogMessage;

// Return explanatory text about the crash and the reporter, suitable for the
// body text of a dialog.
- (NSString*)explanatoryDialogText;

// Returns the amount of time the UI should be shown before timing out.
- (NSTimeInterval)messageTimeout;

// Preps the comment-prompting alert window for display:
// * localizes all the elements
// * resizes and adjusts layout as necessary for localization
// * removes the email section if includeEmail is NO
- (void)configureAlertWindowIncludingEmail:(BOOL)includeEmail;

// Rmevoes the email section of the dialog, adjusting the rest of the window
// as necessary.
- (void)removeEmailPrompt;

// Run an alert window with the given timeout. Returns
// NSRunStoppedResponse if the timeout is exceeded. A timeout of 0
// queues the message immediately in the modal run loop.
- (NSInteger)runModalWindow:(NSWindow*)window 
                withTimeout:(NSTimeInterval)timeout;

// Returns a unique client id (user-specific), creating a persistent
// one in the user defaults, if necessary.
- (NSString*)clientID;

// Returns a dictionary that can be used to map Breakpad parameter names to
// URL parameter names.
- (NSMutableDictionary *)dictionaryForServerType:(NSString *)serverType;

// Helper method to set HTTP parameters based on server type.  This is
// called right before the upload - crashParameters will contain, on exit,
// URL parameters that should be sent with the minidump.
- (BOOL)populateServerDictionary:(NSMutableDictionary *)crashParameters;

// Initialization helper to create dictionaries mapping Breakpad
// parameters to URL parameters
- (void)createServerParameterDictionaries;

// Accessor method for the URL parameter dictionary
- (NSMutableDictionary *)urlParameterDictionary;

// This method adds a key/value pair to the dictionary that
// will be uploaded to the crash server.
- (void)addServerParameter:(id)value forKey:(NSString *)key;

// This method is used to periodically update the UI with how many
// seconds are left in the dialog display.
- (void)updateSecondsLeftInDialogDisplay:(NSTimer*)theTimer;

// When we receive this notification, it means that the user has
// begun editing the email address or comments field, and we disable
// the timers so that the user has as long as they want to type
// in their comments/email.
- (void)controlTextDidBeginEditing:(NSNotification *)aNotification;

@end

@implementation Reporter
//=============================================================================
+ (uid_t)consoleUID {
  SCDynamicStoreRef store =
    SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("Reporter"), NULL, NULL);
  uid_t uid = -2;  // Default to "nobody"
  if (store) {
    CFStringRef user = SCDynamicStoreCopyConsoleUser(store, &uid, NULL);

    if (user)
      CFRelease(user);
    else
      uid = -2;

    CFRelease(store);
  }

  return uid;
}

//=============================================================================
- (id)initWithConfigurationFD:(int)fd {
  if ((self = [super init])) {
    configFile_ = fd;
    remainingDialogTime_ = 0;
  }

  // Because the reporter is embedded in the framework (and many copies
  // of the framework may exist) its not completely certain that the OS
  // will obey the com.apple.PreferenceSync.ExcludeAllSyncKeys in our
  // Info.plist. To make sure, also set the key directly if needed.
  NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
  if (![ud boolForKey:kApplePrefsSyncExcludeAllKey]) {
    [ud setBool:YES forKey:kApplePrefsSyncExcludeAllKey];
  }

  [self createServerParameterDictionaries];

  return self;
}

//=============================================================================
- (NSString *)readString {
  NSMutableString *str = [NSMutableString stringWithCapacity:32];
  char ch[2] = { 0 };

  while (read(configFile_, &ch[0], 1) == 1) {
    if (ch[0] == '\n') {
      // Break if this is the first newline after reading some other string
      // data.
      if ([str length])
        break;
    } else {
      [str appendString:[NSString stringWithUTF8String:ch]];
    }
  }

  return str;
}

//=============================================================================
- (NSData *)readData:(ssize_t)length {
  NSMutableData *data = [NSMutableData dataWithLength:length];
  char *bytes = (char *)[data bytes];

  if (read(configFile_, bytes, length) != length)
    return nil;

  return data;
}

//=============================================================================
- (BOOL)readConfigurationData {
  parameters_ = [[NSMutableDictionary alloc] init];

  while (1) {
    NSString *key = [self readString];

    if (![key length])
      break;

    // Read the data.  Try to convert to a UTF-8 string, or just save
    // the data
    NSString *lenStr = [self readString];
    ssize_t len = [lenStr intValue];
    NSData *data = [self readData:len];
    id value = [[NSString alloc] initWithData:data
                                     encoding:NSUTF8StringEncoding];

    // If the keyname is prefixed by BREAKPAD_SERVER_PARAMETER_PREFIX
    // that indicates that it should be uploaded to the server along
    // with the minidump, so we treat it specially.
    if ([key hasPrefix:@BREAKPAD_SERVER_PARAMETER_PREFIX]) {
      NSString *urlParameterKey =
        [key substringFromIndex:[@BREAKPAD_SERVER_PARAMETER_PREFIX length]];
      if ([urlParameterKey length]) {
        if (value) {
          [self addServerParameter:value
                            forKey:urlParameterKey];
        } else {
          [self addServerParameter:data
                            forKey:urlParameterKey];
        }
      }
    } else {
      [parameters_ setObject:(value ? value : data) forKey:key];
    }
    [value release];
  }

  // generate a unique client ID based on this host's MAC address
  // then add a key/value pair for it
  NSString *clientID = [self clientID];
  [parameters_ setObject:clientID forKey:@"guid"];

  close(configFile_);
  configFile_ = -1;

  return YES;
}

// Per user per machine
- (NSString *)clientID {
  NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
  NSString *crashClientID = [ud stringForKey:kClientIdPreferenceKey];
  if (crashClientID) {
    return crashClientID;
  }

  // Otherwise, if we have no client id, generate one!
  srandom((int)[[NSDate date] timeIntervalSince1970]);
  long clientId1 = random();
  long clientId2 = random();
  long clientId3 = random();
  crashClientID = [NSString stringWithFormat:@"%x%x%x",
                            clientId1, clientId2, clientId3];

  [ud setObject:crashClientID forKey:kClientIdPreferenceKey];
  [ud synchronize];
  return crashClientID;
}

//=============================================================================
- (BOOL)readLogFileData {
  unsigned int logFileCounter = 0;

  NSString *logPath;
  size_t logFileTailSize =
      [[parameters_ objectForKey:@BREAKPAD_LOGFILE_UPLOAD_SIZE] intValue];

  NSMutableArray *logFilenames; // An array of NSString, one per log file
  logFilenames = [[NSMutableArray alloc] init];

  char tmpDirTemplate[80] = "/tmp/CrashUpload-XXXXX";
  char *tmpDir = mkdtemp(tmpDirTemplate);

  // Construct key names for the keys we expect to contain log file paths
  for(logFileCounter = 0;; logFileCounter++) {
    NSString *logFileKey = [NSString stringWithFormat:@"%@%d",
                                     @BREAKPAD_LOGFILE_KEY_PREFIX,
                                     logFileCounter];

    logPath = [parameters_ objectForKey:logFileKey];

    // They should all be consecutive, so if we don't find one, assume
    // we're done

    if (!logPath) {
      break;
    }

    NSData *entireLogFile = [[NSData alloc] initWithContentsOfFile:logPath];

    if (entireLogFile == nil) {
      continue;
    }

    NSRange fileRange;

    // Truncate the log file, only if necessary

    if ([entireLogFile length] <= logFileTailSize) {
      fileRange = NSMakeRange(0, [entireLogFile length]);
    } else {
      fileRange = NSMakeRange([entireLogFile length] - logFileTailSize,
                              logFileTailSize);
    }

    char tmpFilenameTemplate[100];

    // Generate a template based on the log filename
    sprintf(tmpFilenameTemplate,"%s/%s-XXXX", tmpDir,
            [[logPath lastPathComponent] fileSystemRepresentation]);

    char *tmpFile = mktemp(tmpFilenameTemplate);

    NSData *logSubdata = [entireLogFile subdataWithRange:fileRange];
    NSString *tmpFileString = [NSString stringWithUTF8String:tmpFile];
    [logSubdata writeToFile:tmpFileString atomically:NO];

    [logFilenames addObject:[tmpFileString lastPathComponent]];
    [entireLogFile release];
  }

  if ([logFilenames count] == 0) {
    [logFilenames release];
    logFileData_ =  nil;
    return NO;
  }

  // now, bzip all files into one
  NSTask *tarTask = [[NSTask alloc] init];

  [tarTask setCurrentDirectoryPath:[NSString stringWithUTF8String:tmpDir]];
  [tarTask setLaunchPath:@"/usr/bin/tar"];

  NSMutableArray *bzipArgs = [NSMutableArray arrayWithObjects:@"-cjvf",
                                             @"log.tar.bz2",nil];
  [bzipArgs addObjectsFromArray:logFilenames];

  [logFilenames release];

  [tarTask setArguments:bzipArgs];
  [tarTask launch];
  [tarTask waitUntilExit];
  [tarTask release];

  NSString *logTarFile = [NSString stringWithFormat:@"%s/log.tar.bz2",tmpDir];
  logFileData_ = [[NSData alloc] initWithContentsOfFile:logTarFile];
  if (logFileData_ == nil) {
    GTMLoggerDebug(@"Cannot find temp tar log file: %@", logTarFile);
    return NO;
  }
  return YES;

}

//=============================================================================
- (BOOL)readMinidumpData {
  NSString *minidumpDir = [parameters_ objectForKey:@kReporterMinidumpDirectoryKey];
  NSString *minidumpID = [parameters_ objectForKey:@kReporterMinidumpIDKey];

  if (![minidumpID length])
    return NO;

  NSString *path = [minidumpDir stringByAppendingPathComponent:minidumpID];
  path = [path stringByAppendingPathExtension:@"dmp"];

  // check the size of the minidump and limit it to a reasonable size
  // before attempting to load into memory and upload
  const char *fileName = [path fileSystemRepresentation];
  struct stat fileStatus;

  BOOL success = YES;

  if (!stat(fileName, &fileStatus)) {
    if (fileStatus.st_size > kMinidumpFileLengthLimit) {
      fprintf(stderr, "Breakpad Reporter: minidump file too large " \
              "to upload : %d\n", (int)fileStatus.st_size);
      success = NO;
    }
  } else {
      fprintf(stderr, "Breakpad Reporter: unable to determine minidump " \
              "file length\n");
      success = NO;
  }

  if (success) {
    minidumpContents_ = [[NSData alloc] initWithContentsOfFile:path];
    success = ([minidumpContents_ length] ? YES : NO);
  }

  if (!success) {
    // something wrong with the minidump file -- delete it
    unlink(fileName);
  }

  return success;
}

//=============================================================================
- (BOOL)askUserPermissionToSend {
  // Initialize Cocoa, needed to display the alert
  NSApplicationLoad();

  // Get the timeout value for the notification.
  NSTimeInterval timeout = [self messageTimeout];

  NSInteger buttonPressed = NSAlertAlternateReturn;
  // Determine whether we should create a text box for user feedback.
  if ([self shouldRequestComments]) {
    BOOL didLoadNib = [NSBundle loadNibNamed:@"Breakpad" owner:self];
    if (!didLoadNib) {
      return NO;
    }

    [self configureAlertWindowIncludingEmail:[self shouldRequestEmail]];

    buttonPressed = [self runModalWindow:alertWindow_ withTimeout:timeout];

    // Extract info from the user into the parameters_ dictionary
    if ([self commentsValue]) {
      [parameters_ setObject:[self commentsValue] forKey:@BREAKPAD_COMMENTS];
    }
    if ([self emailValue]) {
      [parameters_ setObject:[self emailValue] forKey:@BREAKPAD_EMAIL];
    }
  } else {
    // Create an alert panel to tell the user something happened
    NSPanel* alert = NSGetAlertPanel([self shortDialogMessage],
                                     [self explanatoryDialogText],
                                     NSLocalizedString(@"sendReportButton", @""),
                                     NSLocalizedString(@"cancelButton", @""),
                                     nil);

    // Pop the alert with an automatic timeout, and wait for the response
    buttonPressed = [self runModalWindow:alert withTimeout:timeout];

    // Release the panel memory
    NSReleaseAlertPanel(alert);
  }
  return buttonPressed == NSAlertDefaultReturn;
}

- (void)configureAlertWindowIncludingEmail:(BOOL)includeEmail {
  // Swap in localized values, making size adjustments to impacted elements as
  // we go. Remember that the origin is in the bottom left, so elements above
  // "fall" as text areas are shrunk from their overly-large IB sizes.

  // Localize the header. No resizing needed, as it has plenty of room.
  [dialogTitle_ setStringValue:[self shortDialogMessage]];

  // Localize the explanatory text field.
  [commentMessage_ setStringValue:[NSString stringWithFormat:@"%@\n\n%@",
                                   [self explanatoryDialogText],
                                   NSLocalizedString(@"commentsMsg", @"")]];
  CGFloat commentHeightDelta = [commentMessage_ breakpad_adjustHeightToFit];
  [headerBox_ breakpad_shiftVertically:commentHeightDelta];
  [alertWindow_ breakpad_adjustHeight:commentHeightDelta];

  // Either localize the email explanation field or remove the whole email
  // section depending on whether or not we are asking for email.
  if (includeEmail) {
    [emailMessage_ setStringValue:NSLocalizedString(@"emailMsg", @"")];
    CGFloat emailHeightDelta = [emailMessage_ breakpad_adjustHeightToFit];
    [preEmailBox_ breakpad_shiftVertically:emailHeightDelta];
    [alertWindow_ breakpad_adjustHeight:emailHeightDelta];
  } else {
    [self removeEmailPrompt];  // Handles necessary resizing.
  }

  // Localize the email label, and shift the associated text field.
  [emailLabel_ setStringValue:NSLocalizedString(@"emailLabel", @"")];
  CGFloat emailLabelWidthDelta = [emailLabel_ breakpad_adjustWidthToFit];
  [emailEntryField_ breakpad_shiftHorizontally:emailLabelWidthDelta];

  // Localize the placeholder text.
  [[commentsEntryField_ cell]
      setPlaceholderString:NSLocalizedString(@"commentsPlaceholder", @"")];
  [[emailEntryField_ cell]
      setPlaceholderString:NSLocalizedString(@"emailPlaceholder", @"")];

  // Localize the privacy policy label, and keep it right-aligned to the arrow.
  [privacyLinkLabel_ setStringValue:NSLocalizedString(@"privacyLabel", @"")];
  CGFloat privacyLabelWidthDelta = [privacyLinkLabel_ breakpad_adjustWidthToFit];
  [privacyLinkLabel_ breakpad_shiftHorizontally:(-privacyLabelWidthDelta)];

  // Localize the buttons, and keep the cancel button at the right distance.
  [sendButton_ setTitle:NSLocalizedString(@"sendReportButton", @"")];
  CGFloat sendButtonWidthDelta = [sendButton_ breakpad_smartSizeToFit];
  [cancelButton_ breakpad_shiftHorizontally:(-sendButtonWidthDelta)];
  [cancelButton_ setTitle:NSLocalizedString(@"cancelButton", @"")];
  [cancelButton_ breakpad_smartSizeToFit];
}

- (void)removeEmailPrompt {
  [emailSectionBox_ setHidden:YES];
  CGFloat emailSectionHeight = NSHeight([emailSectionBox_ frame]);
  [preEmailBox_ breakpad_shiftVertically:(-emailSectionHeight)];
  [alertWindow_ breakpad_adjustHeight:(-emailSectionHeight)];
}

- (NSInteger)runModalWindow:(NSWindow*)window 
                withTimeout:(NSTimeInterval)timeout {
  // Queue a |stopModal| message to be performed in |timeout| seconds.
  if (timeout > 0.001) {
    remainingDialogTime_ = timeout;
    SEL updateSelector = @selector(updateSecondsLeftInDialogDisplay:);
    messageTimer_ = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                     target:self
                                                   selector:updateSelector
                                                   userInfo:nil
                                                    repeats:YES];
  }

  // Run the window modally and wait for either a |stopModal| message or a
  // button click.
  [NSApp activateIgnoringOtherApps:YES];
  NSInteger returnMethod = [NSApp runModalForWindow:window];

  return returnMethod;
}

- (IBAction)sendReport:(id)sender {
  // Force the text fields to end editing so text for the currently focused
  // field will be commited.
  [alertWindow_ makeFirstResponder:alertWindow_];

  [alertWindow_ orderOut:self];
  // Use NSAlertDefaultReturn so that the return value of |runModalWithWindow|
  // matches the AppKit function NSRunAlertPanel()
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
}

// UI Button Actions
//=============================================================================
- (IBAction)cancel:(id)sender {
  [alertWindow_ orderOut:self];
  // Use NSAlertDefaultReturn so that the return value of |runModalWithWindow|
  // matches the AppKit function NSRunAlertPanel()
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
}

- (IBAction)showPrivacyPolicy:(id)sender {
  // Get the localized privacy policy URL and open it in the default browser.
  NSURL* privacyPolicyURL =
      [NSURL URLWithString:NSLocalizedString(@"privacyPolicyURL", @"")];
  [[NSWorkspace sharedWorkspace] openURL:privacyPolicyURL];
}

// Text Field Delegate Methods
//=============================================================================
- (BOOL)    control:(NSControl*)control
           textView:(NSTextView*)textView
doCommandBySelector:(SEL)commandSelector {
  BOOL result = NO;
  // If the user has entered text on the comment field, don't end
  // editing on "return".
  if (control == commentsEntryField_ &&
      commandSelector == @selector(insertNewline:)
      && [[textView string] length] > 0) {
    [textView insertNewlineIgnoringFieldEditor:self];
    result = YES;
  }
  return result;
}

- (void)controlTextDidBeginEditing:(NSNotification *)aNotification {
  [messageTimer_ invalidate];
  [self setCountdownMessage:@""];
}

- (void)updateSecondsLeftInDialogDisplay:(NSTimer*)theTimer {
  remainingDialogTime_ -= 1;

  NSString *countdownMessage;
  NSString *formatString;

  int displayedTimeLeft; // This can be either minutes or seconds.
  
  if (remainingDialogTime_ > 59) {
    // calculate minutes remaining for UI purposes
    displayedTimeLeft = (int)(remainingDialogTime_ / 60);
    
    if (displayedTimeLeft == 1) {
      formatString = NSLocalizedString(@"countdownMsgMinuteSingular", @"");
    } else {
      formatString = NSLocalizedString(@"countdownMsgMinutesPlural", @"");
    }
  } else {
    displayedTimeLeft = (int)remainingDialogTime_;
    if (displayedTimeLeft == 1) {
      formatString = NSLocalizedString(@"countdownMsgSecondSingular", @"");
    } else {
      formatString = NSLocalizedString(@"countdownMsgSecondsPlural", @"");
    }
  }
  countdownMessage = [NSString stringWithFormat:formatString,
                               displayedTimeLeft];
  if (remainingDialogTime_ <= 30) {
    [countdownLabel_ setTextColor:[NSColor redColor]];
  }
  [self setCountdownMessage:countdownMessage];
  if (remainingDialogTime_ <= 0) {
    [messageTimer_ invalidate];
    [NSApp stopModal];
  }
}



#pragma mark Accessors
#pragma mark -
//=============================================================================

- (NSString *)commentsValue {
  return [[commentsValue_ retain] autorelease];
}

- (void)setCommentsValue:(NSString *)value {
  if (commentsValue_ != value) {
    [commentsValue_ release];
    commentsValue_ = [value copy];
  }
}

- (NSString *)emailValue {
  return [[emailValue_ retain] autorelease];
}

- (void)setEmailValue:(NSString *)value {
  if (emailValue_ != value) {
    [emailValue_ release];
    emailValue_ = [value copy];
  }
}

- (NSString *)countdownMessage {
  return [[countdownMessage_ retain] autorelease];
}

- (void)setCountdownMessage:(NSString *)value {
  if (countdownMessage_ != value) {
    [countdownMessage_ release];
    countdownMessage_ = [value copy];
  }
}

#pragma mark -
//=============================================================================
- (BOOL)reportIntervalElapsed {
  float interval = [[parameters_ objectForKey:@BREAKPAD_REPORT_INTERVAL]
    floatValue];
  NSString *program = [parameters_ objectForKey:@BREAKPAD_PRODUCT];
  NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
  NSMutableDictionary *programDict =
    [NSMutableDictionary dictionaryWithDictionary:[ud dictionaryForKey:program]];
  NSNumber *lastTimeNum = [programDict objectForKey:kLastSubmission];
  NSTimeInterval lastTime = lastTimeNum ? [lastTimeNum floatValue] : 0;
  NSTimeInterval now = CFAbsoluteTimeGetCurrent();
  NSTimeInterval spanSeconds = (now - lastTime);

  [programDict setObject:[NSNumber numberWithDouble:now] 
                  forKey:kLastSubmission];
  [ud setObject:programDict forKey:program];
  [ud synchronize];

  // If we've specified an interval and we're within that time, don't ask the
  // user if we should report
  GTMLoggerDebug(@"Reporter Interval: %f", interval);
  if (interval > spanSeconds) {
    GTMLoggerDebug(@"Within throttling interval, not sending report");
    return NO;
  }
  return YES;
}

- (BOOL)isOnDemand {
  return [[parameters_ objectForKey:@BREAKPAD_ON_DEMAND]
	   isEqualToString:@"YES"];
}

- (BOOL)shouldSubmitSilently {
  return [[parameters_ objectForKey:@BREAKPAD_SKIP_CONFIRM]
            isEqualToString:@"YES"];
}

- (BOOL)shouldRequestComments {
  return [[parameters_ objectForKey:@BREAKPAD_REQUEST_COMMENTS]
            isEqualToString:@"YES"];
}

- (BOOL)shouldRequestEmail {
  return [[parameters_ objectForKey:@BREAKPAD_REQUEST_EMAIL]
            isEqualToString:@"YES"];
}

- (NSString*)shortDialogMessage {
  NSString *displayName = [parameters_ objectForKey:@BREAKPAD_PRODUCT_DISPLAY];
  if (![displayName length])
    displayName = [parameters_ objectForKey:@BREAKPAD_PRODUCT];

  if ([self isOnDemand]) {
    return [NSString
             stringWithFormat:NSLocalizedString(@"noCrashDialogHeader", @""),
             displayName];
  } else {
    return [NSString 
             stringWithFormat:NSLocalizedString(@"crashDialogHeader", @""),
             displayName];
  }
}

- (NSString*)explanatoryDialogText {
  NSString *displayName = [parameters_ objectForKey:@BREAKPAD_PRODUCT_DISPLAY];
  if (![displayName length])
    displayName = [parameters_ objectForKey:@BREAKPAD_PRODUCT];

  NSString *vendor = [parameters_ objectForKey:@BREAKPAD_VENDOR];
  if (![vendor length])
    vendor = @"unknown vendor";

  if ([self isOnDemand]) {
    return [NSString
             stringWithFormat:NSLocalizedString(@"noCrashDialogMsg", @""),
             vendor, displayName];
  } else {
    return [NSString
             stringWithFormat:NSLocalizedString(@"crashDialogMsg", @""),
             vendor];
  }
}

- (NSTimeInterval)messageTimeout {
  // Get the timeout value for the notification.
  NSTimeInterval timeout = [[parameters_ objectForKey:@BREAKPAD_CONFIRM_TIMEOUT]
                              floatValue];
  // Require a timeout of at least a minute (except 0, which means no timeout).
  if (timeout > 0.001 && timeout < 60.0) {
    timeout = 60.0;
  }
  return timeout;
}

- (void)createServerParameterDictionaries {
  serverDictionary_ = [[NSMutableDictionary alloc] init];
  socorroDictionary_ = [[NSMutableDictionary alloc] init];
  googleDictionary_ = [[NSMutableDictionary alloc] init];
  extraServerVars_ = [[NSMutableDictionary alloc] init];

  [serverDictionary_ setObject:socorroDictionary_ forKey:kSocorroServerType];
  [serverDictionary_ setObject:googleDictionary_ forKey:kGoogleServerType];

  [googleDictionary_ setObject:@"ptime" forKey:@BREAKPAD_PROCESS_UP_TIME];
  [googleDictionary_ setObject:@"email" forKey:@BREAKPAD_EMAIL];
  [googleDictionary_ setObject:@"comments" forKey:@BREAKPAD_COMMENTS];
  [googleDictionary_ setObject:@"prod" forKey:@BREAKPAD_PRODUCT];
  [googleDictionary_ setObject:@"ver" forKey:@BREAKPAD_VERSION];

  [socorroDictionary_ setObject:@"Comments" forKey:@BREAKPAD_COMMENTS];
  [socorroDictionary_ setObject:@"CrashTime"
                         forKey:@BREAKPAD_PROCESS_CRASH_TIME];
  [socorroDictionary_ setObject:@"StartupTime"
                         forKey:@BREAKPAD_PROCESS_START_TIME];
  [socorroDictionary_ setObject:@"Version"
                         forKey:@BREAKPAD_VERSION];
  [socorroDictionary_ setObject:@"ProductName"
                         forKey:@BREAKPAD_PRODUCT];
  [socorroDictionary_ setObject:@"Email"
                         forKey:@BREAKPAD_EMAIL];
}

- (NSMutableDictionary *)dictionaryForServerType:(NSString *)serverType {
  if (serverType == nil || [serverType length] == 0) {
    return [serverDictionary_ objectForKey:kDefaultServerType];
  }
  return [serverDictionary_ objectForKey:serverType];
}

- (NSMutableDictionary *)urlParameterDictionary {
  NSString *serverType = [parameters_ objectForKey:@BREAKPAD_SERVER_TYPE];
  return [self dictionaryForServerType:serverType];

}

- (BOOL)populateServerDictionary:(NSMutableDictionary *)crashParameters {
  NSDictionary *urlParameterNames = [self urlParameterDictionary];

  id key;
  NSEnumerator *enumerator = [parameters_ keyEnumerator];

  while ((key = [enumerator nextObject])) {
    // The key from parameters_ corresponds to a key in
    // urlParameterNames.  The value in parameters_ gets stored in
    // crashParameters with a key that is the value in
    // urlParameterNames.

    // For instance, if parameters_ has [PRODUCT_NAME => "FOOBAR"] and
    // urlParameterNames has [PRODUCT_NAME => "pname"] the final HTTP
    // URL parameter becomes [pname => "FOOBAR"].
    NSString *breakpadParameterName = (NSString *)key;
    NSString *urlParameter = [urlParameterNames
                                   objectForKey:breakpadParameterName];
    if (urlParameter) {
      [crashParameters setObject:[parameters_ objectForKey:key]
                          forKey:urlParameter];
    }
  }

  // Now, add the parameters that were added by the application.
  enumerator = [extraServerVars_ keyEnumerator];

  while ((key = [enumerator nextObject])) {
    NSString *urlParameterName = (NSString *)key;
    NSString *urlParameterValue =
      [extraServerVars_ objectForKey:urlParameterName];
    [crashParameters setObject:urlParameterValue
                        forKey:urlParameterName];
  }
  return YES;
}

- (void)addServerParameter:(id)value forKey:(NSString *)key {
  [extraServerVars_ setObject:value forKey:key];
}

//=============================================================================
- (void)report {
  NSURL *url = [NSURL URLWithString:[parameters_ objectForKey:@BREAKPAD_URL]];
  HTTPMultipartUpload *upload = [[HTTPMultipartUpload alloc] initWithURL:url];
  NSMutableDictionary *uploadParameters = [NSMutableDictionary dictionary];

  if (![self populateServerDictionary:uploadParameters]) {
    return;
  }

  [upload setParameters:uploadParameters];

  // Add minidump file
  if (minidumpContents_) {
    [upload addFileContents:minidumpContents_ name:@"upload_file_minidump"];

    // Send it
    NSError *error = nil;
    NSData *data = [upload send:&error];
    NSString *result = [[NSString alloc] initWithData:data
                                         encoding:NSUTF8StringEncoding];
    const char *reportID = "ERR";

    if (error) {
      fprintf(stderr, "Breakpad Reporter: Send Error: %s\n",
              [[error description] UTF8String]);
    } else {
      NSCharacterSet *trimSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
      reportID = [[result stringByTrimmingCharactersInSet:trimSet] UTF8String];
    }

    // rename the minidump file according to the id returned from the server
    NSString *minidumpDir = [parameters_ objectForKey:@kReporterMinidumpDirectoryKey];
    NSString *minidumpID = [parameters_ objectForKey:@kReporterMinidumpIDKey];

    NSString *srcString = [NSString stringWithFormat:@"%@/%@.dmp",
                                    minidumpDir, minidumpID];
    NSString *destString = [NSString stringWithFormat:@"%@/%s.dmp",
                                     minidumpDir, reportID];

    const char *src = [srcString fileSystemRepresentation];
    const char *dest = [destString fileSystemRepresentation];

    if (rename(src, dest) == 0) {
      GTMLoggerInfo(@"Breakpad Reporter: Renamed %s to %s after successful " \
                    "upload",src, dest);
    }
    else {
      // can't rename - don't worry - it's not important for users
      GTMLoggerDebug(@"Breakpad Reporter: successful upload report ID = %s\n",
                     reportID );
    }
    [result release];
  }

  if (logFileData_) {
    HTTPMultipartUpload *logUpload = [[HTTPMultipartUpload alloc] initWithURL:url];

    [uploadParameters setObject:@"log" forKey:@"type"];
    [logUpload setParameters:uploadParameters];
    [logUpload addFileContents:logFileData_ name:@"log"];

    NSError *error = nil;
    NSData *data = [logUpload send:&error];
    NSString *result = [[NSString alloc] initWithData:data
                                         encoding:NSUTF8StringEncoding];
    [result release];
    [logUpload release];
  }

  [upload release];
}

//=============================================================================
- (void)dealloc {
  [parameters_ release];
  [minidumpContents_ release];
  [logFileData_ release];
  [googleDictionary_ release];
  [socorroDictionary_ release];
  [serverDictionary_ release];
  [extraServerVars_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [emailEntryField_ setMaximumLength:kEmailMaxLength];
  [commentsEntryField_ setMaximumLength:kUserCommentsMaxLength];
}

@end

//=============================================================================
@implementation LengthLimitingTextField

- (void)setMaximumLength:(NSUInteger)maxLength {
  maximumLength_ = maxLength;
}

// This is the method we're overriding in NSTextField, which lets us
// limit the user's input if it makes the string too long.
- (BOOL)       textView:(NSTextView *)textView
shouldChangeTextInRange:(NSRange)affectedCharRange
      replacementString:(NSString *)replacementString {

  // Sometimes the range comes in invalid, so reject if we can't
  // figure out if the replacement text is too long.
  if (affectedCharRange.location == NSNotFound) {
    return NO;
  }
  // Figure out what the new string length would be, taking into
  // account user selections.
  NSUInteger newStringLength =
    [[textView string] length] - affectedCharRange.length +
    [replacementString length];
  if (newStringLength > maximumLength_) {
    return NO;
  } else {
    return YES;
  }
}

// Cut, copy, and paste have to be caught specifically since there is no menu.
- (BOOL)performKeyEquivalent:(NSEvent*)event {
  // Only handle the key equivalent if |self| is the text field with focus.
  NSText* fieldEditor = [self currentEditor];
  if (fieldEditor != nil) {
    // Check for a single "Command" modifier
    NSUInteger modifiers = [event modifierFlags];
    modifiers &= NSDeviceIndependentModifierFlagsMask;
    if (modifiers == NSCommandKeyMask) {
      // Now, check for Select All, Cut, Copy, or Paste key equivalents.
      NSString* characters = [event characters];
      // Select All is Command-A.
      if ([characters isEqualToString:@"a"]) {
        [fieldEditor selectAll:self];
        return YES;
      // Cut is Command-X.
      } else if ([characters isEqualToString:@"x"]) {
        [fieldEditor cut:self];
        return YES;
      // Copy is Command-C.
      } else if ([characters isEqualToString:@"c"]) {
        [fieldEditor copy:self];
        return YES;
      // Paste is Command-V.
      } else if ([characters isEqualToString:@"v"]) {
        [fieldEditor paste:self];
        return YES;
      }
    }
  }
  // Let the super class handle the rest (e.g. Command-Period will cancel).
  return [super performKeyEquivalent:event];
}

@end

//=============================================================================
int main(int argc, const char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#if DEBUG
  // Log to stderr in debug builds.
  [GTMLogger setSharedLogger:[GTMLogger standardLoggerWithStderr]];
#endif
  GTMLoggerDebug(@"Reporter Launched, argc=%d", argc);
  // The expectation is that there will be one argument which is the path
  // to the configuration file
  if (argc != 2) {
    exit(1);
  }

  // Open the file before (potentially) switching to console user
  int configFile = open(argv[1], O_RDONLY, 0600);

  if (configFile == -1) {
    GTMLoggerDebug(@"Couldn't open config file %s - %s",
                   argv[1],
                   strerror(errno));
  }

  // we want to avoid a build-up of old config files even if they
  // have been incorrectly written by the framework
  unlink(argv[1]);

  if (configFile == -1) {
    GTMLoggerDebug(@"Couldn't unlink config file %s - %s",
                   argv[1],
                   strerror(errno));
    exit(1);
  }

  Reporter *reporter = [[Reporter alloc] initWithConfigurationFD:configFile];

  // Gather the configuration data
  if (![reporter readConfigurationData]) {
    GTMLoggerDebug(@"reporter readConfigurationData failed");
    exit(1);
  }

  // Read the minidump into memory before we (potentially) switch from the
  // root user
  [reporter readMinidumpData];

  [reporter readLogFileData];

  // only submit a report if we have not recently crashed in the past
  BOOL shouldSubmitReport = [reporter reportIntervalElapsed];
  BOOL okayToSend = NO;

  // ask user if we should send
  if (shouldSubmitReport) {
    if ([reporter shouldSubmitSilently]) {
      GTMLoggerDebug(@"Skipping confirmation and sending report");
      okayToSend = YES;
    } else {
      okayToSend = [reporter askUserPermissionToSend];
    }
  }

  // If we're running as root, switch over to nobody
  if (getuid() == 0 || geteuid() == 0) {
    struct passwd *pw = getpwnam("nobody");

    // If we can't get a non-root uid, don't send the report
    if (!pw) {
      GTMLoggerDebug(@"!pw - %s", strerror(errno));
      exit(0);
    }

    if (setgid(pw->pw_gid) == -1) {
      GTMLoggerDebug(@"setgid(pw->pw_gid) == -1 - %s", strerror(errno));
      exit(0);
    }

    if (setuid(pw->pw_uid) == -1) {
      GTMLoggerDebug(@"setuid(pw->pw_uid) == -1 - %s", strerror(errno));
      exit(0);
    }
  }
  else {
     GTMLoggerDebug(@"getuid() !=0 || geteuid() != 0");
  }

  if (okayToSend && shouldSubmitReport) {
    GTMLoggerDebug(@"Sending Report");
    [reporter report];
    GTMLoggerDebug(@"Report Sent!");
  } else {
    GTMLoggerDebug(@"Not sending crash report okayToSend=%d, "\
                     "shouldSubmitReport=%d", okayToSend, shouldSubmitReport);
  }

  GTMLoggerDebug(@"Exiting with no errors");
  // Cleanup
  [reporter release];
  [pool release];
  return 0;
}
