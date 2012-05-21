/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

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

  public URI collectionURI(boolean full, long newer, long limit, String sort, String ids) throws URISyntaxException {
    ArrayList<String> params = new ArrayList<String>();
    if (full) {
      params.add("full=1");
    }
    if (newer >= 0) {
      // Translate local millisecond timestamps into server decimal seconds.
      String newerString = Utils.millisecondsToDecimalSecondsString(newer);
      params.add("newer=" + newerString);
    }
    if (limit > 0) {
      params.add("limit=" + limit);
    }
    if (sort != null) {
      params.add("sort=" + sort);       // We trust these values.
    }
    if (ids != null) {
      params.add("ids=" + ids);         // We trust these values.
    }

    if (params.size() == 0) {
      return this.collectionPathURI;
    }

    StringBuilder out = new StringBuilder();
    char indicator = '?';
    for (String param : params) {
      out.append(indicator);
      indicator = '&';
      out.append(param);
    }
    String uri = this.collectionPath + out.toString();
    return new URI(uri);
  }

  public URI wboURI(String id) throws URISyntaxException {
    return new URI(this.collectionPath + "/" + id);
  }

  // Override these.
  protected long getDefaultFetchLimit() {
    return -1;
  }
  protected String getDefaultSort() {
    return null;
  }
}
