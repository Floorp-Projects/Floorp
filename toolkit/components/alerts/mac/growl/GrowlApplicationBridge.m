//
//  GrowlApplicationBridge.m
//  Growl
//
//  Created by Evan Schoenberg on Wed Jun 16 2004.
//  Copyright 2004-2006 The Growl Project. All rights reserved.
//

#import "GrowlApplicationBridge.h"
#ifdef GROWL_WITH_INSTALLER
#import "GrowlInstallationPrompt.h"
#import "GrowlVersionUtilities.h"
#endif
#include "CFGrowlAdditions.h"
#include "CFURLAdditions.h"
#include "CFMutableDictionaryAdditions.h"
#import "GrowlDefinesInternal.h"
#import "GrowlPathUtilities.h"
#import "GrowlPathway.h"

#import <ApplicationServices/ApplicationServices.h>


/*!
 * The 10.3+ exception handling can only work if -fobjc-exceptions is enabled
 */
#if 0
	#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3
	# define TRY		@try {
	# define ENDTRY		}
	# define CATCH		@catch(NSException *localException) {
	# define ENDCATCH	}
	#else
	# define TRY		NS_DURING
	# define ENDTRY
	# define CATCH		NS_HANDLER
	# define ENDCATCH	NS_ENDHANDLER
	#endif
#else
	# define TRY		NS_DURING
	# define ENDTRY
	# define CATCH		NS_HANDLER
	# define ENDCATCH	NS_ENDHANDLER
#endif

@interface GrowlApplicationBridge (PRIVATE)
/*!
 *	@method launchGrowlIfInstalled
 *	@abstract Launches GrowlHelperApp.
 *	@discussion Launches the GrowlHelperApp if it's not already running.
 *	 GROWL_IS_READY will be posted to the distributed notification center
 *	 once it is ready.
 *
 *	 Uses <code>+_launchGrowlIfInstalledWithRegistrationDictionary:</code>.
 *	@result Returns YES if GrowlHelperApp began launching or was already running, NO if Growl isn't installed
 */
+ (BOOL) launchGrowlIfInstalled;

/*!
 *	@method _launchGrowlIfInstalledWithRegistrationDictionary:
 *	@abstract Launches GrowlHelperApp and registers.
 *	@discussion Launches the GrowlHelperApp if it's not already running, and passes it a registration dictionary.
 *	 If Growl is turned on in the Growl prefpane, GROWL_IS_READY will be posted
 *	 to the distributed notification center when Growl is listening for
 *	 notifications.
 *	@param regDict The dictionary with which to register.
 *	@result Returns YES if GrowlHelperApp began launching or was already running, NO if Growl isn't installed
 */
+ (BOOL) _launchGrowlIfInstalledWithRegistrationDictionary:(NSDictionary *)regDict;

#ifdef GROWL_WITH_INSTALLER
+ (void) _checkForPackagedUpdateForGrowlPrefPaneBundle:(NSBundle *)growlPrefPaneBundle;
#endif

/*!	@method	_applicationNameForGrowlSearchingRegistrationDictionary:
 *	@abstract Obtain the name of the current application.
 *	@param regDict	The dictionary to search, or <code>nil</code> not to.
 *	@result	The name of the current application.
 *	@discussion	Does not call +bestRegistrationDictionary, and is therefore safe to call from it.
 */
+ (NSString *) _applicationNameForGrowlSearchingRegistrationDictionary:(NSDictionary *)regDict;
/*!	@method	_applicationNameForGrowlSearchingRegistrationDictionary:
 *	@abstract Obtain the icon of the current application.
 *	@param regDict	The dictionary to search, or <code>nil</code> not to.
 *	@result	The icon of the current application, in IconFamily format (same as is used in 'icns' resources and .icns files).
 *	@discussion	Does not call +bestRegistrationDictionary, and is therefore safe to call from it.
 */
+ (NSData *) _applicationIconDataForGrowlSearchingRegistrationDictionary:(NSDictionary *)regDict;

/*! @method growlProxy
 *  @abstract Obtain (creating a connection if needed) a proxy to the Growl Helper Application
 */
+ (NSProxy<GrowlNotificationProtocol> *) growlProxy;
@end

static NSDictionary *cachedRegistrationDictionary = nil;
static NSString	*appName = nil;
static NSData	*appIconData = nil;

static id		delegate = nil;
static BOOL		growlLaunched = NO;
static NSProxy<GrowlNotificationProtocol> *growlProxy = nil;

#ifdef GROWL_WITH_INSTALLER
static NSMutableArray	*queuedGrowlNotifications = nil;

static BOOL				userChoseNotToInstallGrowl = NO;
static BOOL				promptedToInstallGrowl = NO;
static BOOL				promptedToUpgradeGrowl = NO;
#endif

//used primarily by GIP, but could be useful elsewhere.
static BOOL		registerWhenGrowlIsReady = NO;

#pragma mark -

@implementation GrowlApplicationBridge

+ (void) setGrowlDelegate:(NSObject<GrowlApplicationBridgeDelegate> *)inDelegate {
	NSDistributedNotificationCenter *NSDNC = [NSDistributedNotificationCenter defaultCenter];

	if (inDelegate != delegate) {
		[delegate release];
		delegate = [inDelegate retain];
	}

	[cachedRegistrationDictionary release];
	cachedRegistrationDictionary = [[self bestRegistrationDictionary] retain];

	//Cache the appName from the delegate or the process name
	[appName autorelease];
	appName = [[self _applicationNameForGrowlSearchingRegistrationDictionary:cachedRegistrationDictionary] retain];
	if (!appName) {
		NSLog(@"%@", @"GrowlApplicationBridge: Cannot register because the application name was not supplied and could not be determined");
		return;
	}

	/* Cache the appIconData from the delegate if it responds to the
	 * applicationIconDataForGrowl selector, or the application if not
	 */
	[appIconData autorelease];
	appIconData = [[self _applicationIconDataForGrowlSearchingRegistrationDictionary:cachedRegistrationDictionary] retain];

	//Add the observer for GROWL_IS_READY which will be triggered later if all goes well
	[NSDNC addObserver:self
			  selector:@selector(_growlIsReady:)
				  name:GROWL_IS_READY
				object:nil];

	/* Watch for notification clicks if our delegate responds to the
	 * growlNotificationWasClicked: selector. Notifications will come in on a
	 * unique notification name based on our app name, pid and
	 * GROWL_NOTIFICATION_CLICKED.
	 */
	int pid = [[NSProcessInfo processInfo] processIdentifier];
	NSString *growlNotificationClickedName = [[NSString alloc] initWithFormat:@"%@-%d-%@",
		appName, pid, GROWL_NOTIFICATION_CLICKED];
	if ([delegate respondsToSelector:@selector(growlNotificationWasClicked:)])
		[NSDNC addObserver:self
				  selector:@selector(growlNotificationWasClicked:)
					  name:growlNotificationClickedName
					object:nil];
	else
		[NSDNC removeObserver:self
						 name:growlNotificationClickedName
					   object:nil];
	[growlNotificationClickedName release];

	NSString *growlNotificationTimedOutName = [[NSString alloc] initWithFormat:@"%@-%d-%@",
		appName, pid, GROWL_NOTIFICATION_TIMED_OUT];
	if ([delegate respondsToSelector:@selector(growlNotificationTimedOut:)])
		[NSDNC addObserver:self
				  selector:@selector(growlNotificationTimedOut:)
					  name:growlNotificationTimedOutName
					object:nil];
	else
		[NSDNC removeObserver:self
						 name:growlNotificationTimedOutName
					   object:nil];
	[growlNotificationTimedOutName release];

#ifdef GROWL_WITH_INSTALLER
	//Determine if the user has previously told us not to ever request installation again
	userChoseNotToInstallGrowl = [[NSUserDefaults standardUserDefaults] boolForKey:@"Growl Installation:Do Not Prompt Again"];
#endif

	growlLaunched = [self _launchGrowlIfInstalledWithRegistrationDictionary:cachedRegistrationDictionary];
}

+ (NSObject<GrowlApplicationBridgeDelegate> *) growlDelegate {
	return delegate;
}

#pragma mark -

+ (void) notifyWithTitle:(NSString *)title
			 description:(NSString *)description
		notificationName:(NSString *)notifName
				iconData:(NSData *)iconData
				priority:(int)priority
				isSticky:(BOOL)isSticky
			clickContext:(id)clickContext
{
	[GrowlApplicationBridge notifyWithTitle:title
								description:description
						   notificationName:notifName
								   iconData:iconData
								   priority:priority
								   isSticky:isSticky
							   clickContext:clickContext
								 identifier:nil];
}

/* Send a notification to Growl for display.
 * title, description, and notifName are required.
 * All other id parameters may be nil to accept defaults.
 * priority is 0 by default; isSticky is NO by default.
 */
+ (void) notifyWithTitle:(NSString *)title
			 description:(NSString *)description
		notificationName:(NSString *)notifName
				iconData:(NSData *)iconData
				priority:(int)priority
				isSticky:(BOOL)isSticky
			clickContext:(id)clickContext
			  identifier:(NSString *)identifier
{
	NSParameterAssert(notifName);	//Notification name is required.
	NSParameterAssert(title || description);	//At least one of title or description is required.

	// Build our noteDict from all passed parameters
	NSMutableDictionary *noteDict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
		notifName,	 GROWL_NOTIFICATION_NAME,
		nil];

	if (title)			setObjectForKey(noteDict, GROWL_NOTIFICATION_TITLE, title);
	if (description)	setObjectForKey(noteDict, GROWL_NOTIFICATION_DESCRIPTION, description);
	if (iconData)		setObjectForKey(noteDict, GROWL_NOTIFICATION_ICON, iconData);
	if (clickContext)	setObjectForKey(noteDict, GROWL_NOTIFICATION_CLICK_CONTEXT, clickContext);
	if (priority)		setIntegerForKey(noteDict, GROWL_NOTIFICATION_PRIORITY, priority);
	if (isSticky)		setBooleanForKey(noteDict, GROWL_NOTIFICATION_STICKY, isSticky);
	if (identifier)		setObjectForKey(noteDict, GROWL_NOTIFICATION_IDENTIFIER, identifier);

	[self notifyWithDictionary:noteDict];
	[noteDict release];
}

+ (void) notifyWithDictionary:(NSDictionary *)userInfo {
	//post it.
	if (growlLaunched) {		
		NSProxy<GrowlNotificationProtocol> *currentGrowlProxy = [self growlProxy];

		//Make sure we have everything that we need (that we can retrieve from the registration dictionary).
		userInfo = [self notificationDictionaryByFillingInDictionary:userInfo];

		if (currentGrowlProxy) {
			//Post to Growl via GrowlApplicationBridgePathway
			TRY
				[currentGrowlProxy postNotificationWithDictionary:userInfo];
			ENDTRY
			CATCH
				NSLog(@"GrowlApplicationBridge: exception while sending notification: %@", localException);
			ENDCATCH
		} else {
			//NSLog(@"GrowlApplicationBridge: could not find local GrowlApplicationBridgePathway, falling back to NSDistributedNotificationCenter");

			//DNC needs a plist. this means we must pass data, not an NSImage.
			Class     NSImageClass = [NSImage class];
			NSImage          *icon = [userInfo objectForKey:GROWL_NOTIFICATION_ICON];
			NSImage       *appIcon = [userInfo objectForKey:GROWL_NOTIFICATION_APP_ICON];
			BOOL       iconIsImage =    icon &&    [icon isKindOfClass:NSImageClass];
			BOOL    appIconIsImage = appIcon && [appIcon isKindOfClass:NSImageClass];
			if (iconIsImage || appIconIsImage) {
				NSMutableDictionary *mUserInfo = [userInfo mutableCopy];
				//notification icon.
				if (iconIsImage)
					[mUserInfo setObject:[icon TIFFRepresentation] forKey:GROWL_NOTIFICATION_ICON];
				//per-notification application icon.
				if (appIconIsImage)
					[mUserInfo setObject:[appIcon TIFFRepresentation] forKey:GROWL_NOTIFICATION_APP_ICON];

				userInfo = [mUserInfo autorelease];
			}

			//Post to Growl via NSDistributedNotificationCenter
			[[NSDistributedNotificationCenter defaultCenter] postNotificationName:GROWL_NOTIFICATION
																		   object:nil
																		 userInfo:userInfo
															   deliverImmediately:NO];
		}
	} else {
#ifdef GROWL_WITH_INSTALLER
		/*if Growl launches, and the user hasn't already said NO to installing
		 *	it, store this notification for posting
		 */
		if (!([self isGrowlInstalled] || userChoseNotToInstallGrowl)) {
			if (!queuedGrowlNotifications)
				queuedGrowlNotifications = [[NSMutableArray alloc] init];
			[queuedGrowlNotifications addObject:userInfo];

			//if we have not already asked the user to install Growl, do it now
			if (!promptedToInstallGrowl) {
				[GrowlInstallationPrompt showInstallationPrompt];
				promptedToInstallGrowl = YES;
			}
		}
#endif
	}
}

#pragma mark -

+ (BOOL) isGrowlInstalled {
	return ([GrowlPathUtilities growlPrefPaneBundle] != nil);
}

+ (BOOL) isGrowlRunning {
	BOOL growlIsRunning = NO;
	ProcessSerialNumber PSN = { kNoProcess, kNoProcess };

	while (GetNextProcess(&PSN) == noErr) {
		CFDictionaryRef infoDict = ProcessInformationCopyDictionary(&PSN, kProcessDictionaryIncludeAllInformationMask);
		CFStringRef bundleId = CFDictionaryGetValue(infoDict, kCFBundleIdentifierKey);

		if (bundleId && CFStringCompare(bundleId, CFSTR("com.Growl.GrowlHelperApp"), 0) == kCFCompareEqualTo) {
			growlIsRunning = YES;
			CFRelease(infoDict);
			break;
		}
		CFRelease(infoDict);
	}

	return growlIsRunning;
}

+ (void) displayInstallationPromptIfNeeded {
#ifdef GROWL_WITH_INSTALLER
    //if we have not already asked the user to install Growl, do it now
    if (!promptedToInstallGrowl) {
        [GrowlInstallationPrompt showInstallationPrompt];
        promptedToInstallGrowl = YES;
    }
#endif
}

#pragma mark -

+ (BOOL) registerWithDictionary:(NSDictionary *)regDict {
	if (regDict)
		regDict = [self registrationDictionaryByFillingInDictionary:regDict];
	else
		regDict = [self bestRegistrationDictionary];

	[cachedRegistrationDictionary release];
	cachedRegistrationDictionary = [regDict retain];

	return [self _launchGrowlIfInstalledWithRegistrationDictionary:regDict];
}

+ (void) reregisterGrowlNotifications {
	[self registerWithDictionary:nil];
}

+ (void) setWillRegisterWhenGrowlIsReady:(BOOL)flag {
	registerWhenGrowlIsReady = flag;
}
+ (BOOL) willRegisterWhenGrowlIsReady {
	return registerWhenGrowlIsReady;
}

#pragma mark -

+ (NSDictionary *) registrationDictionaryFromDelegate {
	NSDictionary *regDict = nil;

	if (delegate && [delegate respondsToSelector:@selector(registrationDictionaryForGrowl)])
		regDict = [delegate registrationDictionaryForGrowl];

	return regDict;
}

+ (NSDictionary *) registrationDictionaryFromBundle:(NSBundle *)bundle {
	if (!bundle) bundle = [NSBundle mainBundle];

	NSDictionary *regDict = nil;

	NSString *regDictPath = [bundle pathForResource:@"Growl Registration Ticket" ofType:GROWL_REG_DICT_EXTENSION];
	if (regDictPath) {
		regDict = [NSDictionary dictionaryWithContentsOfFile:regDictPath];
		if (!regDict)
			NSLog(@"GrowlApplicationBridge: The bundle at %@ contains a registration dictionary, but it is not a valid property list. Please tell this application's developer.", [bundle bundlePath]);
	}

	return regDict;
}

+ (NSDictionary *) bestRegistrationDictionary {
	NSDictionary *registrationDictionary = [self registrationDictionaryFromDelegate];
	if (!registrationDictionary) {
		registrationDictionary = [self registrationDictionaryFromBundle:nil];
		if (!registrationDictionary)
			NSLog(@"GrowlApplicationBridge: The Growl delegate did not supply a registration dictionary, and the app bundle at %@ does not have one. Please tell this application's developer.", [[NSBundle mainBundle] bundlePath]);
	}

	return [self registrationDictionaryByFillingInDictionary:registrationDictionary];
}

#pragma mark -

+ (NSDictionary *) registrationDictionaryByFillingInDictionary:(NSDictionary *)regDict {
	return [self registrationDictionaryByFillingInDictionary:regDict restrictToKeys:nil];
}

+ (NSDictionary *) registrationDictionaryByFillingInDictionary:(NSDictionary *)regDict restrictToKeys:(NSSet *)keys {
	if (!regDict) return nil;

	NSMutableDictionary *mRegDict = [regDict mutableCopy];

	if ((!keys) || [keys containsObject:GROWL_APP_NAME]) {
		if (![mRegDict objectForKey:GROWL_APP_NAME]) {
			if (!appName)
				appName = [[self _applicationNameForGrowlSearchingRegistrationDictionary:regDict] retain];

			[mRegDict setObject:appName
			             forKey:GROWL_APP_NAME];
		}
	}

	if ((!keys) || [keys containsObject:GROWL_APP_ICON]) {
		if (![mRegDict objectForKey:GROWL_APP_ICON]) {
			if (!appIconData)
				appIconData = [[self _applicationIconDataForGrowlSearchingRegistrationDictionary:regDict] retain];
			if (appIconData)
				[mRegDict setObject:appIconData forKey:GROWL_APP_ICON];
		}
	}

	if ((!keys) || [keys containsObject:GROWL_APP_LOCATION]) {
		if (![mRegDict objectForKey:GROWL_APP_LOCATION]) {
			NSURL *myURL = copyCurrentProcessURL();
			if (myURL) {
				NSDictionary *file_data = createDockDescriptionWithURL(myURL);
				if (file_data) {
					NSDictionary *location = [[NSDictionary alloc] initWithObjectsAndKeys:file_data, @"file-data", nil];
					[file_data release];
					[mRegDict setObject:location forKey:GROWL_APP_LOCATION];
					[location release];
				} else {
					[mRegDict removeObjectForKey:GROWL_APP_LOCATION];
				}
				[NSMakeCollectable(myURL) release];
			}
		}
	}

	if ((!keys) || [keys containsObject:GROWL_NOTIFICATIONS_DEFAULT]) {
		if (![mRegDict objectForKey:GROWL_NOTIFICATIONS_DEFAULT]) {
			NSArray *all = [mRegDict objectForKey:GROWL_NOTIFICATIONS_ALL];
			if (all)
				[mRegDict setObject:all forKey:GROWL_NOTIFICATIONS_DEFAULT];
		}
	}

	if ((!keys) || [keys containsObject:GROWL_APP_ID])
		if (![mRegDict objectForKey:GROWL_APP_ID])
			[mRegDict setObject:(NSString *)CFBundleGetIdentifier(CFBundleGetMainBundle()) forKey:GROWL_APP_ID];

	return [mRegDict autorelease];
}

+ (NSDictionary *) notificationDictionaryByFillingInDictionary:(NSDictionary *)notifDict {
	NSMutableDictionary *mNotifDict = [notifDict mutableCopy];

	if (![mNotifDict objectForKey:GROWL_APP_NAME]) {
		if (!appName)
			appName = [[self _applicationNameForGrowlSearchingRegistrationDictionary:cachedRegistrationDictionary] retain];

		if (appName) {
			[mNotifDict setObject:appName
			               forKey:GROWL_APP_NAME];
		}
	}

	if (![mNotifDict objectForKey:GROWL_APP_ICON]) {
		if (!appIconData)
			appIconData = [[self _applicationIconDataForGrowlSearchingRegistrationDictionary:cachedRegistrationDictionary] retain];

		if (appIconData) {
			[mNotifDict setObject:appIconData
			               forKey:GROWL_APP_ICON];
		}
	}

	//Only include the PID when there's a click context. We do this because NSDNC imposes a 15-MiB limit on the serialized notification, and we wouldn't want to overrun it because of a 4-byte PID.
	if ([mNotifDict objectForKey:GROWL_NOTIFICATION_CLICK_CONTEXT] && ![mNotifDict objectForKey:GROWL_APP_PID]) {
		NSNumber *pidNum = [[NSNumber alloc] initWithInt:[[NSProcessInfo processInfo] processIdentifier]];

		[mNotifDict setObject:pidNum
		               forKey:GROWL_APP_PID];

		[pidNum release];
	}

	return [mNotifDict autorelease];
}

+ (NSDictionary *) frameworkInfoDictionary {
#ifdef GROWL_WITH_INSTALLER
	return (NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetBundleWithIdentifier(CFSTR("com.growl.growlwithinstallerframework")));
#else
	return (NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetBundleWithIdentifier(CFSTR("com.growl.growlframework")));
#endif
}

#pragma mark -
#pragma mark Private methods

+ (NSString *) _applicationNameForGrowlSearchingRegistrationDictionary:(NSDictionary *)regDict {
	NSString *applicationNameForGrowl = nil;

	if (delegate && [delegate respondsToSelector:@selector(applicationNameForGrowl)])
		applicationNameForGrowl = [delegate applicationNameForGrowl];

	if (!applicationNameForGrowl) {
		applicationNameForGrowl = [regDict objectForKey:GROWL_APP_NAME];

		if (!applicationNameForGrowl)
			applicationNameForGrowl = [[NSProcessInfo processInfo] processName];
	}

	return applicationNameForGrowl;
}
+ (NSData *) _applicationIconDataForGrowlSearchingRegistrationDictionary:(NSDictionary *)regDict {
	NSData *iconData = nil;

	if (delegate) {
		if ([delegate respondsToSelector:@selector(applicationIconForGrowl)])
			iconData = (NSData *)[delegate applicationIconForGrowl];
		else if ([delegate respondsToSelector:@selector(applicationIconDataForGrowl)])
			iconData = [delegate applicationIconDataForGrowl];
	}

	if (!iconData)
		iconData = [regDict objectForKey:GROWL_APP_ICON];

	if (iconData && [iconData isKindOfClass:[NSImage class]])
		iconData = [(NSImage *)iconData TIFFRepresentation];

	if (!iconData) {
		NSURL *URL = copyCurrentProcessURL();
		iconData = [copyIconDataForURL(URL) autorelease];
		[NSMakeCollectable(URL) release];
	}

	return iconData;
}

/*Selector called when a growl notification is clicked.  This should never be
 *	called manually, and the calling observer should only be registered if the
 *	delegate responds to growlNotificationWasClicked:.
 */
+ (void) growlNotificationWasClicked:(NSNotification *)notification {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[delegate growlNotificationWasClicked:
		[[notification userInfo] objectForKey:GROWL_KEY_CLICKED_CONTEXT]];
	[pool drain];
}
+ (void) growlNotificationTimedOut:(NSNotification *)notification {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[delegate growlNotificationTimedOut:
		[[notification userInfo] objectForKey:GROWL_KEY_CLICKED_CONTEXT]];
	[pool drain];
}

#pragma mark -

//When a connection dies, release our reference to its proxy
+ (void) connectionDidDie:(NSNotification *)notification {
	[[NSNotificationCenter defaultCenter] removeObserver:self
													name:NSConnectionDidDieNotification
												  object:[notification object]];
	[growlProxy release]; growlProxy = nil;
}

+ (NSProxy<GrowlNotificationProtocol> *) growlProxy {
	if (!growlProxy) {
		NSConnection *connection = [NSConnection connectionWithRegisteredName:@"GrowlApplicationBridgePathway" host:nil];
		if (connection) {
			[[NSNotificationCenter defaultCenter] addObserver:self
													 selector:@selector(connectionDidDie:)
														 name:NSConnectionDidDieNotification
													   object:connection];
			
			TRY
			{
				NSDistantObject *theProxy = [connection rootProxy];
				if ([theProxy respondsToSelector:@selector(registerApplicationWithDictionary:)]) {
					[theProxy setProtocolForProxy:@protocol(GrowlNotificationProtocol)];
					growlProxy = [(NSProxy<GrowlNotificationProtocol> *)theProxy retain];
				} else {
					NSLog(@"Received a fake GrowlApplicationBridgePathway object. Some other application is interfering with Growl, or something went horribly wrong. Please file a bug report.");
					growlProxy = nil;
				}
			}
			ENDTRY
				CATCH
			{
				NSLog(@"GrowlApplicationBridge: exception while sending notification: %@", localException);
				growlProxy = nil;
			}
			ENDCATCH
		}
	}
	
	return growlProxy;
}

+ (void) _growlIsReady:(NSNotification *)notification {
#pragma unused(notification)
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	//Growl has now launched; we may get here with (growlLaunched == NO) when the user first installs
	growlLaunched = YES;

	//Inform our delegate if it is interested
	if ([delegate respondsToSelector:@selector(growlIsReady)])
		[delegate growlIsReady];

	//Post a notification locally
	[[NSNotificationCenter defaultCenter] postNotificationName:GROWL_IS_READY
														object:nil
													  userInfo:nil];

	//Stop observing for GROWL_IS_READY
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self
															   name:GROWL_IS_READY
															 object:nil];

	//register (fixes #102: this is necessary if we got here by Growl having just been installed)
	if (registerWhenGrowlIsReady) {
		[self reregisterGrowlNotifications];
		registerWhenGrowlIsReady = NO;
	}

#ifdef GROWL_WITH_INSTALLER
	//Perform any queued notifications
	NSEnumerator *enumerator = [queuedGrowlNotifications objectEnumerator];
	NSDictionary *noteDict;

	//Configure the growl proxy if it isn't currently configured
	NSProxy<GrowlNotificationProtocol> *currentGrowlProxy = [self growlProxy];
	
	while ((noteDict = [enumerator nextObject])) {		
		if (currentGrowlProxy) {
			//Post to Growl via GrowlApplicationBridgePathway
			NS_DURING
				[currentGrowlProxy postNotificationWithDictionary:noteDict];
			NS_HANDLER
				NSLog(@"GrowlApplicationBridge: exception while sending notification: %@", localException);
			NS_ENDHANDLER
		} else {
			//Post to Growl via NSDistributedNotificationCenter
			NSLog(@"GrowlApplicationBridge: could not find local GrowlApplicationBridgePathway, falling back to NSDistributedNotificationCenter");
			[[NSDistributedNotificationCenter defaultCenter] postNotificationName:GROWL_NOTIFICATION
																		   object:NULL
																		 userInfo:noteDict
															   deliverImmediately:FALSE];
		}
	}

	[queuedGrowlNotifications release]; queuedGrowlNotifications = nil;
#endif
	
	[pool drain];
}

#ifdef GROWL_WITH_INSTALLER
/*Sent to us by GrowlInstallationPrompt if the user clicks Cancel so we can
 *	avoid prompting again this session (or ever if they checked Don't Ask Again)
 */
+ (void) _userChoseNotToInstallGrowl {
	//Note the user's action so we stop queueing notifications, etc.
	userChoseNotToInstallGrowl = YES;

	//Clear our queued notifications; we won't be needing them
	[queuedGrowlNotifications release]; queuedGrowlNotifications = nil;
}

// Check against our current version number and ensure the installed Growl pane is the same or later
+ (void) _checkForPackagedUpdateForGrowlPrefPaneBundle:(NSBundle *)growlPrefPaneBundle {
	NSString *ourGrowlPrefPaneInfoPath;
	NSDictionary *infoDictionary;
	NSString *packagedVersion, *installedVersion;
	BOOL upgradeIsAvailable;

	ourGrowlPrefPaneInfoPath = [[NSBundle bundleWithIdentifier:@"com.growl.growlwithinstallerframework"] pathForResource:@"GrowlPrefPaneInfo"
																												  ofType:@"plist"];

	NSObject *infoPropertyList = createPropertyListFromURL([NSURL fileURLWithPath:ourGrowlPrefPaneInfoPath],
														   kCFPropertyListImmutable,
														   /* outFormat */ NULL, /* outErrorString */ NULL);
	NSDictionary *infoDict = ([infoPropertyList isKindOfClass:[NSDictionary class]] ? (NSDictionary *)infoPropertyList : nil);

	packagedVersion = [infoDict objectForKey:(NSString *)kCFBundleVersionKey];

	infoDictionary = [growlPrefPaneBundle infoDictionary];
	installedVersion = [infoDictionary objectForKey:(NSString *)kCFBundleVersionKey];

	//If the installed version is earlier than our packaged version, we can offer an upgrade.
	upgradeIsAvailable = (compareVersionStringsTranslating1_0To0_5(packagedVersion, installedVersion) == kCFCompareGreaterThan);
	if (upgradeIsAvailable && !promptedToUpgradeGrowl) {
		NSString	*lastDoNotPromptVersion = [[NSUserDefaults standardUserDefaults] objectForKey:@"Growl Update:Do Not Prompt Again:Last Version"];

		if (!lastDoNotPromptVersion ||
			(compareVersionStringsTranslating1_0To0_5(packagedVersion, lastDoNotPromptVersion) == kCFCompareGreaterThan))
		{
			[GrowlInstallationPrompt showUpdatePromptForVersion:packagedVersion];
			promptedToUpgradeGrowl = YES;
		}
	}
	[infoDict release];
}
#endif

#pragma mark -

+ (OSStatus) getPSN:(struct ProcessSerialNumber *)outAppPSN forAppWithBundleAtPath:(NSString *)appPath {
	OSStatus err;
	while ((err = GetNextProcess(outAppPSN)) == noErr) {
		NSDictionary *dict = [NSMakeCollectable(ProcessInformationCopyDictionary(outAppPSN, kProcessDictionaryIncludeAllInformationMask)) autorelease];
		NSString *bundlePath = [dict objectForKey:@"BundlePath"];
		if ([bundlePath isEqualToString:appPath]) {
			//Match!
			break;
		}
	}
	return err;
}

+ (BOOL) launchApplicationWithBundleAtPath:(NSString *)appPath openDocumentURL:(NSURL *)regItemURL {
	const struct LSLaunchURLSpec spec = {
		.appURL = (CFURLRef)[NSURL fileURLWithPath:appPath],
		.itemURLs = (CFArrayRef)(regItemURL ? [NSArray arrayWithObject:regItemURL] : nil),
		.passThruParams = NULL,
		.launchFlags = kLSLaunchDontAddToRecents | kLSLaunchDontSwitch | kLSLaunchNoParams | kLSLaunchAsync,
		.asyncRefCon = NULL
	};
	OSStatus err = LSOpenFromURLSpec(&spec, NULL);
	if (err != noErr) {
		NSLog(@"Could not launch application at path %@ (with document %@) because LSOpenFromURLSpec returned %i (%s)", appPath, regItemURL, err, GetMacOSStatusCommentString(err));
	}
	return (err == noErr);
}
+ (BOOL) sendOpenEventToProcessWithProcessSerialNumber:(struct ProcessSerialNumber *)appPSN openDocumentURL:(NSURL *)regItemURL {
	OSStatus err;
	BOOL success = NO;
	AEStreamRef stream = AEStreamCreateEvent(kCoreEventClass, kAEOpenDocuments,
											 //Target application
											 typeProcessSerialNumber, appPSN, sizeof(*appPSN),
											 kAutoGenerateReturnID, kAnyTransactionID);
	if (!stream) {
		NSLog(@"%@: Could not create open-document event to register this application with Growl", [self class]);
	} else {
		if (regItemURL) {
			NSString *regItemURLString = [regItemURL absoluteString];
			NSData *regItemURLUTF8Data = [regItemURLString dataUsingEncoding:NSUTF8StringEncoding];
			err = AEStreamWriteKeyDesc(stream, keyDirectObject, typeFileURL, [regItemURLUTF8Data bytes], [regItemURLUTF8Data length]);
			if (err != noErr) {
				NSLog(@"%@: Could not set direct object of open-document event to register this application with Growl because AEStreamWriteKeyDesc returned %li/%s", [self class], (long)err, GetMacOSStatusCommentString(err));
			}
		}

		AppleEvent event;
		err = AEStreamClose(stream, &event);
		if (err != noErr) {
			NSLog(@"%@: Could not finish open-document event to register this application with Growl because AEStreamClose returned %li/%s", [self class], (long)err, GetMacOSStatusCommentString(err));
		} else {
			err = AESendMessage(&event, /*reply*/ NULL, kAENoReply | kAEDontReconnect | kAENeverInteract | kAEDontRecord, kAEDefaultTimeout);
			if (err != noErr) {
				NSLog(@"%@: Could not send open-document event to register this application with Growl because AESend returned %li/%s", [self class], (long)err, GetMacOSStatusCommentString(err));
			}

			AEDisposeDesc(&event);
		}

		success = (err == noErr);
	}

	return success;
}
+ (BOOL) _launchGrowlIfInstalledWithRegistrationDictionary:(NSDictionary *)regDict {
	BOOL success = NO;
	NSBundle *growlPrefPaneBundle = nil;
	NSString *growlHelperAppPath;

	//First look for a running GHA. It might not actually be within a Growl prefpane bundle.
	growlHelperAppPath = [[GrowlPathUtilities runningHelperAppBundle] bundlePath];
	if (growlHelperAppPath) {
		//The GHA bundle path should be: .../Growl.prefPane/Contents/Resources/GrowlHelperApp.app
		NSArray *growlHelperAppPathComponents = [growlHelperAppPath pathComponents];
		NSString *growlPrefPaneBundlePath = [NSString pathWithComponents:[growlHelperAppPathComponents subarrayWithRange:(NSRange){ 0UL, [growlHelperAppPathComponents count] - 3UL }]];
		growlPrefPaneBundle = [NSBundle bundleWithPath:growlPrefPaneBundlePath];
		//Make sure we actually got a Growl.prefPane and not, say, a Growl project folder. (NSBundle can be liberal in its acceptance of a directory as a bundle at times.)
		if (![[growlPrefPaneBundle bundleIdentifier] isEqualToString:GROWL_PREFPANE_BUNDLE_IDENTIFIER])
			growlPrefPaneBundle = nil;
	}
	//If we didn't get a Growl prefpane bundle by finding the bundle that contained GHA, look it up directly.
	if (!growlPrefPaneBundle) {
		growlPrefPaneBundle = [GrowlPathUtilities growlPrefPaneBundle];
	}
	//If we don't already have the path to a running GHA, then…
	if (!growlHelperAppPath) {
		//Look for an installed-but-not-running GHA.
		growlHelperAppPath = [growlPrefPaneBundle pathForResource:@"GrowlHelperApp"
														   ofType:@"app"];
	}

#ifdef GROWL_WITH_INSTALLER
	if (growlPrefPaneBundle) {
		/* Check against our current version number and ensure the installed Growl pane is the same or later */
		[self _checkForPackagedUpdateForGrowlPrefPaneBundle:growlPrefPaneBundle];
	}
#endif

	//Houston, we are go for launch.
	if (growlHelperAppPath) {
		//Let's launch in the background (requires sending the Apple Event ourselves, as LS may activate the application anyway if it's already running)
		NSURL *appURL = [NSURL fileURLWithPath:growlHelperAppPath];
		if (appURL) {
			OSStatus err;

			//Find the PSN for GrowlHelperApp. (We'll need this later.)
			struct ProcessSerialNumber appPSN = {
				0, kNoProcess
			};
			err = [self getPSN:&appPSN forAppWithBundleAtPath:growlHelperAppPath];
			BOOL foundGrowlProcess = (err == noErr);
			BOOL foundNoGrowlProcess = (err == procNotFound);

			//If both of these are false, the process search failed with an error (and I don't mean procNotFound).
			if (foundGrowlProcess || foundNoGrowlProcess) {
				NSURL *regItemURL = nil;
				BOOL passRegDict = NO;

				if (regDict) {
					NSString *regDictFileName;
					NSString *regDictPath;

					//Obtain a truly unique file name
					CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
					CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
					CFRelease(uuid);
					regDictFileName = [[NSString stringWithFormat:@"%@-%u-%@", [self _applicationNameForGrowlSearchingRegistrationDictionary:regDict], getpid(), (NSString *)uuidString] stringByAppendingPathExtension:GROWL_REG_DICT_EXTENSION];
					CFRelease(uuidString);
					if ([regDictFileName length] > NAME_MAX)
						regDictFileName = [[regDictFileName substringToIndex:(NAME_MAX - [GROWL_REG_DICT_EXTENSION length])] stringByAppendingPathExtension:GROWL_REG_DICT_EXTENSION];

					//make sure it's within pathname length constraints
					regDictPath = [NSTemporaryDirectory() stringByAppendingPathComponent:regDictFileName];
					if ([regDictPath length] > PATH_MAX)
						regDictPath = [[regDictPath substringToIndex:(PATH_MAX - [GROWL_REG_DICT_EXTENSION length])] stringByAppendingPathExtension:GROWL_REG_DICT_EXTENSION];

					//Write the registration dictionary out to the temporary directory
					NSData *plistData;
					NSString *error;
					plistData = [NSPropertyListSerialization dataFromPropertyList:regDict
																		   format:NSPropertyListBinaryFormat_v1_0
																 errorDescription:&error];
					if (plistData) {
						if (![plistData writeToFile:regDictPath atomically:NO])
							NSLog(@"GrowlApplicationBridge: Error writing registration dictionary at %@", regDictPath);
					} else {
						NSLog(@"GrowlApplicationBridge: Error writing registration dictionary at %@: %@", regDictPath, error);
						NSLog(@"GrowlApplicationBridge: Registration dictionary follows\n%@", regDict);
						[error release];
					}

					if ([[NSFileManager defaultManager] fileExistsAtPath:regDictPath]) {
						regItemURL = [NSURL fileURLWithPath:regDictPath];
						passRegDict = YES;
					}
				}

				if (foundNoGrowlProcess)
					success = [self launchApplicationWithBundleAtPath:growlHelperAppPath openDocumentURL:(passRegDict ? regItemURL : nil)];
				else
					success = [self sendOpenEventToProcessWithProcessSerialNumber:&appPSN openDocumentURL:(passRegDict ? regItemURL : nil)];
			}
		}
	}

	return success;
}

/*	+ (BOOL)launchGrowlIfInstalled
 *
 *Returns YES if the Growl helper app began launching or was already running.
 *Returns NO and performs no other action if the Growl prefPane is not properly
 *	installed.
 *If Growl is installed but disabled, the application will be registered and
 *	GrowlHelperApp will then quit.  This method will still return YES if Growl
 *	is installed but disabled.
 */
+ (BOOL) launchGrowlIfInstalled {
	return [self _launchGrowlIfInstalledWithRegistrationDictionary:nil];
}

@end
