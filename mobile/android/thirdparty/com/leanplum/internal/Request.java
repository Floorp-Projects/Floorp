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

import com.leanplum.Leanplum;

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
import java.util.Locale;
import java.util.Map;
import java.util.Stack;

/**
 * Leanplum request class.
 *
 * @author Andrew First
 */
public class Request {
  private static final long DEVELOPMENT_MIN_DELAY_MS = 100;
  private static final long DEVELOPMENT_MAX_DELAY_MS = 5000;
  private static final long PRODUCTION_DELAY = 60000;
  private static final String LEANPLUM = "__leanplum__";

  private static String appId;
  private static String accessKey;
  private static String deviceId;
  private static String userId;
  private static final Map<String, Boolean> fileTransferStatus = new HashMap<>();
  private static int pendingDownloads;
  private static NoPendingDownloadsCallback noPendingDownloadsBlock;

  // The token is saved primarily for legacy SharedPreferences decryption. This could
  // likely be removed in the future.
  private static String token = null;
  private static final Object lock = Request.class;
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

  private static ApiResponseCallback apiResponse;

  public static void setAppId(String appId, String accessKey) {
    Request.appId = appId;
    Request.accessKey = accessKey;
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
    try {
      editor.apply();
    } catch (NoSuchMethodError e) {
      editor.commit();
    }
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

    // Make sure the Handler is initialized on the main thread.
    OsHandler.getInstance();
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

  private static void saveRequestForLater(Map<String, Object> args) {
    synchronized (lock) {
      Context context = Leanplum.getContext();
      SharedPreferences preferences = context.getSharedPreferences(
          LEANPLUM, Context.MODE_PRIVATE);
      SharedPreferences.Editor editor = preferences.edit();
      int count = preferences.getInt(Constants.Defaults.COUNT_KEY, 0);
      String itemKey = String.format(Locale.US, Constants.Defaults.ITEM_KEY, count);
      editor.putString(itemKey, JsonConverter.toJson(args));
      count++;
      editor.putInt(Constants.Defaults.COUNT_KEY, count);
      try {
        editor.apply();
      } catch (NoSuchMethodError e) {
        editor.commit();
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
      apiResponse.response(requests, null);
    }
  }

  @SuppressWarnings("BooleanMethodIsAlwaysInverted")
  private boolean attachApiKeys(Map<String, Object> dict) {
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
    void response(List<Map<String, Object>> requests, JSONObject response);
  }

  public interface ErrorCallback {
    void error(Exception e);
  }

  public interface NoPendingDownloadsCallback {
    void noPendingDownloads();
  }

  private void parseResponseJson(JSONObject responseJson, List<Map<String, Object>> requestsToSend,
      Exception error) {
    if (apiResponse != null) {
      apiResponse.response(requestsToSend, responseJson);
    }

    if (responseJson != null) {
      Exception lastResponseError = null;
      int numResponses = Request.numResponses(responseJson);
      for (int i = 0; i < numResponses; i++) {
        JSONObject response = Request.getResponseAt(responseJson, i);
        if (!Request.isResponseSuccess(response)) {
          String errorMessage = Request.getResponseError(response);
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
          Log.e(errorMessage);
          if (i == numResponses - 1) {
            lastResponseError = new Exception(errorMessage);
          }
        }
      }

      if (lastResponseError == null) {
        lastResponseError = error;
      }

      if (lastResponseError != null && this.error != null) {
        this.error.error(lastResponseError);
      } else if (this.response != null) {
        this.response.response(responseJson);
      }
    } else if (error != null && this.error != null) {
      this.error.error(error);
    }
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

    final List<Map<String, Object>> requestsToSend = popUnsentRequests();
    if (requestsToSend.isEmpty()) {
      return;
    }

    final Map<String, Object> multiRequestArgs = new HashMap<>();
    multiRequestArgs.put(Constants.Params.DATA, jsonEncodeUnsentRequests(requestsToSend));
    multiRequestArgs.put(Constants.Params.SDK_VERSION, Constants.LEANPLUM_VERSION);
    multiRequestArgs.put(Constants.Params.ACTION, Constants.Methods.MULTI);
    multiRequestArgs.put(Constants.Params.TIME, Double.toString(new Date().getTime() / 1000.0));
    if (!this.attachApiKeys(multiRequestArgs)) {
      return;
    }

    Util.executeAsyncTask(new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... params) {
        JSONObject result = null;
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

            result = Util.getJsonResponse(op);
            int statusCode = op.getResponseCode();

            Exception errorException = null;
            if (statusCode >= 400) {
              errorException = new Exception("HTTP error " + statusCode);
              if (statusCode == 408 || (statusCode >= 500 && statusCode <= 599)) {
                pushUnsentRequests(requestsToSend);
              }
            } else {
              if (result != null) {
                int numResponses = Request.numResponses(result);
                if (numResponses != requestsToSend.size()) {
                  Log.w("Sent " + requestsToSend.size() +
                      " requests but only" + " received " + numResponses);
                }
              } else {
                errorException = new Exception("Response JSON is null.");
              }
            }
            parseResponseJson(result, requestsToSend, errorException);
          } catch (JSONException e) {
            Log.e("Error parsing JSON response: " + e.toString() + "\n" +
                Log.getStackTraceString(e));
            parseResponseJson(null, requestsToSend, e);
          } catch (Exception e) {
            pushUnsentRequests(requestsToSend);
            Log.e("Unable to send request: " + e.toString() + "\n" +
                Log.getStackTraceString(e));
            parseResponseJson(result, requestsToSend, e);
          } finally {
            if (op != null) {
              op.disconnect();
            }
          }
        } catch (Throwable t) {
          Util.handleException(t);
        }
        return null;
      }
    });
  }

  public void sendEventually() {
    if (Constants.isTestMode) {
      return;
    }
    if (!sent) {
      sent = true;
      Map<String, Object> args = createArgsDictionary();
      saveRequestForLater(args);
    }
  }

  static List<Map<String, Object>> popUnsentRequests() {
    return getUnsentRequests(true);
  }

  static List<Map<String, Object>> getUnsentRequests() {
    return getUnsentRequests(false);
  }

  private static List<Map<String, Object>> getUnsentRequests(boolean remove) {
    List<Map<String, Object>> requestData = new ArrayList<>();

    synchronized (lock) {
      lastSendTimeMs = System.currentTimeMillis();

      Context context = Leanplum.getContext();
      SharedPreferences preferences = context.getSharedPreferences(
          LEANPLUM, Context.MODE_PRIVATE);
      SharedPreferences.Editor editor = preferences.edit();

      int count = preferences.getInt(Constants.Defaults.COUNT_KEY, 0);
      if (count == 0) {
        return new ArrayList<>();
      }
      if (remove) {
        editor.remove(Constants.Defaults.COUNT_KEY);
      }

      for (int i = 0; i < count; i++) {
        String itemKey = String.format(Locale.US, Constants.Defaults.ITEM_KEY, i);
        Map<String, Object> requestArgs;
        try {
          requestArgs = JsonConverter.mapFromJson(new JSONObject(
              preferences.getString(itemKey, "{}")));
          requestData.add(requestArgs);
        } catch (JSONException e) {
          e.printStackTrace();
        }
        if (remove) {
          editor.remove(itemKey);
        }
      }
      if (remove) {
        try {
          editor.apply();
        } catch (NoSuchMethodError e) {
          editor.commit();
        }
      }
    }

    requestData = removeIrrelevantBackgroundStartRequests(requestData);
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

  private static void pushUnsentRequests(List<Map<String, Object>> requestData) {
    if (requestData == null) {
      return;
    }
    for (Map<String, Object> args : requestData) {
      Object retryCountString = args.get("retryCount");
      int retryCount;
      if (retryCountString != null) {
        retryCount = Integer.parseInt(retryCountString.toString()) + 1;
      } else {
        retryCount = 1;
      }
      args.put("retryCount", Integer.toString(retryCount));
      saveRequestForLater(args);
    }
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
    Util.executeAsyncTask(new AsyncTask<Void, Void, Void>() {
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

    Util.executeAsyncTask(new AsyncTask<Void, Void, Void>() {
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
