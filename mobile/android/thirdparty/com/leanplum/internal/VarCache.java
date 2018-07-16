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

import com.leanplum.ActionContext;
import com.leanplum.CacheUpdateBlock;
import com.leanplum.Leanplum;
import com.leanplum.LocationManager;
import com.leanplum.Var;
import com.leanplum.internal.FileManager.HashResults;
import com.leanplum.utils.SharedPreferencesUtil;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.InputStream;
import java.lang.reflect.Array;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Variable cache.
 *
 * @author Andrew First.
 */
public class VarCache {
  private static final Map<String, Var<?>> vars = new ConcurrentHashMap<>();
  private static final Map<String, Object> fileAttributes = new HashMap<>();
  private static final Map<String, InputStream> fileStreams = new HashMap<>();

  /**
   * The default values set by the client. This is not thread-safe so traversals should be
   * synchronized.
   */
  public static final Map<String, Object> valuesFromClient = new HashMap<>();

  private static final Map<String, String> defaultKinds = new HashMap<>();
  private static final Map<String, Object> actionDefinitions = new HashMap<>();
  private static final String LEANPLUM = "__leanplum__";
  private static Map<String, Object> diffs = new HashMap<>();
  private static Map<String, Object> regions = new HashMap<>();
  private static Map<String, Object> messageDiffs = new HashMap<>();
  private static List<Map<String, Object>> updateRuleDiffs;
  private static List<Map<String, Object>> eventRuleDiffs;
  private static Map<String, Object> devModeValuesFromServer;
  private static Map<String, Object> devModeFileAttributesFromServer;
  private static Map<String, Object> devModeActionDefinitionsFromServer;
  private static List<Map<String, Object>> variants = new ArrayList<>();
  private static CacheUpdateBlock updateBlock;
  private static CacheUpdateBlock interfaceUpdateBlock;
  private static CacheUpdateBlock eventsUpdateBlock;
  private static boolean hasReceivedDiffs = false;
  private static Map<String, Object> messages = new HashMap<>();
  private static Object merged;
  private static boolean silent;
  private static int contentVersion;
  private static Map<String, Object> userAttributes;

  private static final String NAME_COMPONENT_REGEX = "(?:[^\\.\\[.(\\\\]+|\\\\.)+";
  private static final Pattern NAME_COMPONENT_PATTERN = Pattern.compile(NAME_COMPONENT_REGEX);

  public static String[] getNameComponents(String name) {
    Matcher matcher = NAME_COMPONENT_PATTERN.matcher(name);
    List<String> components = new ArrayList<>();
    while (matcher.find()) {
      components.add(name.substring(matcher.start(), matcher.end()));
    }
    return components.toArray(new String[0]);
  }

  private static Object traverse(Object collection, Object key, boolean autoInsert) {
    if (collection == null) {
      return null;
    }
    if (collection instanceof Map) {
      Map<Object, Object> castedCollection = CollectionUtil.uncheckedCast(collection);
      Object result = castedCollection.get(key);
      if (autoInsert && result == null && key instanceof String) {
        result = new HashMap<String, Object>();
        castedCollection.put(key, result);
      }
      return result;
    } else if (collection instanceof List) {
      List<Object> castedList = CollectionUtil.uncheckedCast(collection);
      Object result = castedList.get((Integer) key);
      if (autoInsert && result == null) {
        result = new HashMap<String, Object>();
        castedList.set((Integer) key, result);
      }
      return result;
    }
    return null;
  }

  public static boolean registerFile(
      String stringValue, String defaultValue,
      InputStream defaultStream, boolean isResource, String resourceHash, int resourceSize) {
    if (Constants.isDevelopmentModeEnabled) {
      if (!Constants.isNoop()) {
        if (defaultStream == null) {
          return false;
        }
        Map<String, Object> variationAttributes = new HashMap<>();
        Map<String, Object> attributes = new HashMap<>();
        if (isResource) {
          attributes.put(Constants.Keys.HASH, resourceHash);
          attributes.put(Constants.Keys.SIZE, resourceSize);
        } else {
          if (Constants.hashFilesToDetermineModifications && Util.isSimulator()) {
            HashResults result = FileManager.fileMD5HashCreateWithPath(defaultStream);
            if (result != null) {
              attributes.put(Constants.Keys.HASH, result.hash);
              attributes.put(Constants.Keys.SIZE, result.size);
            }
          } else {
            int size = FileManager.getFileSize(
                FileManager.fileValue(stringValue, defaultValue, null));
            attributes.put(Constants.Keys.SIZE, size);
          }
        }
        variationAttributes.put("", attributes);
        fileAttributes.put(stringValue, variationAttributes);
        fileStreams.put(stringValue, defaultStream);
        maybeUploadNewFiles();
      }
      return true;
    }
    return false;
  }

  private static void updateValues(String name, String[] nameComponents, Object value, String kind,
      Map<String, Object> values, Map<String, String> kinds) {
    Object valuesPtr = values;
    if (nameComponents != null && nameComponents.length > 0) {
      for (int i = 0; i < nameComponents.length - 1; i++) {
        valuesPtr = traverse(valuesPtr, nameComponents[i], true);
      }
      if (valuesPtr instanceof Map) {
        Map<String, Object> map = CollectionUtil.uncheckedCast(valuesPtr);
        map.put(nameComponents[nameComponents.length - 1], value);
      }
    }
    if (kinds != null) {
      kinds.put(name, kind);
    }
  }

  public static void registerVariable(Var<?> var) {
    vars.put(var.name(), var);
    synchronized (valuesFromClient) {
      updateValues(
          var.name(), var.nameComponents(), var.defaultValue(),
          var.kind(), valuesFromClient, defaultKinds);
    }
  }

  @SuppressWarnings("unchecked")
  public static <T> Var<T> getVariable(String name) {
    return (Var<T>) vars.get(name);
  }

  private static void computeMergedDictionary() {
    synchronized (valuesFromClient) {
      merged = mergeHelper(valuesFromClient, diffs);
    }
  }

  public static Object mergeHelper(Object vars, Object diff) {
    if (diff == null) {
      return vars;
    }
    if (diff instanceof Number
        || diff instanceof Boolean
        || diff instanceof String
        || diff instanceof Character
        || vars instanceof Number
        || vars instanceof Boolean
        || vars instanceof String
        || vars instanceof Character) {
      return diff;
    }

    Iterable<?> diffKeys = (diff instanceof Map) ? ((Map<?, ?>) diff).keySet() : (Iterable<?>) diff;
    Iterable<?> varsKeys = (vars instanceof Map) ? ((Map<?, ?>) vars).keySet() : (Iterable<?>) vars;
    Map<?, ?> diffMap = (diff instanceof Map) ? ((Map<?, ?>) diff) : null;
    Map<?, ?> varsMap = (vars instanceof Map) ? ((Map<?, ?>) vars) : null;

    // Infer that the diffs is an array if the vars value doesn't exist to tell us the type.
    boolean isArray = false;
    if (vars == null) {
      if (diff instanceof Map && ((Map<?, ?>) diff).size() > 0) {
        isArray = true;
        for (Object var : diffKeys) {
          if (!(var instanceof String)) {
            isArray = false;
            break;
          }
          String str = ((String) var);
          if (str.length() < 3 || str.charAt(0) != '[' || str.charAt(str.length() - 1) != ']') {
            isArray = false;
            break;
          }
          String varSubscript = str.substring(1, str.length() - 1);
          if (!("" + Integer.getInteger(varSubscript)).equals(varSubscript)) {
            isArray = false;
            break;
          }
        }
      }
    }

    // Merge arrays.
    if (vars instanceof List || isArray) {
      ArrayList<Object> merged = new ArrayList<>();
      for (Object var : varsKeys) {
        merged.add(var);
      }
      for (Object varSubscript : diffKeys) {
        String strSubscript = (String) varSubscript;
        int subscript = Integer.parseInt(strSubscript.substring(1, strSubscript.length() - 1));
        Object var = diffMap != null ? diffMap.get(strSubscript) : null;
        while (subscript >= merged.size()) {
          merged.add(null);
        }
        merged.set(subscript, mergeHelper(merged.get(subscript), var));
      }
      return merged;
    }

    // Merge dictionaries.
    if (vars instanceof Map || diff instanceof Map) {
      HashMap<Object, Object> merged = new HashMap<>();
      if (varsKeys != null) {
        for (Object var : varsKeys) {
          if (diffMap != null && varsMap != null) {
            Object diffVar = diffMap.get(var);
            Object value = varsMap.get(var);
            if (diffVar == null && value != null) {
              merged.put(var, value);
            }
          }
        }
      }
      for (Object var : diffKeys) {
        Object diffsValue = diffMap != null ? diffMap.get(var) : null;
        Object varsValue = varsMap != null ? varsMap.get(var) : null;
        Object mergedValues = mergeHelper(varsValue, diffsValue);
        merged.put(var, mergedValues);
      }
      return merged;
    }
    return null;
  }

  @SuppressWarnings("unchecked")
  public static <T> T getMergedValueFromComponentArray(Object[] components, Object values) {
    Object mergedPtr = values;
    for (Object component : components) {
      mergedPtr = traverse(mergedPtr, component, false);
    }
    return (T) mergedPtr;
  }

  public static <T> T getMergedValueFromComponentArray(Object[] components) {
    return getMergedValueFromComponentArray(components,
        merged != null ? merged : valuesFromClient);
  }

  public static Map<String, Object> getDiffs() {
    return diffs;
  }

  public static Map<String, Object> getMessageDiffs() {
    return messageDiffs;
  }

  public static List<Map<String, Object>> getUpdateRuleDiffs() {
    return updateRuleDiffs;
  }

  public static List<Map<String, Object>> getEventRuleDiffs() {
    return eventRuleDiffs;
  }

  public static Map<String, Object> regions() {
    return regions;
  }

  public static boolean hasReceivedDiffs() {
    return hasReceivedDiffs;
  }

  public static void loadDiffs() {
    if (Constants.isNoop()) {
      return;
    }
    Context context = Leanplum.getContext();
    SharedPreferences defaults = context.getSharedPreferences(LEANPLUM, Context.MODE_PRIVATE);
    if (Request.token() == null) {
      applyVariableDiffs(
          new HashMap<String, Object>(),
          new HashMap<String, Object>(),
          new ArrayList<Map<String, Object>>(),
          new ArrayList<Map<String, Object>>(),
          new HashMap<String, Object>(),
          new ArrayList<Map<String, Object>>());
      return;
    }
    try {
      // Crypt functions return input text if there was a problem.
      AESCrypt aesContext = new AESCrypt(Request.appId(), Request.token());
      String variables = aesContext.decodePreference(
          defaults, Constants.Defaults.VARIABLES_KEY, "{}");
      String messages = aesContext.decodePreference(
          defaults, Constants.Defaults.MESSAGES_KEY, "{}");
      String updateRules = aesContext.decodePreference(
          defaults, Constants.Defaults.UPDATE_RULES_KEY, "[]");
      String eventRules = aesContext.decodePreference(
          defaults, Constants.Defaults.EVENT_RULES_KEY, "[]");
      String regions = aesContext.decodePreference(defaults, Constants.Defaults.REGIONS_KEY, "{}");
      String variants = aesContext.decodePreference(defaults, Constants.Keys.VARIANTS, "[]");
      applyVariableDiffs(
          JsonConverter.fromJson(variables),
          JsonConverter.fromJson(messages),
          JsonConverter.<Map<String, Object>>listFromJson(new JSONArray(updateRules)),
          JsonConverter.<Map<String, Object>>listFromJson(new JSONArray(eventRules)),
          JsonConverter.fromJson(regions),
          JsonConverter.<Map<String, Object>>listFromJson(new JSONArray(variants)));
      String deviceId = aesContext.decodePreference(defaults, Constants.Params.DEVICE_ID, null);
      if (deviceId != null) {
        if (Util.isValidDeviceId(deviceId)) {
          Request.setDeviceId(deviceId);
        } else {
          Log.w("Invalid stored device id found: \"" + deviceId + "\"; discarding.");
        }
      }
      String userId = aesContext.decodePreference(defaults, Constants.Params.USER_ID, null);
      if (userId != null) {
        if (Util.isValidUserId(userId)) {
          Request.setUserId(userId);
        } else {
          Log.w("Invalid stored user id found: \"" + userId + "\"; discarding.");
        }
      }
      String loggingEnabled = aesContext.decodePreference(defaults, Constants.Keys.LOGGING_ENABLED,
          "false");
      if (Boolean.parseBoolean(loggingEnabled)) {
        Constants.loggingEnabled = true;
      }
    } catch (Exception e) {
      Log.e("Could not load variable diffs.\n" + Log.getStackTraceString(e));
    }
    userAttributes();
  }

  public static void saveDiffs() {
    if (Constants.isNoop()) {
      return;
    }
    if (Request.token() == null) {
      return;
    }
    Context context = Leanplum.getContext();
    SharedPreferences defaults = context.getSharedPreferences(LEANPLUM, Context.MODE_PRIVATE);
    SharedPreferences.Editor editor = defaults.edit();

    // Crypt functions return input text if there was a problem.
    AESCrypt aesContext = new AESCrypt(Request.appId(), Request.token());
    String variablesCipher = aesContext.encrypt(JsonConverter.toJson(diffs));
    editor.putString(Constants.Defaults.VARIABLES_KEY, variablesCipher);

    String messagesCipher = aesContext.encrypt(JsonConverter.toJson(messages));
    editor.putString(Constants.Defaults.MESSAGES_KEY, messagesCipher);

    try {
      String updateRulesCipher = aesContext.encrypt(
          JsonConverter.listToJsonArray(updateRuleDiffs).toString());
      editor.putString(Constants.Defaults.UPDATE_RULES_KEY, updateRulesCipher);
    } catch (JSONException e) {
      Log.e("Error converting updateRuleDiffs to JSON", e);
    }

    try {
      String eventRulesCipher = aesContext.encrypt(
          JsonConverter.listToJsonArray(eventRuleDiffs).toString());
      editor.putString(Constants.Defaults.EVENT_RULES_KEY, eventRulesCipher);
    } catch (JSONException e) {
      Log.e("Error converting eventRuleDiffs to JSON", e);
    }

    String regionsCipher = aesContext.encrypt(JsonConverter.toJson(regions));
    editor.putString(Constants.Defaults.REGIONS_KEY, regionsCipher);

    try {
      String variantsJson = JsonConverter.listToJsonArray(variants).toString();
      editor.putString(Constants.Keys.VARIANTS, aesContext.encrypt(variantsJson));
    } catch (JSONException e1) {
      Log.e("Error converting " + variants + " to JSON.\n" + Log.getStackTraceString(e1));
    }
    editor.putString(Constants.Params.DEVICE_ID, aesContext.encrypt(Request.deviceId()));
    editor.putString(Constants.Params.USER_ID, aesContext.encrypt(Request.userId()));
    editor.putString(Constants.Keys.LOGGING_ENABLED,
        aesContext.encrypt(String.valueOf(Constants.loggingEnabled)));
    SharedPreferencesUtil.commitChanges(editor);
  }

  /**
   * Convert a resId to a resPath.
   */
  static int getResIdFromPath(String resPath) {
    int resId = 0;
    try {
      String path = resPath.replace("res/", "");
      path = path.substring(0, path.lastIndexOf('.'));  // remove file extension
      String name = path.substring(path.lastIndexOf('/') + 1);
      String type = path.substring(0, path.lastIndexOf('/'));
      type = type.replace('/', '.');
      Context context = Leanplum.getContext();
      resId = context.getResources().getIdentifier(name, type, context.getPackageName());
    } catch (Exception e) {
      // Fall back to 0 on any exception
    }
    return resId;
  }

  /**
   * Update file variables stream info with override info, so that override files don't require
   * downloads if they're already available.
   */
  private static void fileVariableFinish() {
    for (String name : new HashMap<>(vars).keySet()) {
      Var<?> var = vars.get(name);
      if (var == null) {
        continue;
      }
      String overrideFile = var.stringValue;
      if (var.isResource && Constants.Kinds.FILE.equals(var.kind()) && overrideFile != null &&
              !overrideFile.equals(var.defaultValue())) {
        Map<String, Object> variationAttributes = CollectionUtil.uncheckedCast(fileAttributes.get
                (overrideFile));
        InputStream stream = fileStreams.get(overrideFile);
        if (variationAttributes != null && stream != null) {
          var.setOverrideResId(getResIdFromPath(var.stringValue()));
        }
      }
    }
  }

  public static void applyVariableDiffs(
      Map<String, Object> diffs,
      Map<String, Object> messages,
      List<Map<String, Object>> updateRules,
      List<Map<String, Object>> eventRules,
      Map<String, Object> regions,
      List<Map<String, Object>> variants) {
    if (diffs != null) {
      VarCache.diffs = diffs;
      computeMergedDictionary();

      // Update variables with new values.
      // Have to copy the dictionary because a dictionary variable may add a new sub-variable,
      // modifying the variable dictionary.
      for (String name : new HashMap<>(vars).keySet()) {
        vars.get(name).update();
      }
      fileVariableFinish();
    }

    if (messages != null) {
      // Store messages.
      messageDiffs = messages;
      Map<String, Object> newMessages = new HashMap<>();
      for (Map.Entry<String, Object> entry : messages.entrySet()) {
        Map<String, Object> messageConfig = CollectionUtil.uncheckedCast(entry.getValue());
        Map<String, Object> newConfig = new HashMap<>(messageConfig);
        Map<String, Object> actionArgs = CollectionUtil.uncheckedCast(messageConfig.get(Constants
            .Keys.VARS));
        Map<String, Object> defaultArgs = Util.multiIndex(actionDefinitions,
            newConfig.get(Constants.Params.ACTION), "values");
        Map<String, Object> vars = CollectionUtil.uncheckedCast(mergeHelper(defaultArgs,
            actionArgs));
        newMessages.put(entry.getKey(), newConfig);
        newConfig.put(Constants.Keys.VARS, vars);
      }

      VarCache.messages = newMessages;
      for (Map.Entry<String, Object> entry : VarCache.messages.entrySet()) {
        String name = entry.getKey();
        Map<String, Object> messageConfig = CollectionUtil.uncheckedCast(VarCache.messages.get
            (name));
        if (messageConfig != null && messageConfig.get("action") != null) {
          Map<String, Object> actionArgs =
              CollectionUtil.uncheckedCast(messageConfig.get(Constants.Keys.VARS));
          new ActionContext(
              messageConfig.get("action").toString(), actionArgs, name).update();
        }
      }
    }

    if (regions != null) {
      VarCache.regions = regions;
    }

    if (messages != null || regions != null) {
      Set<String> foregroundRegionNames = new HashSet<>();
      Set<String> backgroundRegionNames = new HashSet<>();
      ActionManager.getForegroundandBackgroundRegionNames(foregroundRegionNames,
          backgroundRegionNames);
      LocationManager locationManager = ActionManager.getLocationManager();
      if (locationManager != null) {
        locationManager.setRegionsData(regions, foregroundRegionNames, backgroundRegionNames);
      }
    }

    boolean interfaceUpdated = false;
    if (updateRules != null) {
      interfaceUpdated = !(updateRules.equals(updateRuleDiffs));
      updateRuleDiffs = new ArrayList<>(updateRules);
      VarCache.downloadUpdateRulesImages();
    }

    boolean eventsUpdated = false;
    if (eventRules != null) {
      eventsUpdated = !(eventRules.equals(eventRuleDiffs));
      eventRuleDiffs = new ArrayList<>(eventRules);
    }

    if (variants != null) {
      VarCache.variants = variants;
    }

    contentVersion++;

    if (!silent) {
      saveDiffs();
      triggerHasReceivedDiffs();

      if (interfaceUpdated && interfaceUpdateBlock != null) {
        interfaceUpdateBlock.updateCache();
      }

      if (eventsUpdated && eventsUpdateBlock != null) {
        eventsUpdateBlock.updateCache();
      }
    }
  }

  static void applyUpdateRuleDiffs(List<Map<String, Object>> updateRuleDiffs) {
    VarCache.updateRuleDiffs = updateRuleDiffs;
    VarCache.downloadUpdateRulesImages();
    if (interfaceUpdateBlock != null) {
      interfaceUpdateBlock.updateCache();
    }
    VarCache.saveDiffs();
  }

  private static void downloadUpdateRulesImages() {
    for (Map value : VarCache.updateRuleDiffs) {
      List changes = (List) value.get("changes");
      for (Object change : changes) {
        Map<String, String> castedChange = CollectionUtil.uncheckedCast(change);
        String key = castedChange.get("key");
        if (key != null && key.contains("image")) {
          String name = castedChange.get("value");
          FileManager.maybeDownloadFile(true, name, null, null, null);
        }
      }
    }
  }

  public static int contentVersion() {
    return contentVersion;
  }

  @SuppressWarnings("SameParameterValue")
  private static boolean areActionDefinitionsEqual(
      Map<String, Object> a, Map<String, Object> b) {
    if ((a == null || b == null) || (a.size() != b.size())) {
      return false;
    }
    for (Map.Entry<String, Object> entry : a.entrySet()) {
      Map<String, Object> aItem = CollectionUtil.uncheckedCast(entry.getValue());
      Map<String, Object> bItem = CollectionUtil.uncheckedCast(b.get(entry.getKey()));
      if (bItem == null || aItem == null) {
        return false;
      }

      Object aKind = aItem.get("kind");
      Object aValues = aItem.get("values");
      Object aKinds = aItem.get("kinds");
      Object aOptions = aItem.get("options");
      if (aKind != null && !aKind.equals(bItem.get("kind")) ||
          aValues != null && !aValues.equals(bItem.get("values")) ||
          aKinds != null && !aKinds.equals(bItem.get("kinds")) ||
          (aOptions == null) != (bItem.get("options") == null) ||
          aOptions != null && aOptions.equals(bItem.get("options"))) {
        return false;
      }
    }
    return true;
  }

  private static void triggerHasReceivedDiffs() {
    hasReceivedDiffs = true;
    if (updateBlock != null) {
      updateBlock.updateCache();
    }
  }

  static boolean sendVariablesIfChanged() {
    return sendContentIfChanged(true, false);
  }

  static boolean sendActionsIfChanged() {
    return sendContentIfChanged(false, true);
  }

  private static boolean sendContentIfChanged(boolean variables, boolean actions) {
    boolean changed = false;
    if (variables && devModeValuesFromServer != null
        && !valuesFromClient.equals(devModeValuesFromServer)) {
      changed = true;
    }
    if (actions && !areActionDefinitionsEqual(
        actionDefinitions, devModeActionDefinitionsFromServer)) {
      changed = true;
    }

    if (changed) {
      HashMap<String, Object> params = new HashMap<>();
      if (variables) {
        params.put(Constants.Params.VARS, JsonConverter.toJson(valuesFromClient));
        params.put(Constants.Params.KINDS, JsonConverter.toJson(defaultKinds));
      }
      if (actions) {
        params.put(Constants.Params.ACTION_DEFINITIONS, JsonConverter.toJson(actionDefinitions));
      }
      params.put(Constants.Params.FILE_ATTRIBUTES, JsonConverter.toJson(fileAttributes));
      Request.post(Constants.Methods.SET_VARS, params).sendIfConnected();
    }

    return changed;
  }

  static void maybeUploadNewFiles() {
    // First check to make sure we have all the data we need
    if (Constants.isNoop()
        || devModeFileAttributesFromServer == null
        || !Leanplum.hasStartedAndRegisteredAsDeveloper()
        || !Constants.enableFileUploadingInDevelopmentMode) {
      return;
    }

    List<String> filenames = new ArrayList<>();
    List<JSONObject> fileData = new ArrayList<>();
    List<InputStream> streams = new ArrayList<>();
    int totalSize = 0;
    for (Map.Entry<String, Object> entry : fileAttributes.entrySet()) {
      String name = entry.getKey();
      Map<String, Object> variationAttributes = CollectionUtil.uncheckedCast(entry.getValue());
      Map<String, Object> serverVariationAttributes =
          CollectionUtil.uncheckedCast(devModeFileAttributesFromServer.get(name));
      Map<String, Object> localAttributes = CollectionUtil.uncheckedCast(variationAttributes.get
          (""));
      Map<String, Object> serverAttributes = CollectionUtil.uncheckedCast(
          (serverVariationAttributes != null ? serverVariationAttributes.get("") : null));
      if (FileManager.isNewerLocally(localAttributes, serverAttributes)) {
        Log.v("Will upload file " + name + ". Local attributes: " +
            localAttributes + "; server attributes: " + serverAttributes);

        String hash = (String) localAttributes.get(Constants.Keys.HASH);
        if (hash == null) {
          hash = "";
        }

        String variationPath = FileManager.fileRelativeToAppBundle(name);

        // Upload in batch if we can't put any more files in
        if ((totalSize > Constants.Files.MAX_UPLOAD_BATCH_SIZES && filenames.size() > 0)
            || filenames.size() >= Constants.Files.MAX_UPLOAD_BATCH_FILES) {
          Map<String, Object> params = new HashMap<>();
          params.put(Constants.Params.DATA, fileData.toString());

          Request.post(Constants.Methods.UPLOAD_FILE, params).sendFilesNow(filenames,
              streams);

          filenames = new ArrayList<>();
          fileData = new ArrayList<>();
          streams = new ArrayList<>();
          totalSize = 0;
        }

        // Add the current file to the lists and update size
        Object size = localAttributes.get(Constants.Keys.SIZE);
        totalSize += (Integer) size;
        filenames.add(variationPath);
        JSONObject fileDatum = new JSONObject();
        try {
          fileDatum.put(Constants.Keys.HASH, hash);
          fileDatum.put(Constants.Keys.SIZE, localAttributes.get(Constants.Keys.SIZE) + "");
          fileDatum.put(Constants.Keys.FILENAME, name);
          fileData.add(fileDatum);
        } catch (JSONException e) {
          // HASH, SIZE, or FILENAME are null, which they never should be (they're constants).
          Log.e("Unable to upload files.\n" + Log.getStackTraceString(e));
          fileData.add(new JSONObject());
        }
        streams.add(fileStreams.get(name));
      }
    }

    if (filenames.size() > 0) {
      Map<String, Object> params = new HashMap<>();
      params.put(Constants.Params.DATA, fileData.toString());
      Request.post(Constants.Methods.UPLOAD_FILE, params).sendFilesNow(filenames, streams);
    }
  }

  /**
   * Sets whether values should be saved and callbacks triggered when the variable values get
   * updated.
   */
  public static void setSilent(boolean silent) {
    VarCache.silent = silent;
  }

  public static boolean silent() {
    return silent;
  }

  public static void setDevModeValuesFromServer(Map<String, Object> values,
      Map<String, Object> fileAttributes, Map<String, Object> actionDefinitions) {
    devModeValuesFromServer = values;
    devModeActionDefinitionsFromServer = actionDefinitions;
    devModeFileAttributesFromServer = fileAttributes;
  }

  public static void onUpdate(CacheUpdateBlock block) {
    updateBlock = block;
  }

  public static void onInterfaceUpdate(CacheUpdateBlock block) {
    interfaceUpdateBlock = block;
  }

  public static void onEventsUpdate(CacheUpdateBlock block) {
    eventsUpdateBlock = block;
  }

  public static List<Map<String, Object>> variants() {
    return variants;
  }

  public static Map<String, Object> actionDefinitions() {
    return actionDefinitions;
  }

  public static Map<String, Object> messages() {
    return messages;
  }

  public static void registerActionDefinition(
      String name, int kind, List<ActionArg<?>> args,
      Map<String, Object> options) {
    Map<String, Object> values = new HashMap<>();
    Map<String, String> kinds = new HashMap<>();
    List<String> order = new ArrayList<>();
    for (ActionArg<?> arg : args) {
      updateValues(arg.name(), getNameComponents(arg.name()),
          arg.defaultValue(), arg.kind(), values, kinds);
      order.add(arg.name());
    }
    Map<String, Object> definition = new HashMap<>();
    definition.put("kind", kind);
    definition.put("values", values);
    definition.put("kinds", kinds);
    definition.put("order", order);
    definition.put("options", options);
    actionDefinitions.put(name, definition);
  }

  public static <T> String kindFromValue(T defaultValue) {
    String kind = null;
    if (defaultValue instanceof Integer
        || defaultValue instanceof Long
        || defaultValue instanceof Short
        || defaultValue instanceof Character
        || defaultValue instanceof Byte
        || defaultValue instanceof BigInteger) {
      kind = Constants.Kinds.INT;
    } else if (defaultValue instanceof Float
        || defaultValue instanceof Double
        || defaultValue instanceof BigDecimal) {
      kind = Constants.Kinds.FLOAT;
    } else if (defaultValue instanceof String) {
      kind = Constants.Kinds.STRING;
    } else if (defaultValue instanceof List
        || defaultValue instanceof Array) {
      kind = Constants.Kinds.ARRAY;
    } else if (defaultValue instanceof Map) {
      kind = Constants.Kinds.DICTIONARY;
    } else if (defaultValue instanceof Boolean) {
      kind = Constants.Kinds.BOOLEAN;
    }
    return kind;
  }

  static Map<String, Object> userAttributes() {
    if (userAttributes == null) {
      Context context = Leanplum.getContext();
      SharedPreferences defaults = context.getSharedPreferences(LEANPLUM, Context.MODE_PRIVATE);
      AESCrypt aesContext = new AESCrypt(Request.appId(), Request.token());
      try {
        userAttributes = JsonConverter.fromJson(
            aesContext.decodePreference(defaults, Constants.Defaults.ATTRIBUTES_KEY, "{}"));
      } catch (Exception e) {
        Log.e("Could not load user attributes.\n" + Log.getStackTraceString(e));
        userAttributes = new HashMap<>();
      }
    }
    return userAttributes;
  }

  public static void saveUserAttributes() {
    if (Constants.isNoop() || Request.appId() == null || userAttributes == null) {
      return;
    }
    Context context = Leanplum.getContext();
    SharedPreferences defaults = context.getSharedPreferences(LEANPLUM, Context.MODE_PRIVATE);
    SharedPreferences.Editor editor = defaults.edit();
    // Crypt functions return input text if there was a problem.
    String plaintext = JsonConverter.toJson(userAttributes);
    AESCrypt aesContext = new AESCrypt(Request.appId(), Request.token());
    editor.putString(Constants.Defaults.ATTRIBUTES_KEY, aesContext.encrypt(plaintext));
    SharedPreferencesUtil.commitChanges(editor);
  }

  /**
   * Resets the VarCache to stock state.
   */
  public static void reset() {
    vars.clear();
    fileAttributes.clear();
    fileStreams.clear();
    valuesFromClient.clear();
    defaultKinds.clear();
    actionDefinitions.clear();
    diffs.clear();
    messageDiffs.clear();
    regions.clear();
    devModeValuesFromServer = null;
    devModeFileAttributesFromServer = null;
    devModeActionDefinitionsFromServer = null;
    variants.clear();
    updateBlock = null;
    hasReceivedDiffs = false;
    messages = null;
    merged = null;
    silent = false;
    contentVersion = 0;
    userAttributes = null;
  }
}
