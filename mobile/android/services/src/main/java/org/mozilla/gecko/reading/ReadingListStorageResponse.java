/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.io.IOException;
import java.util.Iterator;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.UnexpectedJSONException;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * A storage response that contains multiple records.
 */
public class ReadingListStorageResponse extends ReadingListResponse {
  public static final ReadingListResponse.ResponseFactory<ReadingListStorageResponse> FACTORY = new ReadingListResponse.ResponseFactory<ReadingListStorageResponse>() {
    @Override
    public ReadingListStorageResponse getResponse(HttpResponse r) {
      return new ReadingListStorageResponse(r);
    }
  };

  private static final String LOG_TAG = "StorageResponse";

  public ReadingListStorageResponse(HttpResponse res) {
    super(res);
  }

  public Iterable<ServerReadingListRecord> getRecords() throws IOException, ParseException, UnexpectedJSONException {
    final ExtendedJSONObject body = jsonObjectBody();
    final JSONArray items = body.getArray("items");

    final int expected = getTotalRecords();
    final int actual = items.size();
    if (actual < expected) {
      Logger.warn(LOG_TAG, "Unexpected number of records. Got " + actual + ", expected " + expected);
    }

    return new Iterable<ServerReadingListRecord>() {
      @Override
      public Iterator<ServerReadingListRecord> iterator() {
        return new Iterator<ServerReadingListRecord>() {
          int position = 0;

          @Override
          public boolean hasNext() {
            return position < actual;
          }

          @Override
          public ServerReadingListRecord next() {
            final Object o = items.get(position++);
            return new ServerReadingListRecord(new ExtendedJSONObject((JSONObject) o));
          }

          @Override
          public void remove() {
            throw new RuntimeException("Cannot remove from iterator.");
          }
        };
      }
    };
  }

  public int getTotalRecords() {
    return getIntegerHeader("Total-Records");
  }
}