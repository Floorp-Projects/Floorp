# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- #4137 - Adds pagination to the history view
- #3695 - Made search suggestions for other tabs clickable

### Changed
- Remove forced focus of toolbar on homescreen
- #4529 - Fixed an issue where the app would sometimes return to a blank toolbar
- #4427 - Fixed an issue where the app would sometimes return to the home fragment

### Removed

## [1.1.0 and earlier] - 2019-07-23
### Added
- #2770 - Added ability to receive tabs from other FxA devices
- #919 - Enabled bookmark synchronization
- #916 - Added the ability to save and delete bookmarks
- #356 - Added the ability to delete history
- #208 - Added normal browsing dark mode (advised to use attrs from now on for most referenced colors)
- #957 - Added telemetry for the context menu
- #1036 - Added telemetry for Find in Page
- #1049 - Added style for progress bar with gradient drawable
- #1165 - Added doorhanger to the toolbar
- #1002 - Added ability to restore tab after crash
- #1195 - Adds telemetry for quick action sheet
- #627 - Sets engine preferred color scheme based on light/dark theme
- #904 - Added tab counter in browser toolbar
- #1312 - Added the ability to edit bookmarks
- #1236 - Added the ability to create bookmark folders
- #1237 - Added the ability to delete bookmark folders
- #1238 - Added the ability to edit bookmark folders
- #1239 - Added the ability to move bookmark folders
- #1068 - Adds the ability to quickly copy the URL by long clicking the URLBar
- #1170: Allow user to add a new site exception to site permissions
- #1430 - Adds the Fenix Snackbar
- #1397 - Adds favicons to the history view
- #1375 - Added setting for turning off history suggestions
- #1139 - Resolved a 170ms delay on cold start
- #176 - Added a swipe to delete gesture on home screen
- #1539 - Added bookmarks multi-select related features
- #1603 - Remove deprecated success path for Firefox Accounts login
- #619 - Sets toolbar behavior based on accessibility and if session is loading
- #1571 - Added a snackbar for undoing bookmark deletion
- #1079 - Managing site permissions exceptions
- #1312 - Added clear textfield buttons for editing bookmarks
- #1312 - Added a missing edit action for bookmark selections
- #974 - Added telemetry for bookmarks
- #113 - Added QR code scanner
- #975 - Added telemetry for preference switches
- #1955 - Added a confirmation dialog for QR code and barcode searches
- #1874 - Added a "Turn on Sync" fragment for Firefox Accounts login and sign up
- #2308 - Update the deprecated BitmapDrawable constructor
- #1311 - Enable downloads in custom tabs.
- #1874 - Added TOP info panel dialog for custom tabs.
- #1411 - Added disabled style for disabled permissions items in site info panel.
- #1735 - Adds API to see the release channel
- #2318 - Added Firefox Accounts Pairing feature to "Turn On Sync" options
- #2390 - Adds Onboarding for Fenix
- #2531 - Adds link to privacy policy in settings
- #225 - Adds the ability to delete all browsing data
- #2903 - Fixed crash when trying to rate the app on a devices without the play store app.
- #2419 - Adds a deletion state to the history component
- #1570 - Enables the opening of links by other apps. Disabled in #3359.
- #3200 - Adds application-services fretboard feature flags for FxA and Sync.
- #3865 - Added a new nightly icon and app name

### Changed
- #3278 - Updates strings in preferences
- #2673 - Fixed can't upload files using third party apps from the file manager.
- #1429 - Updated site permissions ui for MVP
- #1599 - Fixed a crash creating a bookmark for a custom tab
- #1414 - Fixed site permissions settings getting reset in Android 6.
- #1994 - Made app state persist better when rotating the screen
- #654 - Updated Refresh button to turn into Stop button while menu is open.
- [AC #2725](https://github.com/mozilla-mobile/android-components/issues/2725) Updated tracking protectionPolicy to [recommend](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L156)
- #2789 Custom tabs is not covering the full screen size.
- #2893, #2673, #2916, #2314: Fix several crashes navigating from external links
- #3750 - Crash when tapping "Blocked" on Google Maps after disabling location requests
- #2945 - Fixed "Launches to blank screen and hangs on Nexus 10"
- #3869 - Creates a SearchLocalizationProvider that sets the region to get the correct default search engine based on a locale
- #4192 - Sets keyboard to private mode in private browsing
- #2142 - Fixed "When launching Fenix, Enable private browsing button is focused instead of Search or address"
### Removed
