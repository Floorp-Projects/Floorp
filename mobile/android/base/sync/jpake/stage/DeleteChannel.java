/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncResourceDelegate;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class DeleteChannel {
  private static final String LOG_TAG = "DeleteChannel";

  public static final String KEYEXCHANGE_ID_HEADER  = "X-KeyExchange-Id";
  public static final String KEYEXCHANGE_CID_HEADER = "X-KeyExchange-Cid";

  public void execute(final JPakeClient jClient, final String reason) {
    final BaseResource httpResource;
    try {
      httpResource = new BaseResource(jClient.channelUrl);
    } catch (URISyntaxException e) {
      Logger.debug(LOG_TAG, "Encountered URISyntax exception, displaying abort anyway.");
      jClient.displayAbort(reason);
      return;
    }
    httpResource.delegate = new SyncResourceDelegate(httpResource) {

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        request.setHeader(new BasicHeader(KEYEXCHANGE_ID_HEADER,  jClient.clientId));
        request.setHeader(new BasicHeader(KEYEXCHANGE_CID_HEADER, jClient.channel));
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        try {
          int statusCode = response.getStatusLine().getStatusCode();
          switch (statusCode) {
            case 200:
              Logger.info(LOG_TAG, "Successfully reported error to server.");
              break;
            case 403:
              Logger.info(LOG_TAG, "IP is blacklisted.");
              break;
            case 400:
              Logger.info(LOG_TAG, "Bad request (missing logs, or bad ids");
              break;
            default:
              Logger.info(LOG_TAG, "Server returned " + statusCode);
          }
        } finally {
          BaseResource.consumeEntity(response);
          // Always call displayAbort, even if abort fails. We can't do anything about it.
          jClient.displayAbort(reason);
        }
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        Logger.debug(LOG_TAG, "Encountered HttpProtocolException, displaying abort anyway.");
        jClient.displayAbort(reason);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        Logger.debug(LOG_TAG, "Encountered IOException, displaying abort anyway.");
        jClient.displayAbort(reason);
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        Logger.debug(LOG_TAG, "Encountered GeneralSecurityException, displaying abort anyway.");
        jClient.displayAbort(reason);
      }
    };

    JPakeClient.runOnThread(new Runnable() {
      @Override
      public void run() {
        httpResource.delete();
      }
    });
  }
}
