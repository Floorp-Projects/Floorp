/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.NonObjectJSONException;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * A storage response that contains a single record.
 */
public class ReadingListRecordResponse extends ReadingListResponse {
  @Override
  public boolean wasSuccessful() {
    final int code = getStatusCode();
    if (code == 200 || code == 201 || code == 204) {
      return true;
    }
    return super.wasSuccessful();
  }

  public static final ReadingListResponse.ResponseFactory<ReadingListRecordResponse> FACTORY = new ReadingListResponse.ResponseFactory<ReadingListRecordResponse>() {
    @Override
    public ReadingListRecordResponse getResponse(HttpResponse r) {
      return new ReadingListRecordResponse(r);
    }
  };

  public ReadingListRecordResponse(HttpResponse res) {
    super(res);
  }

  public ServerReadingListRecord getRecord() throws IllegalStateException, NonObjectJSONException, IOException, ParseException {
    return new ServerReadingListRecord(jsonObjectBody());
  }
}