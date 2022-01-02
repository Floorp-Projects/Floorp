Utilities for implementing APIs
===============================

This page covers some utility classes that are useful for
implementing WebExtension APIs:

WindowManager
-------------
This class manages the mapping between the opaque window identifiers used
in the `browser.windows <https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/windows>`__ API.
See the reference docs `here <reference.html#windowmanager-class>`__.

TabManager
----------
This class manages the mapping between the opaque tab identifiers used
in the `browser.tabs <https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/tabs>`__ API.
See the reference docs `here <reference.html#tabmanager-class>`__.

ExtensionSettingsStore
----------------------
ExtensionSettingsStore (ESS) is used for storing changes to settings that are
requested by extensions, and for finding out what the current value
of a setting should be, based on the precedence chain or a specific selection
made (typically) by the user.

When multiple extensions request to make a change to a particular
setting, the most recently installed extension will be given
precedence.

It is also possible to select a specific extension (or no extension, which
infers user-set) to control a setting.  This will typically only happen via
ExtensionPreferencesManager described below.  When this happens, precedence
control is not used until either a new extension is installed, or the controlling
extension is disabled or uninstalled.  If user-set is specifically chosen,
precedence order will only be returned to by installing a new extension that
takes control of the setting.

ESS will manage what has control over a setting through any
extension state changes (ie. install, uninstall, enable, disable).

Notifications:
^^^^^^^^^^^^^^

"extension-setting-changed":
****************************

  When a setting changes an event is emitted via the apiManager. It contains
  the following:

  * *action*: one of select, remove, enable, disable

  * *id*: the id of the extension for which the setting has changed, may be null
    if the setting has returned to default or user set.

  * *type*: The type of setting altered.  This is defined by the module using ESS.
    If the setting is controlled through the ExtensionPreferencesManager below,
    the value will be "prefs".

  * *key*: The name of the setting altered.

  * *item*: The new value, if any that has taken control of the setting.


ExtensionPreferencesManager
---------------------------
ExtensionPreferencesManager (EPM) is used to manage what extensions may control a
setting that results in changing a preference.  EPM adds additional logic on top
of ESS to help manage the preference values based on what is in control of a
setting.

Defining a setting in an API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A preference setting is defined in an API module by calling EPM.addSetting.  addSetting
allows the API to use callbacks that can handle setting preferences as needed.  Since
the setting is defined at runtime, the API module must be loaded as necessary by EPM
to properly manage settings.

In the api module definition (e.g. ext-toolkit.json), the api must use `"settings": true`
so the management code can discover which API modules to load in order to manage a
setting.  See browserSettings[1] as an example.

Settings that are exposed to the user in about:preferences also require special handling.
We typically show that an extension is in control of the preference, and prevent changes
to the setting.  Some settings may allow the user to choose which extension (or none) has
control of the setting.

Preferences behavior
^^^^^^^^^^^^^^^^^^^^

To actually set a setting, the module must call EPM.setSetting.  This is typically done
via an extension API, such as browserSettings.settingName.set({ ...value data... }), though
it may be done at other times, such as during extension startup or install in a modules
onManifest handler.

Preferences are not always changed when an extension uses an API that results in a call
to EPM.setSetting.  When setSetting is called, the values are stored by ESS (above), and if
the extension currently has control, or the setting is controllable by the extension, then
the preferences would be updated.

The preferences would also potentially be updated when installing, enabling, disabling or
uninstalling an extension, or by a user action in about:preferences (or other UI that
allows controlling the preferences).  If all extensions that use a preference setting are
disabled or uninstalled, the prior user-set or default values would be returned to.

An extension may watch for changes using the onChange api (e.g. browserSettings.settingName.onChange).

[1] https://searchfox.org/mozilla-central/rev/04d8e7629354bab9e6a285183e763410860c5006/toolkit/components/extensions/ext-toolkit.json#19

Notifications:
^^^^^^^^^^^^^^

"extension-setting-changed:*name*":
***********************************

  When a setting controlled by EPM changes an event is emitted via the apiManager. It contains
  no other data.  This is used primarily to implement the onChange API.

ESS vs. EPM
-----------
An API may use ESS when it needs to allow an extension to store a setting value that
affects how Firefox works, but does not result in setting a preference.  An example
is allowing an extension to change the newTab value in the newTab service.

An API should use EPM when it needs to allow an extension to change a preference.

Using ESS/EPM with experimental APIs
------------------------------------

Properly managing settings values depends on the ability to load any modules that
define a setting.  Since experimental APIs are defined inside the extension, there
are situations where settings defined in experimental APIs may not be correctly
managed.  The could result in a preference remaining set by the extension after
the extension is disabled or installed, especially when that state is updated during
safe mode.

Extensions making use of settings in an experimental API should practice caution,
potentially unsetting the values when the extension is shutdown.  Values used for
the setting could be stored in the extensions locale storage, and restored into
EPM when the extension is started again.
