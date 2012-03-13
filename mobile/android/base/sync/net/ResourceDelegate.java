/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.security.GeneralSecurityException;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

/**
 * ResourceDelegate implementers must ensure that HTTP responses
 * are fully consumed to ensure that connections are returned to
 * the pool:
 *
 *          EntityUtils.consume(entity);
 * @author rnewman
 *
 */
public interface ResourceDelegate {
  // Request augmentation.
  String getCredentials();
  void addHeaders(HttpRequestBase request, DefaultHttpClient client);

  // Response handling.

  /**
   * Override this to handle an HttpResponse.
   *
   * ResourceDelegate implementers <b>must</b> ensure that HTTP responses are
   * fully consumed to ensure that connections are returned to the pool, for
   * example by calling <code>EntityUtils.consume(response.getEntity())</code>.
   */
  void handleHttpResponse(HttpResponse response);
  void handleHttpProtocolException(ClientProtocolException e);
  void handleHttpIOException(IOException e);

  // During preparation.
  void handleTransportException(GeneralSecurityException e);

  // Connection parameters.
  int connectionTimeout();
  int socketTimeout();
}
