//
//  GrowlPathUtilities.h
//  Growl
//
//  Created by Ingmar Stein on 17.04.05.
//  Copyright 2005-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

/*we can't typedef the enum, because then NSSearchPathDirectory constants
 *	cannot be used in GrowlSearchPathDirectory arguments/variables without a
 *	compiler warning (because NSSearchPathDirectory and GrowlSearchPathDirectory
 *	would then be different types).
 */
typedef int GrowlSearchPathDirectory;
enum {
	//Library/Application\ Support/Growl
	GrowlSupportDirectory = 0x10000,
	//all other directory constants refer to subdirectories of Growl Support.
	GrowlScreenshotsDirectory,
	GrowlTicketsDirectory,
	GrowlPluginsDirectory,
};
typedef NSSearchPathDomainMask GrowlSearchPathDomainMask; //consistency

@interface GrowlPathUtilities : NSObject {

}

#pragma mark Bundles

/*!	@method	growlPrefPaneBundle
 *	@abstract	Returns the Growl preference pane bundle.
 *	@discussion	First, attempts to retrieve the bundle for a running
 *	 GrowlHelperApp process using <code>runningHelperAppBundle</code>, and if
 *	 that was successful, returns the .prefpane bundle that contains it (if any).
 *	Then, if that failed, searches all installed preference panes for the Growl
 *	 preference pane.
 *	@result	The <code>NSBundle</code> for the Growl preference pane if it is
 *	 installed; <code>nil</code> otherwise.
 */
+ (NSBundle *) growlPrefPaneBundle;
/*!	@method	helperAppBundle
 *	@abstract	Returns the GrowlHelperApp bundle.
 *	@discussion	First, attempts to retrieve the bundle for a running
 *	 GrowlHelperApp process using <code>runningHelperAppBundle</code>, and
 *	 returns that if it was successful.
 *	Then, if it wasn't, searches for a Growl preference pane, and, if one is
 *	 installed, returns the GrowlHelperApp bundle inside it.
 *	@result	The <code>NSBundle</code> for GrowlHelperApp if it is present;
 *	 <code>nil</code> otherwise.
 */
+ (NSBundle *) helperAppBundle;

/*!	@method	runningHelperAppBundle
 *	@abstract	Returns the bundle for the running GrowlHelperApp process.
 *	@discussion	If GrowlHelperApp is running, returns an NSBundle for the .app 
 *	 bundle it was loaded from.
 *	If GrowlHelperApp is not running, returns <code>nil</code>.
 *	@result	The <code>NSBundle</code> for GrowlHelperApp if it is running;
 *	 <code>nil</code> otherwise.
 */
+ (NSBundle *) runningHelperAppBundle;

#pragma mark Directories

/*!	@method	searchPathForDirectory:inDomains:mustBeWritable:
 *	@abstract	Returns an array of absolute paths to a given directory.
 *	@discussion	This method returns an array of all the directories of a given
 *	 type that exist (and, if <code>flag</code> is <code>YES</code>, are
 *	 writable). If no directories match this criteria, a valid (but empty)
 *	 array is returned.
 *
 *	 Unlike the <code>NSSearchPathForDirectoriesInDomains</code> function in
 *	 Foundation, this method does not allow you to specify whether tildes are
 *	 expanded: they will always be expanded.
 *	@result	An array of zero or more absolute paths.
 */
+ (NSArray *) searchPathForDirectory:(GrowlSearchPathDirectory) directory inDomains:(GrowlSearchPathDomainMask) domainMask mustBeWritable:(BOOL)flag;
/*!	@method	searchPathForDirectory:inDomains:
 *	@abstract	Returns an array of absolute paths to a given directory.
 *	@discussion	This method returns an array of all the directories of a given
 *	 type that exist. They need not be writable.
 *
 *	 Unlike the <code>NSSearchPathForDirectoriesInDomains</code> function in
 *	 Foundation, this method does not allow you to specify whether tildes are
 *	 expanded: they will always be expanded.
 *	@result	An array of zero or more absolute paths.
 */
+ (NSArray *) searchPathForDirectory:(GrowlSearchPathDirectory) directory inDomains:(GrowlSearchPathDomainMask) domainMask;

/*! @method	growlSupportDirectory
 *	@abstract	Returns the path for Growl's folder inside Application Support.
 *	@discussion	This method creates the folder if it does not already exist.
 *	@result	The path to Growl's support directory.
 */
+ (NSString *) growlSupportDirectory;

/*!	@method	screenshotsDirectory
 *	@abstract	Returns the directory where screenshots are to be stored.
 *	@discussion	The default location of this directory is
 *	 $HOME/Library/Application\ Support/Growl/Screenshots. This method creates
 *	 the folder if it does not already exist.
 *	@result	The absolute path to the screenshot directory.
 */
+ (NSString *) screenshotsDirectory;

/*!	@method	ticketsDirectory
 *	@abstract	Returns the directory where tickets are to be saved.
 *	@discussion	The default location of this directory is
 *	 $HOME/Library/Application\ Support/Growl/Tickets. This method creates
 *	 the folder if it does not already exist.
 *	@result	The absolute path to the ticket directory.
 */
+ (NSString *) ticketsDirectory;

#pragma mark Screenshot names

/*!	@method	nextScreenshotName
 *	@abstract	Returns the name you should use for the next screenshot.
 *	@discussion	Names returned by this method are currently in the format
 *	 'Screenshot N', where N starts at 1 and continues indefinitely. Note the
 *	 lack of a filename extension: you append it yourself.
 *
 *	 The name returned by this method is guaranteed to not exist with any
 *	 filename extension. This is intentional: it would be confusing for the
 *	 user if the fifth screenshot were assigned the name 'Screenshot 1' simply
 *	 because the previous four screenshots had a different filename extension.
 *
 *	 Calling this method is the same as calling
 *	 <code>nextScreenshotNameInDirectory:</code> with a directory of
 *	 <code>nil</code>.
 *	@result	A valid, non-existing, serial filename for a screenshot.
 */
+ (NSString *) nextScreenshotName;
/*!	@method	nextScreenshotNameInDirectory:
 *	@abstract	Returns the name you should use for the next screenshot in a directory.
 *	@discussion	Names returned by this method are currently in the format
 *	 'Screenshot N', where N starts at 1 and continues indefinitely. Note the
 *	 lack of a filename extension: you append it yourself. Note also that the
 *	 directory is not included as a prefix on the result.
 *
 *	 The name returned by this method is guaranteed to not exist in the given
 *	 directory with any filename extension. This is intentional: it would be
 *	 confusing for the user if the fifth screenshot were assigned the name
 *	 'Screenshot 1' simply because the previous four screenshots had a
 *	 different filename extension.
 *	@result	A valid, non-existing, serial filename for a screenshot.
 */
+ (NSString *) nextScreenshotNameInDirectory:(NSString *) dirName;

#pragma mark Tickets

/*!	@method	defaultSavePathForTicketWithApplicationName:
 *	@abstract	Returns an absolute path that can be used for saving a ticket.
 *	@discussion	When called with an application name, ".ticket" is appended to
 *	 it, and the result is appended to the absolute path to the ticket
 *	 directory. When called with <code>nil</code>, the ticket directory itself
 *	 is returned.
 *
 *	 For the purpose of this method, 'the ticket directory' refers to the first
 *	 writable directory returned by
 *	 <code>+searchPathForDirectory:inDomains:</code>. If there is no writable
 *	 directory, this method returns <code>nil</code>.
 *	@param	The application name for the ticket, or <code>nil</code>.
 *	@result	The absolute path to a ticket file, or the ticket directory where a
 *	 ticket file can be saved.
 */
+ (NSString *) defaultSavePathForTicketWithApplicationName:(NSString *) appName;

@end
