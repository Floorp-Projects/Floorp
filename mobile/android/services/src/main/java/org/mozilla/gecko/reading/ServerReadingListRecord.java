/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public class ServerReadingListRecord extends ReadingListRecord {
  final ExtendedJSONObject fields;

  public ServerReadingListRecord(ExtendedJSONObject obj) {
    super(new ServerMetadata(obj));
    this.fields = obj.deepCopy();
  }

  @Override
  public String getURL() {
    return this.fields.getString("url");    // TODO: resolved_url
  }

  @Override
  public String getTitle() {
    return this.fields.getString("title");  // TODO: resolved_title
  }

  @Override
  public String getAddedBy() {
    return this.fields.getString("added_by");
  }

  public String getExcerpt() {
    return this.fields.getString("excerpt");
  }
}
