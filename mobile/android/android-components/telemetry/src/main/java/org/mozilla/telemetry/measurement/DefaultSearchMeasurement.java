/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONObject;

public class DefaultSearchMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "defaultSearch";

    public interface DefaultSearchEngineProvider {
        String getDefaultSearchEngineIdentifier();
    }

    private DefaultSearchEngineProvider provider;

    public DefaultSearchMeasurement() {
        super (FIELD_NAME);
    }

    public void setDefaultSearchEngineProvider(DefaultSearchEngineProvider provider) {
        this.provider = provider;
    }

    @Override
    public Object flush() {
        if (provider == null) {
            return JSONObject.NULL;
        }

        final String identifier = provider.getDefaultSearchEngineIdentifier();
        return identifier != null ? identifier :JSONObject.NULL;
    }
}
