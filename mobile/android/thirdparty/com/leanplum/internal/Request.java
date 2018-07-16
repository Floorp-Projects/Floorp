/*
 * Copyright 2013, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum.internal;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import com.leanplum.Leanplum;
import com.leanplum.utils.SharedPreferencesUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.EOFException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;
import java.util.UUID;

/**
 * Leanplum request class.
 *
 * @author Andrew First
 */
public class Request {
  private static final long DEVELOPMENT_MIN_DELAY_MS = 100;
  private static final long DEVELOPMENT_MAX_DELAY_MS = 5000;
  private static final long PRODUCTION_DELAY = 60000;
  static final int MAX_EVENTS_PER_API_CALL;
  static final String LEANPLUM = "__leanplum__";
  static final String UUID_KEY = "uuid";

  private static String appId;
  private static String accessKey;
  private static String deviceId;
  private static String userId;

  private static final LeanplumEventCallbackManager eventCallbackManager =
      new LeanplumEventCallbackManager();
  private static final Map<String, Boolean> fileTransferStatus = new HashMap<>();
  private static int pendingDownloads;
  private static NoPendingDownloadsCallback noPendingDownloadsBlock;

  // The token is saved primarily for legacy SharedPreferences decryption. This could
  // likely be removed in the future.
  private static String token = null;
  private static final Map<File, Long> fileUploadSize = new HashMap<>();
  private static final Map<File, Double> fileUploadProgress = new HashMap<>();
  private static String fileUploadProgressString = "";
  private static long lastSendTimeMs;
  private static final Object uploadFileLock = new Object();

  private final String httpMethod;
  private final String apiMethod;
  private final Map<String, Object> params;
  private ResponseCallback response;
  private ErrorCallback error;
  private boolean sent;
  private long dataBaseIndex;

  private static ApiResponseCallback apiResponse;

  private static List<Map<String, Object>> localErrors = new ArrayList<>();

  static {
    if (Build.VERSION.SDK_INT <= 17) {
      MAX_EVENTS_PER_API_CALL = 5000;
    } else {
      MAX_EVENTS_PER_API_CALL = 10000;
    }
  }

  public static void setAppId(String appId, String accessKey) {
    if (!TextUtils.isEmpty(appId)) {
      Request.appId = appId.trim();
    }
    if (!TextUtils.isEmpty(accessKey)) {
      Request.accessKey = accessKey.trim();
    }
  }

  public static void setDeviceId(String deviceId) {
    Request.deviceId = deviceId;
  }

  public static void setUserId(String userId) {
    Request.userId = userId;
  }

  public static void setToken(String token) {
    Request.token = token;
  }

  public static String token() {
    return token;
  }

  /**
   * Since requests are batched there can be a case where other Request can take future Request
   * events. We need to have for each Request database index for handle response, error or start
   * callbacks.
   *
   * @return Index of event at database.
   */
  public long getDataBaseIndex() {
    return dataBaseIndex;
  }

  // Update index of event at database.
  public void setDataBaseIndex(long dataBaseIndex) {
    this.dataBaseIndex = dataBaseIndex;
  }

  public static void loadToken() {
    Context context = Leanplum.getContext();
    SharedPreferences defaults = context.getSharedPreferences(
        LEANPLUM, Context.MODE_PRIVATE);
    String token = defaults.getString(Constants.Defaults.TOKEN_KEY, null);
    if (token == null) {
      return;
    }
    setToken(token);
  }

  public static void saveToken() {
    Context context = Leanplum.getContext();
    SharedPreferences defaults = context.getSharedPreferences(
        LEANPLUM, Context.MODE_PRIVATE);
    SharedPreferences.Editor editor = defaults.edit();
    editor.putString(Constants.Defaults.TOKEN_KEY, Request.token());
    SharedPreferencesUtil.commitChanges(editor);
  }

  public static String appId() {
    return appId;
  }

  public static String deviceId() {
    return deviceId;
  }

  public static String userId() {
    return Request.userId;
  }

  public Request(String httpMethod, String apiMethod, Map<String, Object> params) {
    this.httpMethod = httpMethod;
    this.apiMethod = apiMethod;
    this.params = params != null ? params : new HashMap<String, Object>();
    // Check if it is error and here was SQLite exception.
    if (Constants.Methods.LOG.equals(apiMethod) && LeanplumEventDataManager.willSendErrorLog) {
      localErrors.add(createArgsDictionary());
    }
    // Make sure the Handler is initialized on the main thread.
    OsHandler.getInstance();
    dataBaseIndex = -1;
  }

  public static Request get(String apiMethod, Map<String, Object> params) {
    Log.LeanplumLogType level = Constants.Methods.LOG.equals(apiMethod) ?
        Log.LeanplumLogType.DEBUG : Log.LeanplumLogType.VERBOSE;
    Log.log(level, "Will call API method " + apiMethod + " with arguments " + params);
    return RequestFactory.getInstance().createRequest("GET", apiMethod, params);
  }

  public static Request post(String apiMethod, Map<String, Object> params) {
    Log.LeanplumLogType level = Constants.Methods.LOG.equals(apiMethod) ?
        Log.LeanplumLogType.DEBUG : Log.LeanplumLogType.VERBOSE;
    Log.log(level, "Will call API method " + apiMethod + " with arguments " + params);
    return RequestFactory.getInstance().createRequest("POST", apiMethod, params);
  }

  public void onResponse(ResponseCallback response) {
    this.response = response;
  }

  public void onError(ErrorCallback error) {
    this.error = error;
  }

  public void onApiResponse(ApiResponseCallback apiResponse) {
    Request.apiResponse = apiResponse;
  }

  private Map<String, Object> createArgsDictionary() {
    Map<String, Object> args = new HashMap<>();
    args.put(Constants.Params.DEVICE_ID, deviceId);
    args.put(Constants.Params.USER_ID, userId);
    args.put(Constants.Params.ACTION, apiMethod);
    args.put(Constants.Params.SDK_VERSION, Constants.LEANPLUM_VERSION);
    args.put(Constants.Params.DEV_MODE, Boolean.toString(Constants.isDevelopmentModeEnabled));
    args.put(Constants.Params.TIME, Double.toString(new Date().getTime() / 1000.0));
    if (token != null) {
      args.put(Constants.Params.TOKEN, token);
    }
    args.putAll(params);
    return args;
  }

  private void saveRequestForLater(Map<String, Object> args) {
    synchronized (Request.class) {
      Context context = Leanplum.getContext();
      SharedPreferences preferences = context.getSharedPreferences(
          LEANPLUM, Context.MODE_PRIVATE);
      SharedPreferences.Editor editor = preferences.edit();
      long count = LeanplumEventDataManager.getEventsCount();
      String uuid = preferences.getString(Constants.Defaults.UUID_KEY, null);
      if (uuid == null || count % MAX_EVENTS_PER_API_CALL == 0) {
        uuid = UUID.randomUUID().toString();
        editor.putString(Constants.Defaults.UUID_KEY, uuid);
        SharedPreferencesUtil.commitChanges(editor);
      }
      args.put(UUID_KEY, uuid);
      LeanplumEventDataManager.insertEvent(JsonConverter.toJson(args));

      dataBaseIndex = count;
      // Checks if here response and/or error callback for this request. We need to add callbacks to
      // eventCallbackManager only if here was internet connection, otherwise triggerErrorCallback
      // will handle error callback for this event.
      if (response != null || error != null && !Util.isConnected()) {
        eventCallbackManager.addCallbacks(this, response, error);
      }
    }
  }

  public void send() {
    this.sendEventually();
    if (Constants.isDevelopmentModeEnabled) {
      long currentTimeMs = System.currentTimeMillis();
      long delayMs;
      if (lastSendTimeMs == 0 || currentTimeMs - lastSendTimeMs > DEVELOPMENT_MAX_DELAY_MS) {
        delayMs = DEVELOPMENT_MIN_DELAY_MS;
      } else {
        delayMs = (lastSendTimeMs + DEVELOPMENT_MAX_DELAY_MS) - currentTimeMs;
      }
      OsHandler.getInstance().postDelayed(new Runnable() {
        @Override
        public void run() {
          try {
            sendIfConnected();
          } catch (Throwable t) {
            Util.handleException(t);
          }
        }
      }, delayMs);
    }
  }

  /**
   * Wait 1 second for potential other API calls, and then sends the call synchronously if no other
   * call has been sent within 1 minute.
   */
  public void sendIfDelayed() {
    sendEventually();
    OsHandler.getInstance().postDelayed(new Runnable() {
      @Override
      public void run() {
        try {
          sendIfDelayedHelper();
        } catch (Throwable t) {
          Util.handleException(t);
        }
      }
    }, 1000);
  }

  /**
   * Sends the call synchronously if no other call has been sent within 1 minute.
   */
  private void sendIfDelayedHelper() {
    if (Constants.isDevelopmentModeEnabled) {
      send();
    } else {
      long currentTimeMs = System.currentTimeMillis();
      if (lastSendTimeMs == 0 || currentTimeMs - lastSendTimeMs > PRODUCTION_DELAY) {
        sendIfConnected();
      }
    }
  }

  public void sendIfConnected() {
    if (Util.isConnected()) {
      this.sendNow();
    } else {
      this.sendEventually();
      Log.i("Device is offline, will send later");
      triggerErrorCallback(new Exception("Not connected to the Internet"));
    }
  }

  private void triggerErrorCallback(Exception e) {
    if (error != null) {
      error.error(e);
    }
    if (apiResponse != null) {
      List<Map<String, Object>> requests = getUnsentRequests();
      List<Map<String, Object>> requestsToSend = removeIrrelevantBackgroundStartRequests(requests);
      apiResponse.response(requestsToSend, null, requests.size());
    }
  }

  @SuppressWarnings("BooleanMethodIsAlwaysInverted")
  private static boolean attachApiKeys(Map<String, Object> dict) {
    if (appId == null || accessKey == null) {
      Log.e("API keys are not set. Please use Leanplum.setAppIdForDevelopmentMode or "
          + "Leanplum.setAppIdForProductionMode.");
      return false;
    }
    dict.put(Constants.Params.APP_ID, appId);
    dict.put(Constants.Params.CLIENT_KEY, accessKey);
    dict.put(Constants.Params.CLIENT, Constants.CLIENT);
    return true;
  }

  public interface ResponseCallback {
    void response(JSONObject response);
  }

  public interface ApiResponseCallback {
    void response(List<Map<String, Object>> requests, JSONObject response, int countOfEvents);
  }

  public interface ErrorCallback {
    void error(Exception e);
  }

  public interface NoPendingDownloadsCallback {
    void noPendingDownloads();
  }

  /**
   * Parse response body from server.  Invoke potential error or response callbacks for all events
   * of this request.
   *
   * @param responseBody JSONObject with response body from server.
   * @param requestsToSend List of requests that were sent to the server/
   * @param error Exception.
   * @param unsentRequestsSize Size of unsent request, that we will delete.
   */
  private void parseResponseBody(JSONObject responseBody, List<Map<String, Object>>
      requestsToSend, Exception error, int unsentRequestsSize) {
    synchronized (Request.class) {
      if (responseBody == null && error != null) {
        // Invoke potential error callbacks for all events of this request.
        eventCallbackManager.invokeAllCallbacksWithError(error, unsentRequestsSize);
        return;
      } else if (responseBody == null) {
        return;
      }

      // Response for last start call.
      if (apiResponse != null) {
        apiResponse.response(requestsToSend, responseBody, unsentRequestsSize);
      }

      // We will replace it with error from response body, if we found it.
      Exception lastResponseError = error;
      // Valid response, parse and handle response body.
      int numResponses = Request.numResponses(responseBody);
      for (int i = 0; i < numResponses; i++) {
        JSONObject response = Request.getResponseAt(responseBody, i);
        if (Request.isResponseSuccess(response)) {
          continue; // If event response is successful, proceed with next one.
        }

        // If event response was not successful, handle error.
        String errorMessage = getReadableErrorMessage(Request.getResponseError(response));
        Log.e(errorMessage);
        // Throw an exception if last event response is negative.
        if (i == numResponses - 1) {
          lastResponseError = new Exception(errorMessage);
        }
      }

      if (lastResponseError != null) {
        // Invoke potential error callbacks for all events of this request.
        eventCallbackManager.invokeAllCallbacksWithError(lastResponseError, unsentRequestsSize);
      } else {
        // Invoke potential response callbacks for all events of this request.
        eventCallbackManager.invokeAllCallbacksForResponse(responseBody, unsentRequestsSize);
      }
    }
  }

  /**
   * Parse error message from server response and return readable error message.
   *
   * @param errorMessage String of error from server response.
   * @return String of readable error message.
   */
  @NonNull
  private String getReadableErrorMessage(String errorMessage) {
    if (errorMessage == null || errorMessage.length() == 0) {
      errorMessage = "API error";
    } else if (errorMessage.startsWith("App not found")) {
      errorMessage = "No app matching the provided app ID was found.";
      Constants.isInPermanentFailureState = true;
    } else if (errorMessage.startsWith("Invalid access key")) {
      errorMessage = "The access key you provided is not valid for this app.";
      Constants.isInPermanentFailureState = true;
    } else if (errorMessage.startsWith("Development mode requested but not permitted")) {
      errorMessage = "A call to Leanplum.setAppIdForDevelopmentMode "
          + "with your production key was made, which is not permitted.";
      Constants.isInPermanentFailureState = true;
    } else {
      errorMessage = "API error: " + errorMessage;
    }
    return errorMessage;
  }

  private void sendNow() {
    if (Constants.isTestMode) {
      return;
    }
    if (appId == null) {
      Log.e("Cannot send request. appId is not set.");
      return;
    }
    if (accessKey == null) {
      Log.e("Cannot send request. accessKey is not set.");
      return;
    }

    this.sendEventually();

    Util.executeAsyncTask(true, new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... params) {
        sendRequests();
        return null;
      }
    });
  }

  private void sendRequests() {
    List<Map<String, Object>> unsentRequests = new ArrayList<>();
    List<Map<String, Object>> requestsToSend;
    // Check if we have localErrors, if yes then we will send only errors to the server.
    if (localErrors.size() != 0) {
      String uuid = UUID.randomUUID().toString();
      for (Map<String, Object> error : localErrors) {
        error.put(UUID_KEY, uuid);
        unsentRequests.add(error);
      }
      requestsToSend = unsentRequests;
    } else {
      unsentRequests = getUnsentRequests();
      requestsToSend = removeIrrelevantBackgroundStartRequests(unsentRequests);
    }

    if (requestsToSend.isEmpty()) {
      return;
    }

    final Map<String, Object> multiRequestArgs = new HashMap<>();
    if (!Request.attachApiKeys(multiRequestArgs)) {
      return;
    }
    multiRequestArgs.put(Constants.Params.DATA, jsonEncodeUnsentRequests(requestsToSend));
    multiRequestArgs.put(Constants.Params.SDK_VERSION, Constants.LEANPLUM_VERSION);
    multiRequestArgs.put(Constants.Params.ACTION, Constants.Methods.MULTI);
    multiRequestArgs.put(Constants.Params.TIME, Double.toString(new Date().getTime() / 1000.0));

    JSONObject responseBody;
    HttpURLConnection op = null;
    try {
      try {
        op = Util.operation(
            Constants.API_HOST_NAME,
            Constants.API_SERVLET,
            multiRequestArgs,
            httpMethod,
            Constants.API_SSL,
            Constants.NETWORK_TIMEOUT_SECONDS);

        responseBody = Util.getJsonResponse(op);
        int statusCode = op.getResponseCode();

        Exception errorException;
        if (statusCode >= 200 && statusCode <= 299) {
          if (responseBody == null) {
            errorException = new Exception("Response JSON is null.");
            deleteSentRequests(unsentRequests.size());
            parseResponseBody(null, requestsToSend, errorException, unsentRequests.size());
            return;
          }

          Exception exception = null;
          // Checks if we received the same number of responses as a number of sent request.
          int numResponses = Request.numResponses(responseBody);
          if (numResponses != requestsToSend.size()) {
            Log.w("Sent " + requestsToSend.size() + " requests but only" +
                " received " + numResponses);
          }
          parseResponseBody(responseBody, requestsToSend, null, unsentRequests.size());
          // Clear localErrors list.
          localErrors.clear();
          deleteSentRequests(unsentRequests.size());

          // Send another request if the last request had maximum events per api call.
          if (unsentRequests.size() == MAX_EVENTS_PER_API_CALL) {
            sendRequests();
          }
        } else {
          errorException = new Exception("HTTP error " + statusCode);
          if (statusCode != -1 && statusCode != 408 && !(statusCode >= 500 && statusCode <= 599)) {
            deleteSentRequests(unsentRequests.size());
            parseResponseBody(responseBody, requestsToSend, errorException, unsentRequests.size());
          }
        }
      } catch (JSONException e) {
        Log.e("Error parsing JSON response: " + e.toString() + "\n" + Log.getStackTraceString(e));
        deleteSentRequests(unsentRequests.size());
        parseResponseBody(null, requestsToSend, e, unsentRequests.size());
      } catch (Exception e) {
        Log.e("Unable to send request: " + e.toString() + "\n" + Log.getStackTraceString(e));
      } finally {
        if (op != null) {
          op.disconnect();
        }
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  public void sendEventually() {
    if (Constants.isTestMode) {
      return;
    }

    if (LeanplumEventDataManager.willSendErrorLog) {
      return;
    }

    if (!sent) {
      sent = true;
      Map<String, Object> args = createArgsDictionary();
      saveRequestForLater(args);
    }
  }

  static void deleteSentRequests(int requestsCount) {
    if (requestsCount == 0) {
      return;
    }
    synchronized (Request.class) {
      LeanplumEventDataManager.deleteEvents(requestsCount);
    }
  }

  private static List<Map<String, Object>> getUnsentRequests() {
    List<Map<String, Object>> requestData;

    synchronized (Request.class) {
      lastSendTimeMs = System.currentTimeMillis();
      Context context = Leanplum.getContext();
      SharedPreferences preferences = context.getSharedPreferences(
          LEANPLUM, Context.MODE_PRIVATE);
      SharedPreferences.Editor editor = preferences.edit();

      requestData = LeanplumEventDataManager.getEvents(MAX_EVENTS_PER_API_CALL);
      editor.remove(Constants.Defaults.UUID_KEY);
      SharedPreferencesUtil.commitChanges(editor);
    }

    return requestData;
  }

  /**
   * In various scenarios we can end up batching a big number of requests (e.g. device is offline,
   * background sessions), which could make the stored API calls batch look something like:
   * <p>
   * <code>start(B), start(B), start(F), track, start(B), track, start(F), resumeSession</code>
   * <p>
   * where <code>start(B)</code> indicates a start in the background, and <code>start(F)</code>
   * one in the foreground.
   * <p>
   * In this case the first two <code>start(B)</code> can be dropped because they don't contribute
   * any relevant information for the batch call.
   * <p>
   * Essentially we drop every <code>start(B)</code> call, that is directly followed by any kind of
   * a <code>start</code> call.
   *
   * @param requestData A list of the requests, stored on the device.
   * @return A list of only these requests, which contain relevant information for the API call.
   */
  private static List<Map<String, Object>> removeIrrelevantBackgroundStartRequests(
      List<Map<String, Object>> requestData) {
    List<Map<String, Object>> relevantRequests = new ArrayList<>();

    int requestCount = requestData.size();
    if (requestCount > 0) {
      for (int i = 0; i < requestCount; i++) {
        Map<String, Object> currentRequest = requestData.get(i);
        if (i < requestCount - 1
            && Constants.Methods.START.equals(requestData.get(i + 1).get(Constants.Params.ACTION))
            && Constants.Methods.START.equals(currentRequest.get(Constants.Params.ACTION))
            && Boolean.TRUE.toString().equals(currentRequest.get(Constants.Params.BACKGROUND))) {
          continue;
        }
        relevantRequests.add(currentRequest);
      }
    }

    return relevantRequests;
  }

  private static String jsonEncodeUnsentRequests(List<Map<String, Object>> requestData) {
    Map<String, Object> data = new HashMap<>();
    data.put(Constants.Params.DATA, requestData);
    return JsonConverter.toJson(data);
  }


  private static String getSizeAsString(int bytes) {
    if (bytes < (1 << 10)) {
      return bytes + " B";
    } else if (bytes < (1 << 20)) {
      return (bytes >> 10) + " KB";
    } else {
      return (bytes >> 20) + " MB";
    }
  }

  private static void printUploadProgress() {
    int totalFiles = fileUploadSize.size();
    int sentFiles = 0;
    int totalBytes = 0;
    int sentBytes = 0;
    for (Map.Entry<File, Long> entry : fileUploadSize.entrySet()) {
      File file = entry.getKey();
      long fileSize = entry.getValue();
      double fileProgress = fileUploadProgress.get(file);
      if (fileProgress == 1) {
        sentFiles++;
      }
      sentBytes += (int) (fileSize * fileProgress);
      totalBytes += fileSize;
    }
    String progressString = "Uploading resources. " +
        sentFiles + '/' + totalFiles + " files completed; " +
        getSizeAsString(sentBytes) + '/' + getSizeAsString(totalBytes) + " transferred.";
    if (!fileUploadProgressString.equals(progressString)) {
      fileUploadProgressString = progressString;
      Log.i(progressString);
    }
  }

  public void sendFilesNow(final List<String> filenames, final List<InputStream> streams) {
    if (Constants.isTestMode) {
      return;
    }
    final Map<String, Object> dict = createArgsDictionary();
    if (!attachApiKeys(dict)) {
      return;
    }
    final List<File> filesToUpload = new ArrayList<>();

    // First set up the files for upload
    for (int i = 0; i < filenames.size(); i++) {
      String filename = filenames.get(i);
      if (filename == null || Boolean.TRUE.equals(fileTransferStatus.get(filename))) {
        continue;
      }
      File file = new File(filename);
      long size;
      try {
        size = streams.get(i).available();
      } catch (IOException e) {
        size = file.length();
      } catch (NullPointerException e) {
        // Not good. Can't read asset.
        Log.e("Unable to read file " + filename);
        continue;
      }
      fileTransferStatus.put(filename, true);
      filesToUpload.add(file);
      fileUploadSize.put(file, size);
      fileUploadProgress.put(file, 0.0);
    }
    if (filesToUpload.size() == 0) {
      return;
    }

    printUploadProgress();

    // Now upload the files
    Util.executeAsyncTask(false, new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... params) {
        synchronized (uploadFileLock) {  // Don't overload app and server with many upload tasks
          JSONObject result;
          HttpURLConnection op = null;

          try {
            op = Util.uploadFilesOperation(
                Constants.Params.FILE,
                filesToUpload,
                streams,
                Constants.API_HOST_NAME,
                Constants.API_SERVLET,
                dict,
                httpMethod,
                Constants.API_SSL,
                60);

            if (op != null) {
              result = Util.getJsonResponse(op);
              int statusCode = op.getResponseCode();
              if (statusCode != 200) {
                throw new Exception("Leanplum: Error sending request: " + statusCode);
              }
              if (Request.this.response != null) {
                Request.this.response.response(result);
              }
            } else {
              if (error != null) {
                error.error(new Exception("Leanplum: Unable to read file."));
              }
            }
          } catch (JSONException e) {
            Log.e("Unable to convert to JSON.", e);
            if (error != null) {
              error.error(e);
            }
          } catch (SocketTimeoutException e) {
            Log.e("Timeout uploading files. Try again or limit the number of files " +
                "to upload with parameters to syncResourcesAsync.");
            if (error != null) {
              error.error(e);
            }
          } catch (Exception e) {
            Log.e("Unable to send file.", e);
            if (error != null) {
              error.error(e);
            }
          } finally {
            if (op != null) {
              op.disconnect();
            }
          }

          for (File file : filesToUpload) {
            fileUploadProgress.put(file, 1.0);
          }
          printUploadProgress();

          return null;
        }
      }
    });

    // TODO: Upload progress
  }

  void downloadFile(final String path, final String url) {
    if (Constants.isTestMode) {
      return;
    }
    if (Boolean.TRUE.equals(fileTransferStatus.get(path))) {
      return;
    }
    pendingDownloads++;
    Log.i("Downloading resource " + path);
    fileTransferStatus.put(path, true);
    final Map<String, Object> dict = createArgsDictionary();
    dict.put(Constants.Keys.FILENAME, path);
    if (!attachApiKeys(dict)) {
      return;
    }

    Util.executeAsyncTask(false, new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... params) {
        try {
          downloadHelper(Constants.API_HOST_NAME, Constants.API_SERVLET, path, url, dict);
        } catch (Throwable t) {
          Util.handleException(t);
        }
        return null;
      }
    });
    // TODO: Download progress
  }

  private void downloadHelper(String hostName, String servlet, final String path, final String url,
      final Map<String, Object> dict) {
    HttpURLConnection op = null;
    URL originalURL = null;
    try {
      if (url == null) {
        op = Util.operation(
            hostName,
            servlet,
            dict,
            httpMethod,
            Constants.API_SSL,
            Constants.NETWORK_TIMEOUT_SECONDS_FOR_DOWNLOADS);
      } else {
        op = Util.createHttpUrlConnection(url, httpMethod, url.startsWith("https://"),
            Constants.NETWORK_TIMEOUT_SECONDS_FOR_DOWNLOADS);
      }
      originalURL = op.getURL();
      op.connect();
      int statusCode = op.getResponseCode();
      if (statusCode != 200) {
        throw new Exception("Leanplum: Error sending request to: " + hostName +
            ", HTTP status code: " + statusCode);
      }
      Stack<String> dirs = new Stack<>();
      String currentDir = path;
      while ((currentDir = new File(currentDir).getParent()) != null) {
        dirs.push(currentDir);
      }
      while (!dirs.isEmpty()) {
        String directory = FileManager.fileRelativeToDocuments(dirs.pop());
        boolean isCreated = new File(directory).mkdir();
        if (!isCreated) {
          Log.w("Failed to create directory: ", directory);
        }
      }

      FileOutputStream out = new FileOutputStream(
          new File(FileManager.fileRelativeToDocuments(path)));
      Util.saveResponse(op, out);
      pendingDownloads--;
      if (Request.this.response != null) {
        Request.this.response.response(null);
      }
      if (pendingDownloads == 0 && noPendingDownloadsBlock != null) {
        noPendingDownloadsBlock.noPendingDownloads();
      }
    } catch (Exception e) {
      if (e instanceof EOFException) {
        if (op != null && !op.getURL().equals(originalURL)) {
          downloadHelper(null, op.getURL().toString(), path, url, new HashMap<String, Object>());
          return;
        }
      }
      Log.e("Error downloading resource:" + path, e);
      pendingDownloads--;
      if (error != null) {
        error.error(e);
      }
      if (pendingDownloads == 0 && noPendingDownloadsBlock != null) {
        noPendingDownloadsBlock.noPendingDownloads();
      }
    } finally {
      if (op != null) {
        op.disconnect();
      }
    }
  }

  public static int numPendingDownloads() {
    return pendingDownloads;
  }

  public static void onNoPendingDownloads(NoPendingDownloadsCallback block) {
    noPendingDownloadsBlock = block;
  }


  public static int numResponses(JSONObject response) {
    if (response == null) {
      return 0;
    }
    try {
      return response.getJSONArray("response").length();
    } catch (JSONException e) {
      Log.e("Could not parse JSON response.", e);
      return 0;
    }
  }

  public static JSONObject getResponseAt(JSONObject response, int index) {
    try {
      return response.getJSONArray("response").getJSONObject(index);
    } catch (JSONException e) {
      Log.e("Could not parse JSON response.", e);
      return null;
    }
  }

  public static JSONObject getLastResponse(JSONObject response) {
    int numResponses = numResponses(response);
    if (numResponses > 0) {
      return getResponseAt(response, numResponses - 1);
    } else {
      return null;
    }
  }

  public static boolean isResponseSuccess(JSONObject response) {
    if (response == null) {
      return false;
    }
    try {
      return response.getBoolean("success");
    } catch (JSONException e) {
      Log.e("Could not parse JSON response.", e);
      return false;
    }
  }

  public static String getResponseError(JSONObject response) {
    if (response == null) {
      return null;
    }
    try {
      JSONObject error = response.optJSONObject("error");
      if (error == null) {
        return null;
      }
      return error.getString("message");
    } catch (JSONException e) {
      Log.e("Could not parse JSON response.", e);
      return null;
    }
  }
}
