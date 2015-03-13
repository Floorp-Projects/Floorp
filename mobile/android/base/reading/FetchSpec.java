/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.net.URI;
import java.net.URISyntaxException;

import ch.boye.httpclientandroidlib.client.utils.URIBuilder;

/**
 * Defines the parameters that can be added to a reading list fetch URI.
 */
public class FetchSpec {
  private final String queryString;

  private FetchSpec(final String q) {
    this.queryString = q;
  }

  public URI getURI(final URI serviceURI) throws URISyntaxException {
    return new URIBuilder(serviceURI).setCustomQuery(queryString).build();
  }

  public URI getURI(final URI serviceURI, final String path) throws URISyntaxException {
    final String currentPath = serviceURI.getPath();
    final String newPath = (currentPath == null ? "" : currentPath) + path;
    return new URIBuilder(serviceURI).setPath(newPath)
                                     .setCustomQuery(queryString)
                                     .build();
  }

  public static class Builder {
    final StringBuilder b = new StringBuilder();
    boolean first = true;

    public FetchSpec build() {
      return new FetchSpec(b.toString());
    }

    private void ampersand() {
      if (first) {
        first = false;
        return;
      }
      b.append('&');
    }

    public Builder setUnread(boolean unread) {
      ampersand();
      b.append("unread=");
      b.append(unread);
      return this;
    }

    private void qualifyAttribute(String qual, String attr) {
      ampersand();
      b.append(qual);
      b.append(attr);
      b.append('=');
    }

    public Builder setMinAttribute(String attr, int val) {
      qualifyAttribute("min_", attr);
      b.append(val);
      return this;
    }

    public Builder setMaxAttribute(String attr, int val) {
      qualifyAttribute("max_", attr);
      b.append(val);
      return this;
    }

    public Builder setNotAttribute(String attr, String val) {
      qualifyAttribute("not_", attr);
      b.append(val);
      return this;
    }

    public Builder setSince(long since) {
      if (since == -1L) {
        return this;
      }

      ampersand();
      b.append("_since=");
      b.append(since);
      return this;
    }

    public Builder setExcludeDeleted() {
      ampersand();
      b.append("not_deleted=true");
      return this;
    }
  }
}