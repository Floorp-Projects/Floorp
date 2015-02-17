/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsPackageKitService.h"
#include "nsStringAPI.h"
#include "prlink.h"

#include <glib.h>
#include <glib-object.h>

using namespace mozilla;

typedef struct _GAsyncResult GAsyncResult;
typedef enum {
  G_BUS_TYPE_STARTER = -1,
  G_BUS_TYPE_NONE = 0,
  G_BUS_TYPE_SYSTEM  = 1,
  G_BUS_TYPE_SESSION = 2
} GBusType;
typedef struct _GCancellable GCancellable;
typedef enum {
  G_DBUS_CALL_FLAGS_NONE = 0,
  G_DBUS_CALL_FLAGS_NO_AUTO_START = (1<<0)
} GDBusCallFlags;
typedef struct _GDBusInterfaceInfo GDBusInterfaceInfo;
typedef struct _GDBusProxy GDBusProxy;
typedef enum {
  G_DBUS_PROXY_FLAGS_NONE = 0,
  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES = (1<<0),
  G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS = (1<<1),
  G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START = (1<<2),
  G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES = (1<<3)
} GDBusProxyFlags;
typedef struct _GVariant GVariant;
typedef void (*GAsyncReadyCallback) (GObject *source_object,
                                     GAsyncResult *res,
                                     gpointer user_data);

#define GDBUS_FUNCTIONS \
  FUNC(g_dbus_proxy_call, void, (GDBusProxy *proxy, const gchar *method_name, GVariant *parameters, GDBusCallFlags flags, gint timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)) \
  FUNC(g_dbus_proxy_call_finish, GVariant*, (GDBusProxy *proxy, GAsyncResult *res, GError **error)) \
  FUNC(g_dbus_proxy_new_finish, GDBusProxy*, (GAsyncResult *res, GError **error)) \
  FUNC(g_dbus_proxy_new_for_bus, void, (GBusType bus_type, GDBusProxyFlags flags, GDBusInterfaceInfo *info, const gchar *name, const gchar *object_path, const gchar *interface_name,  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)) \
  FUNC(g_variant_is_floating, gboolean, (GVariant *value)) \
  FUNC(g_variant_new, GVariant*, (const gchar *format_string, ...)) \
  FUNC(g_variant_unref, void, (GVariant* value))

#define FUNC(name, type, params) \
  typedef type (*_##name##_fn) params; \
  static _##name##_fn _##name;

GDBUS_FUNCTIONS

#undef FUNC

#define g_dbus_proxy_call _g_dbus_proxy_call
#define g_dbus_proxy_call_finish _g_dbus_proxy_call_finish
#define g_dbus_proxy_new_finish _g_dbus_proxy_new_finish
#define g_dbus_proxy_new_for_bus _g_dbus_proxy_new_for_bus
#define g_variant_is_floating _g_variant_is_floating
#define g_variant_new _g_variant_new
#define g_variant_unref _g_variant_unref

static PRLibrary *gioLib = nullptr;

typedef void (*nsGDBusFunc)();
struct nsGDBusDynamicFunction {
  const char *functionName;
  nsGDBusFunc *function;
};

nsresult
nsPackageKitService::Init()
{
#define FUNC(name, type, params) { #name, (nsGDBusFunc *)&_##name },
  const nsGDBusDynamicFunction kGDBusSymbols[] = {
    GDBUS_FUNCTIONS
  };
#undef FUNC

  if (!gioLib) {
    gioLib = PR_LoadLibrary("libgio-2.0.so.0");
    if (!gioLib)
      return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < ArrayLength(kGDBusSymbols); i++) {
    *kGDBusSymbols[i].function =
      PR_FindFunctionSymbol(gioLib, kGDBusSymbols[i].functionName);
    if (!*kGDBusSymbols[i].function) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsPackageKitService, nsIPackageKitService)

nsPackageKitService::~nsPackageKitService()
{
  if (gioLib) {
    PR_UnloadLibrary(gioLib);
    gioLib = nullptr;
  }
}

static const char* InstallPackagesMethods[] = {
  "InstallPackageNames",
  "InstallMimeTypes",
  "InstallFontconfigResources",
  "InstallGStreamerResources"
};

struct InstallPackagesProxyNewData {
  nsCOMPtr<nsIObserver> observer;
  uint32_t method;
  GVariant* parameters;
};

static void
InstallPackagesNotifyObserver(nsIObserver* aObserver,
                              gchar* aErrorMessage)
{
  if (aObserver) {
    aObserver->Observe(nullptr, "packagekit-install",
                       aErrorMessage ?
                       NS_ConvertUTF8toUTF16(aErrorMessage).get() :
                       nullptr);
  }
}

static void
InstallPackagesProxyCallCallback(GObject *aSourceObject,
                                 GAsyncResult *aResult,
                                 gpointer aUserData)
{
  nsCOMPtr<nsIObserver> observer = static_cast<nsIObserver*>(aUserData);
  GDBusProxy* proxy = reinterpret_cast<GDBusProxy*>(aSourceObject);

  GError* error = nullptr;
  GVariant* result = g_dbus_proxy_call_finish(proxy, aResult, &error);
  if (result) {
    InstallPackagesNotifyObserver(observer, nullptr);
    g_variant_unref(result);
  } else {
    NS_ASSERTION(error, "g_dbus_proxy_call_finish should set error when it returns NULL");
    InstallPackagesNotifyObserver(observer, error->message);
    g_error_free(error);
  }

  g_object_unref(proxy);
  observer->Release();
}

static void
InstallPackagesProxyNewCallback(GObject *aSourceObject,
                                GAsyncResult *aResult,
                                gpointer aUserData)
{
  InstallPackagesProxyNewData* userData =
    static_cast<InstallPackagesProxyNewData*>(aUserData);

  NS_ASSERTION(g_variant_is_floating(userData->parameters),
               "userData->parameters should be a floating reference.");

  GError* error = nullptr;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(aResult, &error);

  if (proxy) {
    // Send the asynchronous request to install the packages
    // This call will consume the floating reference userData->parameters so we
    // don't need to release it explicitly.
    nsIObserver* observer;
    userData->observer.forget(&observer);
    g_dbus_proxy_call(proxy,
                      InstallPackagesMethods[userData->method],
                      userData->parameters,
                      G_DBUS_CALL_FLAGS_NONE,
                      G_MAXINT,
                      nullptr,
                      &InstallPackagesProxyCallCallback,
                      static_cast<gpointer>(observer));
  } else {
    NS_ASSERTION(error, "g_dbus_proxy_new_finish should set error when it returns NULL");
    InstallPackagesNotifyObserver(userData->observer, error->message);
    g_error_free(error);
    g_variant_unref(userData->parameters);
  }
  delete userData;
}

NS_IMETHODIMP
nsPackageKitService::InstallPackages(uint32_t aInstallMethod,
                                     nsIArray* aPackageArray,
                                     nsIObserver* aObserver)
{
  NS_ENSURE_ARG(aPackageArray);

  uint32_t arrayLength;
  aPackageArray->GetLength(&arrayLength);
  if (arrayLength == 0 ||
      arrayLength == std::numeric_limits<uint32_t>::max() ||
      aInstallMethod >= PK_INSTALL_METHOD_COUNT) {
    return NS_ERROR_INVALID_ARG;
  }

  // Create the GVariant* parameter from the list of packages.
  GVariant* parameters = nullptr;
  nsAutoArrayPtr<gchar*> packages(new gchar*[arrayLength + 1]);

  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < arrayLength; i++) {
    nsCOMPtr<nsISupportsString> package =
      do_QueryElementAt(aPackageArray, i);
    if (!package) {
      rv = NS_ERROR_FAILURE;
      break;
    }
    nsString data;
    package->GetData(data);
    packages[i] = g_strdup(NS_ConvertUTF16toUTF8(data).get());
    if (!packages[i]) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
  }
  packages[arrayLength] = nullptr;

  if (NS_SUCCEEDED(rv)) {
    // We create a new GVariant object to send parameters to PackageKit.
    parameters = g_variant_new("(u^ass)", static_cast<guint32>(0),
                               packages.get(), "hide-finished");
    if (!parameters) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  for (uint32_t i = 0; i < arrayLength; i++) {
    g_free(packages[i]);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Send the asynchronous request to load the bus proxy
  InstallPackagesProxyNewData* data = new InstallPackagesProxyNewData;
  data->observer = aObserver;
  data->method = aInstallMethod;
  data->parameters = parameters;
  g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                           G_DBUS_PROXY_FLAGS_NONE,
                           nullptr,
                           "org.freedesktop.PackageKit",
                           "/org/freedesktop/PackageKit",
                           "org.freedesktop.PackageKit.Modify",
                           nullptr,
                           &InstallPackagesProxyNewCallback,
                           static_cast<gpointer>(data));
  return NS_OK;
}
