/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.math.BigInteger;

import org.mozilla.gecko.sync.Utils;

/**
 * Helper Function to generate a uniformly random value in [0, r).
 */
public class JPakeNumGeneratorRandom implements JPakeNumGenerator {

  @Override
  public BigInteger generateFromRange(BigInteger r) {
    return Utils.generateBigIntegerLessThan(r);
  }
}
