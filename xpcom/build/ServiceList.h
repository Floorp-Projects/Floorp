/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef ACCESSIBILITY
MOZ_SERVICE(AccessibilityService, nsIAccessibilityService, "@mozilla.org/accessibilityService;1")
#endif
MOZ_SERVICE(ChromeRegistryService, nsIChromeRegistry, "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(ToolkitChromeRegistryService, nsIToolkitChromeRegistry, "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(XULChromeRegistryService, nsIXULChromeRegistry, "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(XULOverlayProviderService, nsIXULOverlayProvider, "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(IOService, nsIIOService, "@mozilla.org/network/io-service;1")
MOZ_SERVICE(ObserverService, nsIObserverService, "@mozilla.org/observer-service;1")
MOZ_SERVICE(StringBundleService, nsIStringBundleService, "@mozilla.org/intl/stringbundle;1")
MOZ_SERVICE(XPConnect, nsIXPConnect, "@mozilla.org/js/xpc/XPConnect;1")

#ifdef MOZ_USE_NAMESPACE
namespace mozilla
{
#endif

MOZ_SERVICE(HistoryService, IHistory, "@mozilla.org/browser/history;1")

#ifdef MOZ_USE_NAMESPACE
}
#endif
