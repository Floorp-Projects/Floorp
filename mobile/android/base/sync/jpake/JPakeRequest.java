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
 *   Chenxia Liu <liuche@mozilla.com>
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

package org.mozilla.gecko.sync.jpake;

import java.net.URI;
import java.net.URISyntaxException;

import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.ResourceDelegate;

import android.util.Log;
import ch.boye.httpclientandroidlib.HttpEntity;

public class JPakeRequest implements Resource {
  private static String LOG_TAG = "JPAKE_REQUEST";

  private BaseResource resource;
  public JPakeRequestDelegate delegate;

  public JPakeRequest(String uri, ResourceDelegate delegate) throws URISyntaxException {
    this(new URI(uri), delegate);
  }

  public JPakeRequest(URI uri, ResourceDelegate delegate) {
    this.resource = new BaseResource(uri);
    this.resource.delegate = delegate;
    Log.d(LOG_TAG, "new URI: " + uri);
  }

  @Override
  public void get() {
    this.resource.get();
  }

  @Override
  public void delete() {
    this.resource.delete();
  }

  @Override
  public void post(HttpEntity body) {
    this.resource.post(body);
  }

  @Override
  public void put(HttpEntity body) {
    this.resource.put(body);
  }
}
