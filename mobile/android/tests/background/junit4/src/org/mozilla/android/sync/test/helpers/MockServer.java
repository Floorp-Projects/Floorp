/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Utils;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;
import org.simpleframework.http.core.Container;

import java.io.IOException;
import java.io.PrintStream;

import static org.junit.Assert.assertEquals;

public class MockServer implements Container {
  public static final String LOG_TAG = "MockServer";

  public int statusCode = 200;
  public String body = "Hello World";

  public MockServer() {
  }

  public MockServer(int statusCode, String body) {
    this.statusCode = statusCode;
    this.body = body;
  }

  public String expectedBasicAuthHeader;

  protected PrintStream handleBasicHeaders(Request request, Response response, int code, String contentType) throws IOException {
    return this.handleBasicHeaders(request, response, code, contentType, System.currentTimeMillis());
  }

  protected PrintStream handleBasicHeaders(Request request, Response response, int code, String contentType, long time) throws IOException {
    Logger.debug(LOG_TAG, "< Auth header: " + request.getValue("Authorization"));

    PrintStream bodyStream = response.getPrintStream();
    response.setCode(code);
    response.setValue("Content-Type", contentType);
    response.setValue("Server", "HelloWorld/1.0 (Simple 4.0)");
    response.setDate("Date", time);
    response.setDate("Last-Modified", time);

    final String timestampHeader = Utils.millisecondsToDecimalSecondsString(time);
    response.setValue("X-Weave-Timestamp", timestampHeader);
    Logger.debug(LOG_TAG, "> X-Weave-Timestamp header: " + timestampHeader);
    return bodyStream;
  }

  protected void handle(Request request, Response response, int code, String body) {
    try {
      Logger.debug(LOG_TAG, "Handling request...");
      PrintStream bodyStream = this.handleBasicHeaders(request, response, code, "application/json");

      if (expectedBasicAuthHeader != null) {
        Logger.debug(LOG_TAG, "Expecting auth header " + expectedBasicAuthHeader);
        assertEquals(request.getValue("Authorization"), expectedBasicAuthHeader);
      }

      bodyStream.println(body);
      bodyStream.close();
    } catch (IOException e) {
      Logger.error(LOG_TAG, "Oops.");
    }
  }
  public void handle(Request request, Response response) {
    this.handle(request, response, statusCode, body);
  }
}
