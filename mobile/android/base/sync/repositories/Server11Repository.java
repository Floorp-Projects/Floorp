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

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;

import org.mozilla.gecko.sync.CredentialsSource;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

/**
 * A Server11Repository implements fetching and storing against the Sync 1.1 API.
 * It doesn't do crypto: that's the job of the middleware.
 *
 * @author rnewman
 */
public class Server11Repository extends Repository {

  private String serverURI;
  private String username;
  private String collection;
  private String collectionPath;
  private URI collectionPathURI;
  public CredentialsSource credentialsSource;
  public static final String VERSION_PATH_FRAGMENT = "1.1/";

  /**
   *
   * @param serverURI
   *        URI of the Sync 1.1 server (string)
   * @param username
   *        Username on the server (string)
   * @param collection
   *        Name of the collection (string)
   * @throws URISyntaxException
   */
  public Server11Repository(String serverURI, String username, String collection, CredentialsSource credentialsSource) throws URISyntaxException {
    this.serverURI  = serverURI;
    this.username   = username;
    this.collection = collection;

    this.collectionPath = this.serverURI + VERSION_PATH_FRAGMENT + this.username + "/storage/" + this.collection;
    this.collectionPathURI = new URI(this.collectionPath);
    this.credentialsSource = credentialsSource;
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    delegate.onSessionCreated(new Server11RepositorySession(this));
  }

  public URI collectionURI() {
    return this.collectionPathURI;
  }

  public URI collectionURI(boolean full, long newer, String ids) throws URISyntaxException {
    // Do it this way to make it easier to add more params later.
    // It's pretty ugly, I'll grant.
    // I can't believe Java doesn't have a good way to do this.
    boolean anyParams = full;
    String  uriParams = "";
    if (anyParams) {
      StringBuilder params = new StringBuilder("?");
      if (full) {
        params.append("full=1");
      }
      if (newer >= 0) {
        // Translate local millisecond timestamps into server decimal seconds.
        String newerString = Utils.millisecondsToDecimalSecondsString(newer);
        params.append((full ? "&newer=" : "newer=") + newerString);
      }
      if (ids != null) {
        params.append(((full || newer >= 0) ? "&ids=" : "ids=") + ids);
      }
      uriParams = params.toString();
    }
    String uri = this.collectionPath + uriParams;
    return new URI(uri);
  }

  public URI wboURI(String id) throws URISyntaxException {
    return new URI(this.collectionPath + "/" + id);
  }
}
