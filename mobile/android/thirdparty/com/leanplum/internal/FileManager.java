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

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.net.Uri;
import android.os.AsyncTask;
import android.text.TextUtils;

import com.leanplum.Leanplum;
import com.leanplum.Var;

import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Leanplum file manager.
 *
 * @author Andrew First
 */
public class FileManager {
  interface ResourceUpdateCallback {
    void onResourceSyncFinish();
  }

  private static ResourceUpdateCallback updateCallback;
  private static boolean isInitialized = false;
  private static boolean initializing = false;
  static final Object initializingLock = new Object();
  public static Var<HashMap<String, Object>> resources = null;

  public enum DownloadFileResult {
    NONE,
    EXISTS_IN_ASSETS,
    DOWNLOADING
  }

  static class HashResults {
    final String hash;
    final int size;

    public HashResults(String hash, int size) {
      this.hash = hash;
      this.size = size;
    }
  }

  public static DownloadFileResult maybeDownloadFile(boolean isResource, String stringValue,
      String defaultValue, String urlValue, final Runnable onComplete) {
    if (stringValue != null && !stringValue.equals(defaultValue) &&
        (!isResource || VarCache.getResIdFromPath(stringValue) == 0)) {
      InputStream inputStream = null;
      try {
        Context context = Leanplum.getContext();
        inputStream = context.getResources().getAssets().open(stringValue);
        if (inputStream != null) {
          return DownloadFileResult.EXISTS_IN_ASSETS;
        }
      } catch (IOException ignored) {
      } finally {
        if (inputStream != null) {
          try {
            inputStream.close();
          } catch (IOException e) {
            Log.w("Failed to close InputStream.", e.getMessage());
          }
        }
      }
      String realPath = FileManager.fileRelativeToAppBundle(stringValue);
      if (!FileManager.fileExistsAtPath(realPath)) {
        realPath = FileManager.fileRelativeToDocuments(stringValue);
        if (!FileManager.fileExistsAtPath(realPath)) {
          Request downloadRequest = Request.get(Constants.Methods.DOWNLOAD_FILE, null);
          downloadRequest.onResponse(new Request.ResponseCallback() {
            @Override
            public void response(JSONObject response) {
              if (onComplete != null) {
                onComplete.run();
              }
            }
          });
          downloadRequest.onError(new Request.ErrorCallback() {
            @Override
            public void error(Exception e) {
              if (onComplete != null) {
                onComplete.run();
              }
            }
          });
          downloadRequest.downloadFile(stringValue, urlValue);
          return DownloadFileResult.DOWNLOADING;
        }
      }
    }
    return DownloadFileResult.NONE;
  }

  static HashResults fileMD5HashCreateWithPath(InputStream is) {
    try {
      MessageDigest md = MessageDigest.getInstance("MD5");
      int size = 0;
      try {
        is = new DigestInputStream(is, md);
        byte[] buffer = new byte[8192];
        int bytesRead;
        while ((bytesRead = is.read(buffer)) != -1) {
          size += bytesRead;
        }
      } finally {
        if (is != null) {
          try {
            is.close();
          } catch (IOException e) {
            Log.w("Failed to close InputStream.", e.getMessage());
          }
        }
      }
      byte[] digest = md.digest();

      StringBuilder hexString = new StringBuilder();
      for (byte dig : digest) {
        String hex = Integer.toHexString(0xFF & dig);
        if (hex.length() == 1) {
          hexString.append('0');
        }
        hexString.append(hex);
      }
      return new HashResults(hexString.toString(), size);
    } catch (NoSuchAlgorithmException e) {
      e.printStackTrace();
      return null;
    } catch (IOException e) {
      e.printStackTrace();
      return null;
    }
  }

  static int getFileSize(String path) {
    if (path == null) {
      return -1;
    }
    return (int) new File(path).length();
  }

  public static boolean fileExistsAtPath(String path) {
    return path != null && new File(path).exists();
  }

  @SuppressWarnings("SameReturnValue")
  private static String appBundlePath() {
    return "";
  }

  private static String documentsPath() {
    Context context = Leanplum.getContext();
    if (context != null) {
      return context.getDir("Leanplum_Documents", Context.MODE_PRIVATE).getAbsolutePath();
    }
    return null;
  }

  private static String bundlePath() {
    Context context = Leanplum.getContext();
    return context.getDir("Leanplum_Bundle", Context.MODE_PRIVATE).getAbsolutePath();
  }

  private static String fileRelativeTo(String root, String path) {
    return root + "/" + path;
  }

  static String fileRelativeToAppBundle(String path) {
    if (path == null) {
      return null;
    }
    return fileRelativeTo(appBundlePath(), path);
  }

  static String fileRelativeToLPBundle(String path) {
    if (path == null) {
      return null;
    }
    return fileRelativeTo(bundlePath(), path);
  }

  public static String fileRelativeToDocuments(String path) {
    if (path == null) {
      return null;
    }
    return fileRelativeTo(documentsPath(), path);
  }

  static boolean isNewerLocally(
      Map<String, Object> localAttributes,
      Map<String, Object> serverAttributes) {
    if (serverAttributes == null) {
      return true;
    }
    String localHash = (String) localAttributes.get(Constants.Keys.HASH);
    String serverHash = (String) serverAttributes.get(Constants.Keys.HASH);
    Integer localSize = (Integer) localAttributes.get(Constants.Keys.SIZE);
    Integer serverSize = (Integer) serverAttributes.get(Constants.Keys.SIZE);
    return (serverSize == null || (localSize != null && !localSize.equals(serverSize))) ||
        (localHash != null && (serverHash == null || !localHash.equals(serverHash)));
  }

  static void setResourceSyncFinishBlock(ResourceUpdateCallback callback) {
    updateCallback = callback;
  }

  static boolean initializing() {
    return initializing;
  }

  public static boolean isResourceSyncingEnabled() {
    return initializing || isInitialized;
  }

  private static void enableResourceSyncing(List<Pattern> patternsToInclude,
      List<Pattern> patternsToExclude) {
    resources = Var.define(Constants.Values.RESOURCES_VARIABLE, new HashMap<String, Object>());

    // This is from http://stackoverflow.com/questions/3052964/is-there-any-way-to-tell-what-folder-a-drawable-resource-was-inflated-from.
    String drawableDirPrefix = "res/drawable";
    String layoutDirPrefix = "res/layout";
    ZipInputStream apk = null;
    Context context = Leanplum.getContext();
    try {
      apk = new ZipInputStream(new FileInputStream(context.getPackageResourcePath()));
      ZipEntry entry;
      while ((entry = apk.getNextEntry()) != null) {
        String resourcePath = entry.getName();
        if (resourcePath.startsWith(drawableDirPrefix) ||
            resourcePath.startsWith(layoutDirPrefix)) {
          String unprefixedResourcePath = resourcePath.substring(4);

          if (patternsToInclude != null &&
              patternsToInclude.size() > 0) {
            boolean included = false;
            for (Pattern pattern : patternsToInclude) {
              if (pattern.matcher(unprefixedResourcePath).matches()) {
                included = true;
                break;
              }
            }
            if (!included) {
              continue;
            }
          }
          if (patternsToExclude != null) {
            boolean excluded = false;
            for (Pattern pattern : patternsToExclude) {
              if (pattern.matcher(unprefixedResourcePath).matches()) {
                excluded = true;
                break;
              }
            }
            if (excluded) {
              continue;
            }
          }

          ByteArrayOutputStream outputStream = new ByteArrayOutputStream();

          int bytesRead;
          int size = 0;
          byte[] buffer = new byte[8192];
          while ((bytesRead = apk.read(buffer)) != -1) {
            outputStream.write(buffer, 0, bytesRead);
            size += bytesRead;
          }
          apk.closeEntry();

          String hash = ("" + entry.getTime()) + ("" + size);

          Var.defineResource(
              Constants.Values.RESOURCES_VARIABLE
                  + "." + unprefixedResourcePath.replace(".", "\\.").replace('/', '.'),
              resourcePath, size, hash, outputStream.toByteArray());
        }
      }
    } catch (IOException e) {
      Log.w("Error occurred when trying " +
          "to enable resource syncing." + e.getMessage());
    } finally {
      if (apk != null) {
        try {
          apk.close();
        } catch (IOException e) {
          Log.w("Failed to close ZipInputStream.", e.getMessage());
        }
      }
    }
    isInitialized = true;
    synchronized (initializingLock) {
      initializing = false;
      if (updateCallback != null) {
        updateCallback.onResourceSyncFinish();
      }
    }
  }

  private static List<Pattern> compilePatterns(List<String> patterns) {
    if (patterns == null) {
      return new ArrayList<>(0);
    }
    List<Pattern> compiledPatterns = new ArrayList<>(patterns.size());
    for (String pattern : patterns) {
      try {
        compiledPatterns.add(Pattern.compile(pattern));
      } catch (PatternSyntaxException e) {
        Log.e("Ignoring malformed resource syncing pattern: \"" + pattern +
            "\". Patterns must be regular expressions.");
      }
    }
    return compiledPatterns;
  }

  public static void enableResourceSyncing(final List<String> patternsToInclude,
      final List<String> patternsToExclude, boolean isAsync) {
    initializing = true;
    if (isInitialized) {
      return;
    }

    try {
      final List<Pattern> compiledIncludePatterns = compilePatterns(patternsToInclude);
      final List<Pattern> compiledExcludePatterns = compilePatterns(patternsToExclude);

      if (isAsync) {
        Util.executeAsyncTask(false, new AsyncTask<Void, Void, Void>() {
          @Override
          protected Void doInBackground(Void... params) {
            try {
              enableResourceSyncing(compiledIncludePatterns, compiledExcludePatterns);
            } catch (Throwable t) {
              Util.handleException(t);
            }
            return null;
          }
        });
      } else {
        enableResourceSyncing(compiledIncludePatterns, compiledExcludePatterns);
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  /**
   * Returns an String with path for a specified file/resource/asset variable.
   *
   * @param stringValue Name of the file.
   * @return String Path for a given variable.
   */
  public static String fileValue(String stringValue) {
    return fileValue(stringValue, stringValue, null);
  }

  public static String fileValue(String stringValue, String defaultValue,
      Boolean valueIsInAssets) {
    String result;
    if (stringValue == null) {
      return null;
    }
    if (stringValue.equals(defaultValue)) {
      result = FileManager.fileRelativeToAppBundle(defaultValue);
      if (FileManager.fileExistsAtPath(result)) {
        return result;
      }
    }

    if (valueIsInAssets == null) {
      InputStream inputStream = null;
      try {
        Context context = Leanplum.getContext();
        inputStream = context.getAssets().open(stringValue);
        return stringValue;
      } catch (Exception ignored) {
      } finally {
        if (inputStream != null) {
          try {
            inputStream.close();
          } catch (Exception e) {
            Log.w("Failed to close InputStream: " + e);
          }
        }
      }
    } else if (valueIsInAssets) {
      return stringValue;
    }

    result = FileManager.fileRelativeToLPBundle(stringValue);
    if (!FileManager.fileExistsAtPath(result)) {
      result = FileManager.fileRelativeToDocuments(stringValue);
      if (!FileManager.fileExistsAtPath(result)) {
        result = FileManager.fileRelativeToAppBundle(stringValue);
        if (!FileManager.fileExistsAtPath(result)) {
          result = FileManager.fileRelativeToLPBundle(defaultValue);
          if (!FileManager.fileExistsAtPath(result)) {
            result = FileManager.fileRelativeToAppBundle(defaultValue);
            if (!FileManager.fileExistsAtPath(result)) {
              return defaultValue;
            }
          }
        }
      }
    }
    return result;
  }

  /**
   * Returns an InputStream for a specified file/resource/asset variable. It will automatically find
   * a proper file based on provided data. File can be located in either assets, res or in-memory.
   * Caller is responsible for closing the returned InputStream.
   *
   * @param isResource Whether variable is resource.
   * @param isAsset Whether variable is asset.
   * @param valueIsInAssets Whether variable is located in assets directory.
   * @param value Name of the file.
   * @param defaultValue Default name of the file.
   * @param resourceData Data if file is not located on internal storage.
   * @return InputStream for a given variable.
   */
  public static InputStream stream(boolean isResource, Boolean isAsset, Boolean valueIsInAssets,
      String value, String defaultValue, byte[] resourceData) {
    if (TextUtils.isEmpty(value)) {
      return null;
    }
    try {
      Context context = Leanplum.getContext();
      if (isResource && value.equals(defaultValue) && resourceData != null) {
        return new ByteArrayInputStream(resourceData);
      } else if (isResource && value.equals(defaultValue) && resourceData == null) {
        // Convert resource name into resource id.
        int resourceId = Util.generateIdFromResourceName(value);
        // Android only generates id's greater then 0.
        if (resourceId != 0) {
          Resources res = context.getResources();
          // Based on resource Id, we can extract package it belongs, directory where it is stored
          // and name of the file.
          Uri resourceUri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
              "://" + res.getResourcePackageName(resourceId)
              + '/' + res.getResourceTypeName(resourceId)
              + '/' + res.getResourceEntryName(resourceId));
          return context.getContentResolver().openInputStream(resourceUri);
        }
        return null;
      }
      if (valueIsInAssets == null) {
        try {
          return context.getAssets().open(value);
        } catch (IOException ignored) {
        }
      } else if (valueIsInAssets || (isAsset && value.equals(defaultValue))) {
        return context.getAssets().open(value);
      }
      return new FileInputStream(new File(value));
    } catch (IOException e) {
      Log.w("Failed to load a stream." + e.getMessage());
      return null;
    }
  }
}
