/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.auth;

import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;

public interface AuthenticatorStage {
  public void execute(AccountAuthenticator aa) throws URISyntaxException, UnsupportedEncodingException;
}
