/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.Utils;

public class NullPayloadRecord extends MockRecord {
    public NullPayloadRecord() {
        super(Utils.generateGuid(), null, 0, false, 0);
    }

    @Override
    public JSONObject toJSONObject() {
        return new JSONObject();
    }
}
