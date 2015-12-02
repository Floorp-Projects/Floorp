/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import java.security.GeneralSecurityException;

public class CryptoException extends Exception { 
  public GeneralSecurityException cause;
  public CryptoException(GeneralSecurityException e) {
    this();
    this.cause = e;
  }
  public CryptoException() {
    
  }
  private static final long serialVersionUID = -5219310989960126830L;
}
