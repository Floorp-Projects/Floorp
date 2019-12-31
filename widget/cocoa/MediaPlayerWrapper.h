/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_COCOA_MEDIAPLAYERWRAPPER_H_
#define WIDGET_COCOA_MEDIAPLAYERWRAPPER_H_

#import <Foundation/Foundation.h>
#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

bool MediaPlayerWrapperInit();
void MediaPlayerWrapperClose();

typedef NS_ENUM(NSUInteger, MPNowPlayingPlaybackState) {
  MPNowPlayingPlaybackStateUnknown = 0,
  MPNowPlayingPlaybackStatePlaying,
  MPNowPlayingPlaybackStatePaused,
  MPNowPlayingPlaybackStateStopped,
  MPNowPlayingPlaybackStateInterrupted
};

typedef NS_ENUM(NSInteger, MPRemoteCommandHandlerStatus) {
  /// There was no error executing the requested command.
  MPRemoteCommandHandlerStatusSuccess = 0,

  /// The command could not be executed because the requested content does not
  /// exist in the current application state.
  MPRemoteCommandHandlerStatusNoSuchContent = 100,

  /// The command could not be executed because there is no now playing item
  /// available that is required for this command. As an example, an
  /// application would return this error code if an "enable language option"
  /// command is received, but nothing is currently playing.
  MPRemoteCommandHandlerStatusNoActionableNowPlayingItem = 110,

  /// The command could not be executed because a device required
  /// is not available. For instance, if headphones are required, or if a watch
  /// app realizes that it needs the companion to fulfull a request.
  MPRemoteCommandHandlerStatusDeviceNotFound = 120,

  /// The command could not be executed for another reason.
  MPRemoteCommandHandlerStatusCommandFailed = 200
};

NS_ASSUME_NONNULL_BEGIN

@interface MPNowPlayingInfoCenter : NSObject

/// Returns the default now playing info center.
/// The default center holds now playing info about the current application.
+ (MPNowPlayingInfoCenter*)defaultCenter;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// The current now playing info for the center.
/// Setting the info to nil will clear it.
@property(nonatomic, copy, nullable) NSDictionary<NSString*, id>* nowPlayingInfo;

/// The current playback state of the app.
/// This only applies on macOS, where playback state cannot be determined by
/// the application's audio session. This property must be set every time
/// the app begins or halts playback, otherwise remote control functionality may
/// not work as expected.
@property(nonatomic) MPNowPlayingPlaybackState playbackState;

@end

@class MPRemoteCommand;

@interface MPRemoteCommandEvent : NSObject

/// The command that sent the event.
@property(nonatomic, readonly) MPRemoteCommand* command;

/// The time when the event occurred.
@property(nonatomic, readonly) NSTimeInterval timestamp;

@end

@interface MPRemoteCommand : NSObject

/// Whether a button (for example) should be enabled and tappable for this
/// particular command.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Target-action style for adding handlers to commands.
// Actions receive an MPRemoteCommandEvent as the first parameter.
// Targets are not retained by addTarget:action:, and should be removed from the
// command when the target is deallocated.
//
// Your selector should return a MPRemoteCommandHandlerStatus value when
// possible. This allows the system to respond appropriately to commands that
// may not have been able to be executed in accordance with the application's
// current state.
- (void)addTarget:(id)target action:(SEL)action;
- (void)removeTarget:(id)target action:(nullable SEL)action;
- (void)removeTarget:(nullable id)target;

/// Returns an opaque object to act as the target.
- (id)addTargetWithHandler:(MPRemoteCommandHandlerStatus (^)(MPRemoteCommandEvent* event))handler;

@end

@interface MPRemoteCommandCenter : NSObject

// Playback Commands
@property(nonatomic, readonly) MPRemoteCommand* pauseCommand;
@property(nonatomic, readonly) MPRemoteCommand* playCommand;
@property(nonatomic, readonly) MPRemoteCommand* stopCommand;
@property(nonatomic, readonly) MPRemoteCommand* togglePlayPauseCommand;

// Previous/Next Track Commands
@property(nonatomic, readonly) MPRemoteCommand* nextTrackCommand;
@property(nonatomic, readonly) MPRemoteCommand* previousTrackCommand;

+ (MPRemoteCommandCenter*)sharedCommandCenter;

@end

NS_ASSUME_NONNULL_END

#endif  // WIDGET_COCOA_MEDIAPLAYERWRAPPER_H_
