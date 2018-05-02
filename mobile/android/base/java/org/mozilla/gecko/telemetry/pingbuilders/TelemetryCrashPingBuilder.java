/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.util.Log;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.util.StringUtils;

import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.Map.Entry;
import java.util.TimeZone;

/**
 * Builds a {@link TelemetryOutgoingPing} representing a crash ping.
 *
 * See https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/crash-ping.html
 * for details on the crash ping.
 */
public class TelemetryCrashPingBuilder extends TelemetryPingBuilder {
    private static final String LOGTAG = "GeckoTelemetryCrashPingBuilder";

    private static final int PING_VERSION = 1;

    private static final String ISO8601_DATE = "yyyy-MM-dd";
    private static final String ISO8601_DATE_HOURS = "yyyy-MM-dd'T'HH':00:00.000Z'";

    // The following list should be kept in sync with the one in CrashManager.jsm
    private static final String[] ANNOTATION_WHITELIST = {
        "AsyncShutdownTimeout",
        "AvailablePageFile",
        "AvailablePhysicalMemory",
        "AvailableVirtualMemory",
        "BlockedDllList",
        "BlocklistInitFailed",
        "BuildID",
        "ContainsMemoryReport",
        "CrashTime",
        "EventLoopNestingLevel",
        "ipc_channel_error",
        "IsGarbageCollecting",
        "MozCrashReason",
        "OOMAllocationSize",
        "ProductID",
        "ProductName",
        "ReleaseChannel",
        "RemoteType",
        "SecondsSinceLastCrash",
        "ShutdownProgress",
        "StartupCrash",
        "SystemMemoryUsePercentage",
        "TextureUsage",
        "TotalPageFile",
        "TotalPhysicalMemory",
        "TotalVirtualMemory",
        "UptimeTS",
        "User32BeforeBlocklist",
        "Version",
    };

    public TelemetryCrashPingBuilder(String crashId, String clientId, HashMap<String, String> annotations) {
        super(TelemetryPingBuilder.UNIFIED_TELEMETRY_VERSION);

        payload.put("type", "crash");
        payload.put("id", docID);
        payload.put("version", TelemetryPingBuilder.UNIFIED_TELEMETRY_VERSION);
        payload.put("creationDate", currentDate(ISO8601_DATE_HOURS));
        payload.put("clientId", clientId);

        // Parse the telemetry environment and extract relevant fields from it
        ExtendedJSONObject env = null;
        String architecture = null;
        String displayVersion = null;
        String platformVersion = null;
        String xpcomAbi = null;

        try {
            env = new ExtendedJSONObject(annotations.get("TelemetryEnvironment"));
            final ExtendedJSONObject build = env.getObject("build");
            final ExtendedJSONObject system = env.getObject("system");

            if (build != null) {
                architecture = build.getString("architecture");
                displayVersion = build.getString("displayVersion");
                platformVersion = build.getString("platformVersion");
                xpcomAbi = build.getString("xpcomAbi");
            }

            if (system != null) {
              final ExtendedJSONObject os = system.getObject("os");

              if (os != null) {
                // Override the OS name so that it's consistent with main pings
                os.put("name", TelemetryPingBuilder.OS_NAME);
              }
            }
        } catch (NonObjectJSONException | IOException e) {
            Log.w(LOGTAG, "Couldn't parse the telemetry environment, the ping will be incomplete");
        }

        if (env != null) {
            payload.put("environment", env);
        }

        payload.put("payload", createPayloadNode(crashId, annotations));
        payload.put("application",
                    createApplicationNode(annotations, architecture, displayVersion, platformVersion, xpcomAbi));
    }

    /**
     * Return the current date as a string in the specified format.
     *
     * @param format The date format, the following constants are provided:
     * ISO8601_DATE - the ISO 8601 date format, YYYY-MM-DD
     * ISO8601_DATE_HOURS - the ISO 8601 full date format, YYYY-MM-DDTHH:00:00.000Z
     * @returns The formatted date as a string
     */
    private static String currentDate(String format) {
        TimeZone tz = TimeZone.getTimeZone("UTC");
        DateFormat df = new SimpleDateFormat(format);
        df.setTimeZone(tz);

        return df.format(new Date());
    }

    /**
     * Creates the ping's "payload" node which contains most of the information
     * about the crash and the "metadata" sub-node holding various annotations.
     *
     * @param crashId The UUID identifying the crash
     * @param annotations A map holding the crash annotations
     * @returns A JSON object representing the ping's payload node
     */
    private static ExtendedJSONObject createPayloadNode(String crashId, HashMap<String, String> annotations) {
        ExtendedJSONObject node = new ExtendedJSONObject();

        node.put("sessionId", annotations.get("TelemetrySessionId"));
        node.put("version", PING_VERSION);
        node.put("crashDate", currentDate(ISO8601_DATE));
        node.put("crashTime", currentDate(ISO8601_DATE_HOURS));
        node.put("hasCrashEnvironment", true);
        node.put("crashId", crashId);
        node.put("minidumpSha256Hash", annotations.get("MinidumpSha256Hash"));
        node.put("processType", "main");

        try {
            final ExtendedJSONObject stackTraces = new ExtendedJSONObject(annotations.get("StackTraces"));
            node.put("stackTraces", stackTraces);
        } catch (NonObjectJSONException | IOException e) {
            Log.w(LOGTAG, "Couldn't parse the stack traces, the ping will be incomplete");
        }

        // Assemble the payload metadata
        node.put("metadata", createMetadataNode(annotations));

        return node;
    }

    /**
     * Creates the ping's "metadata" node which is nested under the "payload" one. This node
     * contains all the non-PII annotations found in the crash dump.
     *
     * @param annotations A map holding the crash annotations
     * @returns A JSON object representing the ping's metadata node
     */
    private static ExtendedJSONObject createMetadataNode(HashMap<String, String> annotations) {
        ExtendedJSONObject node = new ExtendedJSONObject();

        for (Entry<String, String> pair : annotations.entrySet()) {
            if (Arrays.binarySearch(ANNOTATION_WHITELIST, pair.getKey()) >= 0) {
                node.put(pair.getKey(), pair.getValue());
            }
        }

        return node;
    }

    /**
     * Creates the ping's "application" node. This contains version and build information about the
     * crashed application.
     *
     * @param annotations A map holding the crash annotations
     * @param architecture The platform architecture
     * @param xpcomAbi The XPCOM ABI version
     * @returns A JSON object representing the ping's application node
     */
    private static ExtendedJSONObject createApplicationNode(HashMap<String, String> annotations,
                                                            String architecture, String displayVersion,
                                                            String platformVersion, String xpcomAbi) {
        ExtendedJSONObject node = new ExtendedJSONObject();
        final String version = annotations.get("Version");

        node.put("vendor", annotations.get("Vendor"));
        node.put("name", annotations.get("ProductName"));
        node.put("buildId", annotations.get("BuildID"));
        node.put("displayVersion", displayVersion);
        node.put("platformVersion", platformVersion);
        node.put("version", version);
        node.put("channel", annotations.get("ReleaseChannel"));

        if (architecture != null) {
            node.put("architecture", architecture);
        }

        if (xpcomAbi != null) {
            node.put("xpcomAbi", xpcomAbi);
        }

        return node;
    }

    @Override
    public String getDocType() {
        return "crash";
    }

    @Override
    public String[] getMandatoryFields() {
        return new String[] {
            "application",
            "clientId",
            "creationDate",
            "environment",
            "id",
            "payload",
            "type",
            "version",
        };
    }
}
