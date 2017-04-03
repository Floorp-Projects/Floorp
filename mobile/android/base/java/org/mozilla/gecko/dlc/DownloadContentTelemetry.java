/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;


import android.support.annotation.StringDef;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.BaseAction.RecoverableDownloadContentException;

/**
 * Helper class to send DLC telemetry events.
 */
public class DownloadContentTelemetry {
    private static final String EXTRA_ACTION = "action";
    private static final String EXTRA_RESULT = "result";
    private static final String EXTRA_CONTENT = "content";
    private static final String EXTRA_ERROR = "error";
    private static final String EXTRA_UPDATED = "updated";
    private static final String EXTRA_ACTION_REQUIRED = "action_required";

    private static final String ACTION_DOWNLOAD = "dlc_download";
    private static final String ACTION_SYNC = "dlc_sync";

    private static final String RESULT_SUCCESS = "success";
    private static final String RESULT_FAILURE = "failure";

    @StringDef({ERROR_NO_NETWORK, ERROR_NETWORK_METERED, ERROR_DISK_SPACE, ERROR_CHECKSUM, ERROR_DISK_IO,
                ERROR_NETWORK_IO, ERROR_MEMORY, ERROR_SERVER, ERROR_LOGIC, ERROR_UNRECOVERABLE})
    /* package */ @interface Error {}
    /* package */ static final String ERROR_NO_NETWORK = "no_network";
    /* package */ static final String ERROR_NETWORK_METERED = "network_metered";
    /* package */ static final String ERROR_DISK_SPACE = "disk_space";
    /* package */ static final String ERROR_CHECKSUM = "checksum";
    /* package */ static final String ERROR_DISK_IO = "io_disk";
    /* package */ static final String ERROR_NETWORK_IO = "io_network";
    /* package */ static final String ERROR_MEMORY = "memory";
    /* package */ static final String ERROR_SERVER = "server";
    /* package */ static final String ERROR_LOGIC = "logic";
    /* package */ static final String ERROR_UNRECOVERABLE = "unrecoverable";

    /* package */ static void eventDownloadSuccess(DownloadContent content) {
        try {
            final JSONObject extras = new JSONObject();
            extras.put(EXTRA_ACTION, ACTION_DOWNLOAD);
            extras.put(EXTRA_RESULT, RESULT_SUCCESS);
            extras.put(EXTRA_CONTENT, content.getId());

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, extras.toString());
        } catch (JSONException e) {
            throw new AssertionError("Should not happen: Can't build telemetry extra JSON", e);
        }
    }

    /* package */ static void eventDownloadFailure(DownloadContent content, RecoverableDownloadContentException e) {
        eventDownloadFailure(content, translateErrorType(e.getErrorType()));
    }

    /* package */ static void eventDownloadFailure(DownloadContent content, @Error String errorType) {
        try {
            final JSONObject extras = new JSONObject();
            extras.put(EXTRA_ACTION, ACTION_DOWNLOAD);
            extras.put(EXTRA_RESULT, RESULT_FAILURE);
            extras.put(EXTRA_CONTENT, content.getId());
            extras.put(EXTRA_ERROR, errorType);

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, extras.toString());
        } catch (JSONException e) {
            throw new AssertionError("Should not happen: Can't build telemetry extra JSON", e);
        }
    }

    /* package */ static void eventSyncSuccess(boolean updated, boolean actionRequired) {
        try {
            final JSONObject extras = new JSONObject();
            extras.put(EXTRA_ACTION, ACTION_SYNC);
            extras.put(EXTRA_RESULT, RESULT_SUCCESS);
            extras.put(EXTRA_UPDATED, updated);
            extras.put(EXTRA_ACTION_REQUIRED, actionRequired);

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, extras.toString());
        } catch (JSONException e) {
            throw new AssertionError("Should not happen: Can't build telemetry extra JSON", e);
        }
    }

    /* package */ static void eventSyncFailure(RecoverableDownloadContentException e) {
        eventSyncFailure(translateErrorType(e.getErrorType()));
    }

    /* package */ static void eventSyncFailure(@Error String errorType) {
        try {
            final JSONObject extras = new JSONObject();
            extras.put(EXTRA_ACTION, ACTION_SYNC);
            extras.put(EXTRA_RESULT, RESULT_FAILURE);
            extras.put(EXTRA_ERROR, errorType);

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, extras.toString());
        } catch (JSONException e) {
            throw new AssertionError("Should not happen: Can't build telemetry extra JSON", e);
        }
    }

    private static String translateErrorType(@RecoverableDownloadContentException.ErrorType int errorType) {
        switch (errorType) {
            case RecoverableDownloadContentException.DISK_IO:
                return ERROR_DISK_IO;
            case RecoverableDownloadContentException.MEMORY:
                return ERROR_MEMORY;
            case RecoverableDownloadContentException.NETWORK:
                return ERROR_NETWORK_IO;
            case RecoverableDownloadContentException.SERVER:
                return ERROR_SERVER;
            default:
                throw new AssertionError("Unknown error type: " + errorType);
        }
    }
}
