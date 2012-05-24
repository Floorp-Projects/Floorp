/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.math.BigInteger;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.setup.Constants;

public class JPakeJson {
  /*
   * Helper function to generate a JSON-encoded ZKP.
   */
  public static ExtendedJSONObject makeJZkp(BigInteger gr, BigInteger b, String id) {
    ExtendedJSONObject result = new ExtendedJSONObject();
    result.put(Constants.ZKP_KEY_GR, BigIntegerHelper.toEvenLengthHex(gr));
    result.put(Constants.ZKP_KEY_B, BigIntegerHelper.toEvenLengthHex(b));
    result.put(Constants.ZKP_KEY_ID, id);
    return result;
  }
}
