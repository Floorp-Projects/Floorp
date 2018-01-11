/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "mozilla/Services.h"

MOZ_SERVICE(ChromeRegistryService, nsIChromeRegistry,
            "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(ToolkitChromeRegistryService, nsIToolkitChromeRegistry,
            "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(XULChromeRegistryService, nsIXULChromeRegistry,
            "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(XULOverlayProviderService, nsIXULOverlayProvider,
            "@mozilla.org/chrome/chrome-registry;1")
MOZ_SERVICE(IOService, nsIIOService,
            "@mozilla.org/network/io-service;1")
MOZ_SERVICE(ObserverService, nsIObserverService,
            "@mozilla.org/observer-service;1")
MOZ_SERVICE(StringBundleService, nsIStringBundleService,
            "@mozilla.org/intl/stringbundle;1")
MOZ_SERVICE(XPConnect, nsIXPConnect,
            "@mozilla.org/js/xpc/XPConnect;1")
MOZ_SERVICE(PermissionManager, nsIPermissionManager,
            "@mozilla.org/permissionmanager;1");
MOZ_SERVICE(ServiceWorkerManager, nsIServiceWorkerManager,
            "@mozilla.org/serviceworkers/manager;1");
MOZ_SERVICE(AsyncShutdown, nsIAsyncShutdownService,
            "@mozilla.org/async-shutdown-service;1")
MOZ_SERVICE(UUIDGenerator, nsIUUIDGenerator,
            "@mozilla.org/uuid-generator;1");
MOZ_SERVICE(GfxInfo, nsIGfxInfo,
            "@mozilla.org/gfx/info;1");
MOZ_SERVICE(SocketTransportService, nsISocketTransportService,
            "@mozilla.org/network/socket-transport-service;1");
MOZ_SERVICE(StreamTransportService, nsIStreamTransportService,
            "@mozilla.org/network/stream-transport-service;1");
MOZ_SERVICE(CacheStorageService, nsICacheStorageService,
            "@mozilla.org/netwerk/cache-storage-service;1");
MOZ_SERVICE(URIClassifier, nsIURIClassifier,
            "@mozilla.org/uriclassifierservice");
MOZ_SERVICE(ActivityDistributor, nsIHttpActivityDistributor,
            "@mozilla.org/network/http-activity-distributor;1");

#ifdef MOZ_USE_NAMESPACE
namespace mozilla {
#endif

MOZ_SERVICE(HistoryService, IHistory,
            "@mozilla.org/browser/history;1")

#ifdef MOZ_USE_NAMESPACE
} // namespace mozilla
#endif
