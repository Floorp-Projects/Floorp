# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# NOTE: Although this generates headers and code for C++, using Services.h
# is deprecated in favour of Components.h. This system is currently only
# used for generating services.rs (until a Rust version of Components.h is
# is written and this can be completely deleted).

import buildconfig


services = []


def service(name, iface, contractid):
    """Define a convenient service getter"""
    services.append((name, iface, contractid))


# The `name` parameter is derived from the `iface` by removing the `nsI`
# prefix. (This often matches the `contractid`, but not always.) The only
# exceptions are when that would result in a misleading name, e.g. for
# "@mozilla.org/file/directory_service;1".
service("ChromeRegistry", "nsIChromeRegistry", "@mozilla.org/chrome/chrome-registry;1")
service(
    "ToolkitChromeRegistry",
    "nsIToolkitChromeRegistry",
    "@mozilla.org/chrome/chrome-registry;1",
)
service(
    "XULChromeRegistry", "nsIXULChromeRegistry", "@mozilla.org/chrome/chrome-registry;1"
)
service("DirectoryService", "nsIProperties", "@mozilla.org/file/directory_service;1"),
service("IOService", "nsIIOService", "@mozilla.org/network/io-service;1")
service("ObserverService", "nsIObserverService", "@mozilla.org/observer-service;1")
service(
    "StringBundleService", "nsIStringBundleService", "@mozilla.org/intl/stringbundle;1"
)
service("PermissionManager", "nsIPermissionManager", "@mozilla.org/permissionmanager;1")
service("PrefService", "nsIPrefService", "@mozilla.org/preferences-service;1")
service(
    "ServiceWorkerManager",
    "nsIServiceWorkerManager",
    "@mozilla.org/serviceworkers/manager;1",
)
service(
    "AsyncShutdownService",
    "nsIAsyncShutdownService",
    "@mozilla.org/async-shutdown-service;1",
)
service("UUIDGenerator", "nsIUUIDGenerator", "@mozilla.org/uuid-generator;1")
service("GfxInfo", "nsIGfxInfo", "@mozilla.org/gfx/info;1")
service(
    "SocketTransportService",
    "nsISocketTransportService",
    "@mozilla.org/network/socket-transport-service;1",
)
service(
    "StreamTransportService",
    "nsIStreamTransportService",
    "@mozilla.org/network/stream-transport-service;1",
)
service(
    "CacheStorageService",
    "nsICacheStorageService",
    "@mozilla.org/netwerk/cache-storage-service;1",
)
service("URIClassifier", "nsIURIClassifier", "@mozilla.org/uriclassifierservice")
service(
    "HttpActivityDistributor",
    "nsIHttpActivityDistributor",
    "@mozilla.org/network/http-activity-distributor;1",
)
service("History", "mozilla::IHistory", "@mozilla.org/browser/history;1")
service("ThirdPartyUtil", "mozIThirdPartyUtil", "@mozilla.org/thirdpartyutil;1")
service("URIFixup", "nsIURIFixup", "@mozilla.org/docshell/uri-fixup;1")
service("Bits", "nsIBits", "@mozilla.org/bits;1")
service(
    "MemoryReporterManager",
    "nsIMemoryReporterManager",
    "@mozilla.org/memory-reporter-manager;1",
)
# If you want nsIXULAppInfo, as returned by Services.jsm, you need to call:
#
# nsCOMPtr<nsIXULRuntime> runtime = mozilla::services::GetXULRuntime();
# nsCOMPtr<nsIXULAppInfo> appInfo = do_QueryInterface(runtime);
#
# for C++ or:
#
# let appInfo =
#    get_XULRuntime().and_then(|p| p.query_interface::<nsIXULAppInfo>());
#
# for Rust.  Note that not all applications (e.g. xpcshell) implement
# nsIXULAppInfo.
service("XULRuntime", "nsIXULRuntime", "@mozilla.org/xre/app-info;1")

if buildconfig.substs.get("ENABLE_WEBDRIVER"):
    service("RemoteAgent", "nsIRemoteAgent", "@mozilla.org/remote/agent;1")

# The definition file needs access to the definitions of the particular
# interfaces. If you add a new interface here, make sure the necessary includes
# are also listed in the following code snippet.
CPP_INCLUDES = """
#include "mozilla/Likely.h"
#include "mozilla/Services.h"
#include "mozIThirdPartyUtil.h"
#include "nsComponentManager.h"
#include "nsIObserverService.h"
#include "nsNetCID.h"
#include "nsObserverService.h"
#include "nsXPCOMPrivate.h"
#include "nsIIOService.h"
#include "nsIDirectoryService.h"
#include "nsIChromeRegistry.h"
#include "nsIStringBundle.h"
#include "nsIToolkitChromeRegistry.h"
#include "IHistory.h"
#include "nsIXPConnect.h"
#include "nsIPermissionManager.h"
#include "nsIPrefService.h"
#include "nsIServiceWorkerManager.h"
#include "nsICacheStorageService.h"
#include "nsIStreamTransportService.h"
#include "nsISocketTransportService.h"
#include "nsIURIClassifier.h"
#include "nsIHttpActivityObserver.h"
#include "nsIAsyncShutdown.h"
#include "nsIUUIDGenerator.h"
#include "nsIGfxInfo.h"
#include "nsIURIFixup.h"
#include "nsIBits.h"
#include "nsIXULRuntime.h"
"""

if buildconfig.substs.get("ENABLE_WEBDRIVER"):
    CPP_INCLUDES += '#include "nsIRemoteAgent.h"'


#####
# Codegen Logic
#
# The following code consumes the data listed above to generate the files
# Services.h, Services.cpp, and services.rs which provide access to these
# service getters in both rust and C++ code.
#
# XXX(nika): would it be a good idea to unify Services.jsm into here too?


def services_h(output):
    output.write(
        """\
/* THIS FILE IS GENERATED BY Services.py - DO NOT EDIT */

#ifndef mozilla_Services_h
#define mozilla_Services_h

#include "nscore.h"
#include "nsCOMPtr.h"
"""
    )

    for (name, iface, contractid) in services:
        # Write out a forward declaration for the type in question
        segs = iface.split("::")
        for namespace in segs[:-1]:
            output.write("namespace %s {\n" % namespace)
        output.write("class %s;\n" % segs[-1])
        for namespace in reversed(segs[:-1]):
            output.write("} // namespace %s\n" % namespace)

        # Write out the C-style function signature, and the C++ wrapper
        output.write(
            """
#ifdef MOZILLA_INTERNAL_API
extern "C" {
/**
 * NOTE: Don't call this method directly, instead call mozilla::services::Get%(name)s.
 * It is used to expose XPCOM services to rust code. The return value is already addrefed.
 */
%(type)s* XPCOMService_Get%(name)s();
} // extern "C"

namespace mozilla {
namespace services {
/**
 * Fetch a cached instance of the %(name)s.
 * This function will return nullptr during XPCOM shutdown.
 */
inline already_AddRefed<%(type)s>
Get%(name)s()
{
  return already_AddRefed<%(type)s>(XPCOMService_Get%(name)s());
}
} // namespace services
} // namespace mozilla
#endif // defined(MOZILLA_INTERNAL_API)
"""
            % {
                "name": name,
                "type": iface,
            }
        )

    output.write("#endif // !defined(mozilla_Services_h)\n")


def services_cpp(output):
    output.write(
        """\
/* THIS FILE IS GENERATED BY Services.py - DO NOT EDIT */
"""
    )
    output.write(CPP_INCLUDES)

    for (name, iface, contractid) in services:
        output.write(
            """
static %(type)s* g%(name)s = nullptr;

extern "C" {
/**
 * NOTE: Don't call this method directly, instead call `mozilla::services::Get%(name)s`.
 * This method is extern "C" to expose XPCOM services to rust code.
 * The return value is already addrefed.
 */
%(type)s*
XPCOMService_Get%(name)s()
{
  if (MOZ_UNLIKELY(gXPCOMShuttingDown)) {
    return nullptr;
  }
  if (!g%(name)s) {
    nsCOMPtr<%(type)s> os = do_GetService("%(contractid)s");
    os.swap(g%(name)s);
  }
  return do_AddRef(g%(name)s).take();
}
} // extern "C"
"""
            % {
                "name": name,
                "type": iface,
                "contractid": contractid,
            }
        )

    output.write(
        """
/**
 * Clears service cache, sets gXPCOMShuttingDown
 */
void
mozilla::services::Shutdown()
{
  gXPCOMShuttingDown = true;
"""
    )
    for (name, iface, contractid) in services:
        output.write("  NS_IF_RELEASE(g%s);\n" % name)
    output.write("}\n")


def services_rs(output):
    output.write(
        """\
/* THIS FILE IS GENERATED BY Services.py - DO NOT EDIT */

use crate::RefPtr;
"""
    )

    for (name, iface, _) in services:
        # NOTE: We can't support namespaced interfaces in rust code, so we have to ignore them.
        if "::" in iface:
            continue

        output.write(
            """
/// Fetches a cached reference to the `%(name)s`.
/// This function will return `None` during XPCOM shutdown.
pub fn get_%(name)s() -> Option<RefPtr<crate::interfaces::%(type)s>> {
    extern "C" {
        fn XPCOMService_Get%(name)s() -> *mut crate::interfaces::%(type)s;
    }
    unsafe { RefPtr::from_raw_dont_addref(XPCOMService_Get%(name)s()) }
}
"""
            % {
                "name": name,
                "type": iface,
            }
        )
