/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import java.util.Map;

public class TelemetryPing {
    private final String type;
    private final String documentId;
    private final String uploadPath;
    private final Map<String, Object> measurementResults;

    /* package */ TelemetryPing(String type, String documentId, String uploadPath, Map<String, Object> measurementResults) {
        this.type = type;
        this.documentId = documentId;
        this.uploadPath = uploadPath;
        this.measurementResults = measurementResults;
    }

    public String getType() {
        return type;
    }

    public String getDocumentId() {
        return documentId;
    }

    public String getUploadPath() {
        return uploadPath;
    }

    public Map<String, Object> getMeasurementResults() {
        return measurementResults;
    }
}
