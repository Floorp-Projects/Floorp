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


import ch.boye.httpclientandroidlib.HttpResponse;

public class SyncStorageResponse extends SyncResponse {
  // Server responses on which we want to switch.
  static final int SERVER_RESPONSE_OVER_QUOTA = 14;

  // Higher-level interpretations of response contents.
  public enum Reason {
    SUCCESS,
    OVER_QUOTA,
    UNAUTHORIZED_OR_REASSIGNED,
    SERVICE_UNAVAILABLE,
    BAD_REQUEST,
    UNKNOWN
  }

  public SyncStorageResponse(HttpResponse res) {
    this.response = res;
  }

  /**
   * Return the high-level definition of the status of this request.
   * @return
   */
  public Reason reason() {
    switch (this.response.getStatusLine().getStatusCode()) {
    case 200:
      return Reason.SUCCESS;
    case 400:
      try {
        Object body = this.jsonBody();
        if (body instanceof Number) {
          if (((Number) body).intValue() == SERVER_RESPONSE_OVER_QUOTA) {
            return Reason.OVER_QUOTA;
          }
        }
      } catch (Exception e) {
      }
      return Reason.BAD_REQUEST;
    case 401:
      return Reason.UNAUTHORIZED_OR_REASSIGNED;
    case 503:
      return Reason.SERVICE_UNAVAILABLE;
    }
    return Reason.UNKNOWN;
  }

  // TODO: Content-Type and Content-Length validation.

}
