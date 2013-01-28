//
//  GrowlPreferencesController.h
//  Growl
//
//  Created by Nelson Elhage on 8/24/04.
//  Renamed from GrowlPreferences.h by Mac-arena the Bored Zo on 2005-06-27.
//  Copyright 2004-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

#ifndef GROWL_PREFERENCES_CONTROLLER_H
#define GROWL_PREFERENCES_CONTROLLER_H

#ifdef __OBJC__
#define XSTR(x) (@x)
#else
#define XSTR CFSTR
#endif

#define GrowlPreferencesChanged		XSTR("GrowlPreferencesChanged")
#define GrowlPreview				XSTR("GrowlPreview")
#define GrowlDisplayPluginKey		XSTR("GrowlDisplayPluginName")
#define GrowlUserDefaultsKey		XSTR("GrowlUserDefaults")
#define GrowlStartServerKey			XSTR("GrowlStartServer")
#define GrowlRemoteRegistrationKey	XSTR("GrowlRemoteRegistration")
#define GrowlEnableForwardKey		XSTR("GrowlEnableForward")
#define GrowlForwardDestinationsKey	XSTR("GrowlForwardDestinations")
#define GrowlUDPPortKey				XSTR("GrowlUDPPort")
#define GrowlTCPPortKey				XSTR("GrowlTCPPort")
#define GrowlUpdateCheckKey			XSTR("GrowlUpdateCheck")
#define LastUpdateCheckKey			XSTR("LastUpdateCheck")
#define	GrowlLoggingEnabledKey		XSTR("GrowlLoggingEnabled")
#define	GrowlLogTypeKey				XSTR("GrowlLogType")
#define	GrowlCustomHistKey1			XSTR("Custom log history 1")
#define	GrowlCustomHistKey2			XSTR("Custom log history 2")
#define	GrowlCustomHistKey3			XSTR("Custom log history 3")
#define GrowlMenuExtraKey			XSTR("GrowlMenuExtra")
#define GrowlSquelchModeKey			XSTR("GrowlSquelchMode")
#define LastKnownVersionKey			XSTR("LastKnownVersion")
#define GrowlStickyWhenAwayKey		XSTR("StickyWhenAway")
#define GrowlStickyIdleThresholdKey	XSTR("IdleThreshold")

CFTypeRef GrowlPreferencesController_objectForKey(CFTypeRef key);
CFIndex   GrowlPreferencesController_integerForKey(CFTypeRef key);
Boolean   GrowlPreferencesController_boolForKey(CFTypeRef key);
unsigned short GrowlPreferencesController_unsignedShortForKey(CFTypeRef key);

#ifdef __OBJC__

#import "GrowlAbstractSingletonObject.h"

@interface GrowlPreferencesController : GrowlAbstractSingletonObject {
	void *loginItems;
}

+ (GrowlPreferencesController *) sharedController;

- (void) registerDefaults:(NSDictionary *)inDefaults;
- (id) objectForKey:(NSString *)key;
- (void) setObject:(id)object forKey:(NSString *)key;
- (BOOL) boolForKey:(NSString*) key;
- (void) setBool:(BOOL)value forKey:(NSString *)key;
- (CFIndex) integerForKey:(NSString *)key;
- (void) setInteger:(CFIndex)value forKey:(NSString *)key;
- (void) synchronize;

- (BOOL) shouldStartGrowlAtLogin;
- (void) setShouldStartGrowlAtLogin:(BOOL)flag;
- (void) setStartAtLogin:(NSString *)path enabled:(BOOL)flag;

- (BOOL) isRunning:(NSString *)theBundleIdentifier;
- (BOOL) isGrowlRunning;
- (void) setGrowlRunning:(BOOL)flag noMatterWhat:(BOOL)nmw;
- (void) launchGrowl:(BOOL)noMatterWhat;
- (void) terminateGrowl;

#pragma mark GrowlMenu running state

- (void) enableGrowlMenu;
- (void) disableGrowlMenu;

#pragma mark -
//Simplified accessors

#pragma mark UI

- (BOOL) isBackgroundUpdateCheckEnabled;
- (void) setIsBackgroundUpdateCheckEnabled:(BOOL)flag;

- (NSString *) defaultDisplayPluginName;
- (void) setDefaultDisplayPluginName:(NSString *)name;

- (BOOL) squelchMode;
- (void) setSquelchMode:(BOOL)flag;

- (BOOL) stickyWhenAway;
- (void) setStickyWhenAway:(BOOL)flag;

- (NSNumber*) idleThreshold;
- (void) setIdleThreshold:(NSNumber*)value;

#pragma mark GrowlMenu methods

- (BOOL) isGrowlMenuEnabled;
- (void) setGrowlMenuEnabled:(BOOL)state;

#pragma mark "Network" tab pane

- (BOOL) isGrowlServerEnabled;
- (void) setGrowlServerEnabled:(BOOL)enabled;

- (BOOL) isRemoteRegistrationAllowed;
- (void) setRemoteRegistrationAllowed:(BOOL)flag;

- (BOOL) isForwardingEnabled;
- (void) setForwardingEnabled:(BOOL)enabled;

- (NSString *) remotePassword;
- (void) setRemotePassword:(NSString *)value;

- (unsigned short) UDPPort;
- (void) setUDPPort:(unsigned short)value;

@end

#endif

#endif
