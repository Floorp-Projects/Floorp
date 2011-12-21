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
 *   Richard Newman <rnewman@mozilla.com>
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

import java.io.BufferedReader;
import java.io.IOException;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.util.EntityUtils;

/**
 * Shared abstract class for resource delegate that use the same timeouts
 * and no credentials.
 *
 * @author rnewman
 *
 */
public abstract class SyncResourceDelegate implements ResourceDelegate {

  protected Resource resource;
  public SyncResourceDelegate(Resource resource) {
    this.resource = resource;
  }

  @Override
  public int connectionTimeout() {
    return 30 * 1000;             // Wait 30s for a connection to open.
  }
  @Override
  public int socketTimeout() {
    return 5 * 60 * 1000;         // Wait 5 minutes for data.
  }

  @Override
  public String getCredentials() {
    return null;
  }

  @Override
  public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
  }

  /**
   * Best-effort attempt to ensure that the entity has been fully consumed.
   */
  public static void consumeEntity(HttpEntity entity) {
    try {
      EntityUtils.consume(entity);
    } catch (Exception e) {
      // Doesn't matter.
    }
  }

  public static void consumeReader(BufferedReader reader) {
    try {
      while ((reader.readLine()) != null) {
      }
    } catch (IOException e) {
      return;
    }
  }
}