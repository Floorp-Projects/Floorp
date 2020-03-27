/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.support.annotation.NonNull;

/* package */ final class ServiceUtils {
    private static final String DEFAULT_ISOLATED_CONTENT_SERVICE_NAME_SUFFIX = "0";

    private ServiceUtils() {}

    /**
     * @return StringBuilder containing the name of a service class but not qualifed with any
     * unique identifiers.
     */
    private static StringBuilder startSvcName(@NonNull final GeckoProcessType type) {
        final StringBuilder builder = new StringBuilder(GeckoChildProcessServices.class.getName());
        builder.append("$").append(type);
        return builder;
    }

    /**
     * Given a service's GeckoProcessType, obtain the name of its class, including any qualifiers
     * that are needed to uniquely identify its manifest definition.
     */
    public static String buildSvcName(@NonNull final GeckoProcessType type, final String... suffixes) {
        final StringBuilder builder = startSvcName(type);

        for (final String suffix : suffixes) {
            builder.append(suffix);
        }

        return builder.toString();
    }

    /**
     * Given a service's GeckoProcessType, obtain the name of its class to be used for the purpose
     * of binding as an isolated service.
     *
     * Content services are defined in the manifest as "tab0" through "tabN" for some value of N.
     * For the purposes of binding to an isolated content service, we simply need to repeatedly
     * re-use the definition of "tab0", the "0" being stored as the
     * DEFAULT_ISOLATED_CONTENT_SERVICE_NAME_SUFFIX constant.
     */
    public static String buildIsolatedSvcName(@NonNull final GeckoProcessType type) {
        if (type == GeckoProcessType.CONTENT) {
            return buildSvcName(type, DEFAULT_ISOLATED_CONTENT_SERVICE_NAME_SUFFIX);
        }

        // Non-content services do not require any unique IDs
        return buildSvcName(type);
    }

    /**
     * Given a service's GeckoProcessType, obtain the unqualified name of its class.
     * @return The name of the class that hosts the implementation of the service corresponding
     * to type, but without any unique identifiers that may be required to actually instantiate it.
     */
    private static String buildSvcNamePrefix(@NonNull final GeckoProcessType type) {
        return startSvcName(type).toString();
    }

    /**
     * Extracts flags from the manifest definition of a service.
     * @param context Context to use for extraction
     * @param type Service type
     * @return flags that are specified in the service's definition in our manifest.
     * @see android.content.pm.ServiceInfo for explanation of the various flags.
     */
    public static int getServiceFlags(@NonNull final Context context, @NonNull final GeckoProcessType type) {
        final ComponentName component = new ComponentName(context, buildIsolatedSvcName(type));
        final PackageManager pkgMgr = context.getPackageManager();

        try {
            final ServiceInfo svcInfo = pkgMgr.getServiceInfo(component, 0);
            // svcInfo is never null
            return svcInfo.flags;
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Obtain the list of all services defined for |context|.
     */
    private static ServiceInfo[] getServiceList(@NonNull final Context context) {
        final PackageInfo packageInfo;
        try {
            packageInfo = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), PackageManager.GET_SERVICES);
        } catch (PackageManager.NameNotFoundException e) {
            throw new AssertionError("Should not happen: Can't get package info of own package");
        }
        return packageInfo.services;
    }

    /**
     * Count the number of service definitions in our manifest that satisfy bindings for a
     * particular service type.
     * @param context Context object to use for extracting the service definitions
     * @param type The type of service to count
     * @return The number of available service definitions.
     */
    public static int getServiceCount(@NonNull final Context context, @NonNull final GeckoProcessType type) {
        final ServiceInfo[] svcList = getServiceList(context);
        final String serviceNamePrefix = buildSvcNamePrefix(type);

        int result = 0;
        for (final ServiceInfo svc : svcList) {
            final String svcName = svc.name;
            // If svcName starts with serviceNamePrefix, then both strings must either be equal
            // or else the first subsequent character in svcName must be a digit.
            // This guards against any future GeckoProcessType whose string representation shares
            // a common prefix with another GeckoProcessType value.
            if (svcName.startsWith(serviceNamePrefix) &&
                (svcName.length() == serviceNamePrefix.length() ||
                 Character.isDigit(svcName.codePointAt(serviceNamePrefix.length())))) {
                ++result;
            }
        }

        if (result <= 0) {
            throw new RuntimeException("Could not count " + serviceNamePrefix + " services in manifest");
        }

        return result;
    }

}

