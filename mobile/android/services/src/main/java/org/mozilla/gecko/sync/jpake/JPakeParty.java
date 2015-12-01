/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.math.BigInteger;

public class JPakeParty {

  // Generated values for key exchange.
  public String       signerId;
  public BigInteger   gx1;
  public Zkp          zkp1;

  public BigInteger   x2;
  public BigInteger   gx2;
  public Zkp          zkp2;

  public BigInteger   thisA;
  public Zkp          thisZkpA;

  // Received values during key exchange.
  public BigInteger   gx3;
  public Zkp          zkp3;

  public BigInteger   gx4;
  public Zkp          zkp4;

  public BigInteger   otherA;
  public Zkp          otherZkpA;


  public JPakeParty(String mySignerId) {
    this.signerId = mySignerId;
  }
}
