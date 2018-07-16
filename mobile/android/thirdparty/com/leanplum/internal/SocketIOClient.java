// Copyright (c) 2009-2012 James Coglan
// Copyright (c) 2012 Eric Butler 
// Copyright (c) 2012 Koushik Dutta 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the 'Software'), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// From https://github.com/koush/android-websockets

package com.leanplum.internal;

import android.os.Looper;

import com.leanplum.Leanplum;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.thirdparty_unused.BuildConfig;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Arrays;
import java.util.HashSet;

class SocketIOClient {
  interface Handler {
    void onConnect();

    void on(String event, JSONArray arguments);

    void onDisconnect(int code, String reason);

    void onError(Exception error);
  }

  private String mURL;
  private Handler mHandler;
  private String mSession;
  private int mHeartbeat;
  private WebSocketClient mClient;
  private android.os.Handler mSendHandler;
  private Looper mSendLooper;

  public SocketIOClient(URI uri, Handler handler) {
    // remove trailing "/" from URI, in case user provided e.g. http://test.com/
    mURL = uri.toString().replaceAll("/$", "") + "/socket.io/1/";
    mHandler = handler;
  }

  private String downloadUriAsString()
          throws IOException {
    URL url = new URL(this.mURL);
    HttpURLConnection connection = (HttpURLConnection) url.openConnection();

    try {
      InputStream inputStream = connection.getInputStream();
      BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(inputStream));

      String tempStr;
      StringBuffer stringBuffer = new StringBuffer();

      while ((tempStr = bufferedReader.readLine()) != null) {
        stringBuffer.append(tempStr);
      }

      bufferedReader.close();
      inputStream.close();
      return stringBuffer.toString();

    } finally {
      if (connection != null) {
        connection.disconnect();
      }
    }
  }

  private static byte[] readToEndAsArray(InputStream input) throws IOException {
    DataInputStream dis = new DataInputStream(input);
    byte[] stuff = new byte[1024];
    ByteArrayOutputStream buff = new ByteArrayOutputStream();
    int read;
    while ((read = dis.read(stuff)) != -1) {
      buff.write(stuff, 0, read);
    }

    return buff.toByteArray();
  }

  private static String readToEnd(InputStream input) throws IOException {
    return new String(readToEndAsArray(input));
  }

  public void emit(String name, JSONArray args) throws JSONException {
    final JSONObject event = new JSONObject();
    event.put("name", name);
    event.put("args", args);
    // Log.d("Emitting event: " + event.toString());
    mSendHandler.post(new Runnable() {
      @Override
      public void run() {
        mClient.send(String.format("5:::%s", event.toString()));
      }
    });
  }

  private void connectSession() throws URISyntaxException {
    mClient = new WebSocketClient(new URI(mURL + "websocket/" + mSession),
        new WebSocketClient.Listener() {
          @Override
          public void onMessage(byte[] data) {
            cleanup();
            mHandler.onError(new Exception("Unexpected binary data"));
          }

          @Override
          public void onMessage(String message) {
            try {
              // Log.d("Message: " + message);
              String[] parts = message.split(":", 4);
              int code = Integer.parseInt(parts[0]);
              switch (code) {
                case 1:
                  onConnect();
                  break;
                case 2:
                  // heartbeat
                  mClient.send("2::");
                  break;
                case 3:
                  // message
                case 4:
                  // json message
                  throw new Exception("message type not supported");
                case 5: {
                  final String messageId = parts[1];
                  final String dataString = parts[3];
                  JSONObject data = new JSONObject(dataString);
                  String event = data.getString("name");
                  JSONArray args;
                  try {
                    args = data.getJSONArray("args");
                  } catch (JSONException e) {
                    args = new JSONArray();
                  }
                  if (!"".equals(messageId)) {
                    mSendHandler.post(new Runnable() {
                      @Override
                      public void run() {
                        mClient.send(String.format("6:::%s", messageId));
                      }
                    });
                  }
                  mHandler.on(event, args);
                  break;
                }
                case 6:
                  // ACK
                  break;
                case 7:
                  // error
                  throw new Exception(message);
                case 8:
                  // noop
                  break;
                default:
                  throw new Exception("unknown code");
              }
            } catch (Exception ex) {
              cleanup();
              onError(ex);
            }
          }

          @Override
          public void onError(Exception error) {
            cleanup();
            mHandler.onError(error);
          }

          @Override
          public void onDisconnect(int code, String reason) {
            cleanup();
            // attempt reconnect with same session?
            mHandler.onDisconnect(code, reason);
          }

          @Override
          public void onConnect() {
            mSendHandler.postDelayed(new Runnable() {
              @Override
              public void run() {
                mSendHandler.postDelayed(this, mHeartbeat);
                mClient.send("2:::");
              }
            }, mHeartbeat);
            mHandler.onConnect();
          }
        }, null);
    mClient.connect();
  }

  public void disconnect() throws IOException {
    cleanup();
  }

  private void cleanup() {
    if (mClient != null) {
      mClient.disconnect();
      mClient = null;
    }

    if (mSendLooper != null) {
      mSendLooper.quit();
    }
    mSendLooper = null;
    mSendHandler = null;
  }

  public void connect() {
    if (mClient != null)
      return;
    new Thread() {
      public void run() {

        try {
          String line = downloadUriAsString();
          String[] parts = line.split(":");
          mSession = parts[0];
          String heartbeat = parts[1];
          if (!"".equals(heartbeat))
            mHeartbeat = Integer.parseInt(heartbeat) / 2 * 1000;
          String transportsLine = parts[3];
          String[] transports = transportsLine.split(",");
          HashSet<String> set = new HashSet<>(Arrays.asList(transports));
          if (!set.contains("websocket"))
            throw new Exception("websocket not supported");

          Looper.prepare();
          mSendLooper = Looper.myLooper();
          mSendHandler = new android.os.Handler();

          connectSession();

          Looper.loop();
        } catch (Exception e) {
          mHandler.onError(e);
        }
      }
    }.start();
  }
}
