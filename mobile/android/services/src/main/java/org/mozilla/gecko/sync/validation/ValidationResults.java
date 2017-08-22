/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.validation;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;

import java.util.List;
import java.util.Map;

public abstract class ValidationResults {
    /**
     * Get the problems found by the validator. Must not contain numbers less than or equal to zero.
     * Must use the same names for problems as other platforms!
     */
    public abstract Map<String, Integer> summarizeResults();

    /**
     * Get the summary as JSON suitable for including in telemetry
     */
    @SuppressWarnings("unchecked")
    public JSONArray jsonSummary() {
        Map<String, Integer> problems = summarizeResults();
        JSONArray result = new JSONArray();
        for (Map.Entry<String, Integer> problem : problems.entrySet()) {
            JSONObject o = new JSONObject();
            o.put("name", problem.getKey());
            o.put("count", problem.getValue());
            result.add(o);
        }
        return result;
    }

    public boolean anyProblemsExist() {
        return summarizeResults().size() > 0;
    }
}
