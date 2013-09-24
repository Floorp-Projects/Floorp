/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import java.math.BigInteger;

import org.mozilla.gecko.sync.jpake.JPakeNumGenerator;

public class JPakeNumGeneratorFixed implements JPakeNumGenerator {
  private String[] values;
  private int index = 0;

  public JPakeNumGeneratorFixed(String[] values) {
    this.values = values;
  }

  @Override
  public BigInteger generateFromRange(BigInteger r) {
    BigInteger ret = new BigInteger(values[index], 16).mod(r);
    index = (++index) % values.length;
    return ret;
  }
}
