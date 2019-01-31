.. _incognito:

Incognito Implementation
========================

This page provides a high level overview of how incognito works in
Firefox, primarily to help in understanding how to test the feature.

The Implementation
------------------

The incognito value in manifest.json supports ``spanning`` and ``not_allowed``.
The other value, ``split``, may be supported in the future.  The default
value is ``spanning``, however, by default access to private windows is
not allowed.  The user must turn on support, per extension, in ``about:addons``.

Internally this is handled as a hidden extension permission called
``internal:privateBrowsingAllowed``.  This permission is reset when the
extension is disabled or uninstalled.  The permission is accessible in
several ways:

- extension.privateBrowsingAllowed
- context.privateBrowsingAllowed (see BaseContext)
- WebExtensionPolicy.privateBrowsingAllowed
- WebExtensionPolicy.canAccessWindow(DOMWindow)

Testing
-------

The goal of testing is to ensure that data from a private browsing session
is not accessible to an extension without permission.

In Firefox 67, the feature will initially be disabled, however the
intention is to enable the feature on in 67.  The pref controlling this
is ``extensions.allowPrivateBrowsingByDefault``.  When this pref is
``true``, all extensions have access to private browsing and the manifest
value ``not_allowed`` will produce an error.  To enable incognito.not_allowed
for tests you must flip the pref to false.

Testing EventManager events
---------------------------

This is typically most easily handled by running a test with an extension
that has permission, using ``incognitoOverride: spanning`` in the call to
ExtensionTestUtils.loadExtension.  You can then use a second extension
without permission to try and catch any events that would typically be passed.

If the events can happen without calls produced by an extension, you can
also use BrowserTestUtils to open a private window, and use a non-permissioned
extension to run tests against it.

There are two utility functions in head.js, getIncognitoWindow and
startIncognitoMonitorExtension, which are useful for some basic testing.

Example: `browser_ext_windows_events.js <https://searchfox.org/mozilla-central/rev/78cd247b5d7a08832f87d786541d3e2204842e8e/browser/components/extensions/test/browser/browser_ext_windows_events.js>`_

Testing API Calls
-----------------

This is easily done using an extension without permission.  If you need
an ID of a window or tab, use getIncognitoWindow.  In most cases, the
API call should throw an exception when the window is not accessible.
There are some cases where API calls explicitly do not throw.

Example: `browser_ext_windows_incognito.js <https://searchfox.org/mozilla-central/rev/78cd247b5d7a08832f87d786541d3e2204842e8e/browser/components/extensions/test/browser/browser_ext_windows_incognito.js>`_

Privateness of window vs. tab
-----------------------------

Android does not currently support private windows.  When a tab is available,
the test should prefer tab over window.

- PrivateBrowsingUtils.isBrowserPrivate(tab.linkedBrowser)
- PrivateBrowsingUtils.isContentWindowPrivate(widnow)

When WebExtensionPolicy is handy to use, you can directly check window access:

- policy.canAccessWindow(window)
