/*
 * Copyright 2014, Leanplum, Inc. All rights reserved.
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

package com.leanplum;

import android.support.annotation.NonNull;
import android.text.TextUtils;

import com.leanplum.callbacks.VariablesChangedCallback;
import com.leanplum.internal.ActionManager;
import com.leanplum.internal.BaseActionContext;
import com.leanplum.internal.CollectionUtil;
import com.leanplum.internal.Constants;
import com.leanplum.internal.FileManager;
import com.leanplum.internal.JsonConverter;
import com.leanplum.internal.LeanplumInternal;
import com.leanplum.internal.Log;
import com.leanplum.internal.Util;
import com.leanplum.internal.VarCache;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The context in which an action or message is executed.
 *
 * @author Andrew First
 */
public class ActionContext extends BaseActionContext implements Comparable<ActionContext> {
  private final String name;
  private ActionContext parentContext;
  private final int contentVersion;
  private String key;
  private boolean preventRealtimeUpdating = false;
  private ContextualValues contextualValues;

  public static class ContextualValues {
    /**
     * Parameters from the triggering event or state.
     */
    public Map<String, ?> parameters;

    /**
     * Arguments from the triggering event or state.
     */
    public Map<String, Object> arguments;

    /**
     * The previous user attribute value.
     */
    public Object previousAttributeValue;

    /**
     * The current user attribute value.
     */
    public Object attributeValue;
  }

  public ActionContext(String name, Map<String, Object> args, String messageId) {
    this(name, args, messageId, null, Constants.Messaging.DEFAULT_PRIORITY);
  }

  public ActionContext(String name, Map<String, Object> args, final String messageId,
      final String originalMessageId, int priority) {
    super(messageId, originalMessageId);
    this.name = name;
    this.args = args;
    this.contentVersion = VarCache.contentVersion();
    this.priority = priority;
  }

  public void preventRealtimeUpdating() {
    preventRealtimeUpdating = true;
  }

  public void setContextualValues(ContextualValues values) {
    contextualValues = values;
  }

  public ContextualValues getContextualValues() {
    return contextualValues;
  }

  private static Map<String, Object> getDefinition(String name) {
    Map<String, Object> definition = CollectionUtil.uncheckedCast(
        VarCache.actionDefinitions().get(name));
    if (definition == null) {
      return new HashMap<>();
    }
    return definition;
  }

  private Map<String, Object> getDefinition() {
    return getDefinition(name);
  }

  private Map<String, Object> defaultValues() {
    Map<String, Object> values = CollectionUtil.uncheckedCast(getDefinition().get("values"));
    if (values == null) {
      return new HashMap<>();
    }
    return values;
  }

  private Map<String, String> kinds() {
    Map<String, String> kinds = CollectionUtil.uncheckedCast(getDefinition().get("kinds"));
    if (kinds == null) {
      return new HashMap<>();
    }
    return kinds;
  }

  public void update() {
    this.updateArgs(args, "", defaultValues());
  }

  @SuppressWarnings("unchecked")
  private void updateArgs(Map<String, Object> args,
      String prefix, Map<String, Object> defaultValues) {
    Map<String, String> kinds = kinds();
    for (Map.Entry<String, Object> entry : args.entrySet()) {
      String arg = entry.getKey();
      Object value = entry.getValue();
      Object defaultValue = defaultValues != null ? defaultValues.get(arg) : null;
      String kind = kinds.get(prefix + arg);
      if ((kind == null || !kind.equals(Constants.Kinds.ACTION)) && value instanceof Map &&
          !((Map<String, ?>) value).containsKey(Constants.Values.ACTION_ARG)) {
        Map<String, Object> defaultValueMap = (defaultValue instanceof Map) ?
            (Map<String, Object>) defaultValue : null;
        this.updateArgs((Map<String, Object>) value, prefix + arg + ".", defaultValueMap);
      } else {
        if (kind != null && kind.equals(Constants.Kinds.FILE) ||
            arg.contains(Constants.Values.FILE_PREFIX)) {
          FileManager.maybeDownloadFile(false, value.toString(),
              defaultValue != null ? defaultValue.toString() : null, null, null);

          // Need to check for null because server actions like push notifications aren't
          // defined in the SDK, and so there's no associated metadata.
        } else if (kind == null || kind.equals(Constants.Kinds.ACTION)) {
          Object actionArgsObj = objectNamed(prefix + arg);
          if (!(actionArgsObj instanceof Map)) {
            continue;
          }
          Map<String, Object> actionArgs = (Map<String, Object>) actionArgsObj;
          ActionContext context = new ActionContext(
              (String) actionArgs.get(Constants.Values.ACTION_ARG),
              actionArgs, messageId);
          context.update();
        }
      }
    }
  }

  public String actionName() {
    return name;
  }

  public <T> T objectNamed(String name) {
    if (TextUtils.isEmpty(name)) {
      Log.e("objectNamed - Invalid name parameter provided.");
      return null;
    }
    try {
      if (!preventRealtimeUpdating && VarCache.contentVersion() > contentVersion) {
        ActionContext parent = parentContext;
        if (parent != null) {
          args = parent.getChildArgs(key);
        } else if (messageId != null) {
          // This is sort of a best effort to display the most recent version of the message, if
          // this happens to be null, it probably means that it got changed somehow in between the
          // time when it was activated and displayed (e.g. by forceContentUpdate), in which case
          // we just ignore it and display the latest stable version.
          Map<String, Object> message = CollectionUtil.uncheckedCast(VarCache.messages().get
              (messageId));
          if (message != null) {
            args = CollectionUtil.uncheckedCast(message.get(Constants.Keys.VARS));
          }
        }
      }
      return VarCache.getMergedValueFromComponentArray(
          VarCache.getNameComponents(name), args);
    } catch (Throwable t) {
      Util.handleException(t);
      return null;
    }
  }

  public String stringNamed(String name) {
    if (TextUtils.isEmpty(name)) {
      Log.e("stringNamed - Invalid name parameter provided.");
      return null;
    }
    Object object = objectNamed(name);
    if (object == null) {
      return null;
    }
    try {
      return fillTemplate(object.toString());
    } catch (Throwable t) {
      Util.handleException(t);
      return object.toString();
    }
  }

  public String fillTemplate(String value) {
    if (contextualValues == null || value == null || !value.contains("##")) {
      return value;
    }
    if (contextualValues.parameters != null) {
      Map<String, ?> parameters = contextualValues.parameters;
      for (Map.Entry<String, ?> entry : parameters.entrySet()) {
        String placeholder = "##Parameter " + entry.getKey() + "##";
        value = value.replace(placeholder, "" + entry.getValue());
      }
    }
    if (contextualValues.previousAttributeValue != null) {
      value = value.replace("##Previous Value##",
          contextualValues.previousAttributeValue.toString());
    }
    if (contextualValues.attributeValue != null) {
      value = value.replace("##Value##", contextualValues.attributeValue.toString());
    }
    return value;
  }

  private String getDefaultValue(String name) {
    String[] components = name.split("\\.");
    Map<String, Object> defaultValues = defaultValues();
    for (int i = 0; i < components.length; i++) {
      if (defaultValues == null) {
        return null;
      }
      if (i == components.length - 1) {
        Object value = defaultValues.get(components[i]);
        return value == null ? null : value.toString();
      }
      defaultValues = CollectionUtil.uncheckedCast(defaultValues.get(components[i]));
    }
    return null;
  }

  public InputStream streamNamed(String name) {
    try {
      if (TextUtils.isEmpty(name)) {
        Log.e("streamNamed - Invalid name parameter provided.");
        return null;
      }
      String stringValue = stringNamed(name);
      String defaultValue = getDefaultValue(name);
      if ((stringValue == null || stringValue.length() == 0) &&
          (defaultValue == null || defaultValue.length() == 0)) {
        return null;
      }
      InputStream stream = FileManager.stream(false, null, null,
          FileManager.fileValue(stringValue, defaultValue, null), defaultValue, null);
      if (stream == null) {
        Log.e("Could not open stream named " + name);
      }
      return stream;
    } catch (Throwable t) {
      Util.handleException(t);
      return null;
    }
  }

  public boolean booleanNamed(String name) {
    if (TextUtils.isEmpty(name)) {
      Log.e("booleanNamed - Invalid name parameter provided.");
      return false;
    }
    Object object = objectNamed(name);
    try {
      if (object == null) {
        return false;
      } else if (object instanceof Boolean) {
        return (Boolean) object;
      }
      return convertToBoolean(object.toString());
    } catch (Throwable t) {
      Util.handleException(t);
      return false;
    }
  }

  /**
   * In contrast to Boolean.valueOf this function also converts 1, yes or similar string values
   * correctly to Boolean, e.g.: "1", "yes", "true", "on" --> true; "0", "no", "false", "off" -->
   * false; else null.
   *
   * @param value the text to convert to Boolean.
   * @return Boolean
   */
  private static boolean convertToBoolean(String value) {
    return "1".equalsIgnoreCase(value) || "yes".equalsIgnoreCase(value) ||
        "true".equalsIgnoreCase(value) || "on".equalsIgnoreCase(value);
  }

  public Number numberNamed(String name) {
    if (TextUtils.isEmpty(name)) {
      Log.e("numberNamed - Invalid name parameter provided.");
      return null;
    }
    Object object = objectNamed(name);
    try {
      if (object == null || TextUtils.isEmpty(object.toString())) {
        return 0.0;
      }
      if (object instanceof Number) {
        return (Number) object;
      }
      return Double.valueOf(object.toString());
    } catch (Throwable t) {
      Util.handleException(t);
      return 0.0;
    }
  }

  private Map<String, Object> getChildArgs(String name) {
    Object actionArgsObj = objectNamed(name);
    if (!(actionArgsObj instanceof Map)) {
      return null;
    }
    Map<String, Object> actionArgs = CollectionUtil.uncheckedCast(actionArgsObj);
    Map<String, Object> defaultArgs = CollectionUtil.uncheckedCast(getDefinition(
        (String) actionArgs.get(Constants.Values.ACTION_ARG)).get("values"));
    actionArgs = CollectionUtil.uncheckedCast(VarCache.mergeHelper(defaultArgs, actionArgs));
    return actionArgs;
  }

  public void runActionNamed(String name) {
    if (TextUtils.isEmpty(name)) {
      Log.e("runActionNamed - Invalid name parameter provided.");
      return;
    }
    Map<String, Object> args = getChildArgs(name);
    if (args == null) {
      return;
    }

    // Checks if action "Chain to Existing Message" started.
    if (!isChainToExistingMessageStarted(args, name)) {
      // Try to start action "Chain to a new Message".
      Object messageAction = args.get(Constants.Values.ACTION_ARG);
      if (messageAction != null) {
        createActionContextForMessageId(messageAction.toString(), args, messageId, name, false);
      }
    }
  }

  /**
   * Return true if here was an action for this message and we started it.
   */
  private boolean createActionContextForMessageId(String messageAction,
      Map<String, Object> messageArgs, String messageId, String name, Boolean chained) {
    try {
      final ActionContext actionContext = new ActionContext(messageAction,
          messageArgs, messageId);
      actionContext.contextualValues = contextualValues;
      actionContext.preventRealtimeUpdating = preventRealtimeUpdating;
      actionContext.isRooted = isRooted;
      actionContext.key = name;
      if (chained) {
        LeanplumInternal.triggerAction(actionContext, new VariablesChangedCallback() {
          @Override
          public void variablesChanged() {
            try {
              ActionManager.getInstance().recordMessageImpression(actionContext.getMessageId());
            } catch (Throwable t) {
              Util.handleException(t);
            }
          }
        });
      } else {
        actionContext.parentContext = this;
        LeanplumInternal.triggerAction(actionContext);
      }
      return true;
    } catch (Throwable t) {
      Util.handleException(t);
    }
    return false;
  }

  /**
   * Return true if there was action "Chain to Existing Message" and we started it.
   */
  private boolean isChainToExistingMessageStarted(Map<String, Object> args, final String name) {
    if (args == null) {
      return false;
    }

    final String messageId = getChainedMessageId(args);
    if (!shouldForceContentUpdateForChainedMessage(args)) {
      return executeChainedMessage(messageId, VarCache.messages(), name);
    } else {
      // message may not on the device yet, so we need to fetch it.
      Leanplum.forceContentUpdate(new VariablesChangedCallback() {
        @Override
        public void variablesChanged() {
          executeChainedMessage(messageId, VarCache.messages(), name);
        }
      });
    }
    return false;
  }

  /**
   * Return true if there is a chained message in the actionMap that is not yet loaded onto the device.
   */
  static boolean shouldForceContentUpdateForChainedMessage(Map<String, Object> actionMap) {
    if (actionMap == null) {
      return false;
    }
    String chainedMessageId = getChainedMessageId(actionMap);
    if (chainedMessageId != null
            && (VarCache.messages() == null || !VarCache.messages().containsKey(chainedMessageId))) {
        return true;
    }
    return false;
  }

  /**
   * Extract chained messageId from parent message's actionMap.  If it doesn't exist then return null.
   */
  static String getChainedMessageId(Map<String, Object> actionMap) {
    if (actionMap != null) {
      if (Constants.Values.CHAIN_MESSAGE_ACTION_NAME.equals(actionMap.get(Constants.Values.ACTION_ARG))) {
        return (String) actionMap.get(Constants.Values.CHAIN_MESSAGE_ARG);
      }
    }
    return null;
  }


  private boolean executeChainedMessage(String messageId, Map<String, Object> messages,
                                        String name) {
    if (messages == null) {
      return false;
    }
    Map<String, Object> message = CollectionUtil.uncheckedCast(messages.get(messageId));
    if (message != null) {
      Map<String, Object> messageArgs = CollectionUtil.uncheckedCast(
              message.get(Constants.Keys.VARS));
      Object messageAction = message.get("action");
      return messageAction != null && createActionContextForMessageId(messageAction.toString(),
              messageArgs, messageId, name, true);
    }
    return false;
  }

  /**
   * Prefix given event with all parent actionContext names to while filtering out the string
   * "action" (used in ExperimentVariable names but filtered out from event names).
   *
   * @param eventName Current event.
   * @return Prefixed event name with all parent actions.
   */
  private String eventWithParentEventNames(String eventName) {
    StringBuilder fullEventName = new StringBuilder();
    ActionContext context = this;
    List<ActionContext> parents = new ArrayList<>();
    while (context.parentContext != null) {
      parents.add(context);
      context = context.parentContext;
    }
    for (int i = parents.size() - 1; i >= -1; i--) {
      if (fullEventName.length() > 0) {
        fullEventName.append(' ');
      }
      String actionName;
      if (i > -1) {
        actionName = parents.get(i).key;
      } else {
        actionName = eventName;
      }
      if (actionName == null) {
        fullEventName = new StringBuilder("");
        break;
      }
      actionName = actionName.replace(" action", "");
      fullEventName.append(actionName);
    }

    return fullEventName.toString();
  }

  /**
   * Run the action with the given variable name, and track a message event with the name.
   *
   * @param name Action variable name to run.
   */
  public void runTrackedActionNamed(String name) {
    try {
      if (!Constants.isNoop() && messageId != null && isRooted) {
        if (TextUtils.isEmpty(name)) {
          Log.e("runTrackedActionNamed - Invalid name parameter provided.");
          return;
        }
        trackMessageEvent(name, 0.0, null, null);
      }
      runActionNamed(name);
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  /**
   * Track a message event with the given parameters. Any parent event names will be prepended to
   * given event name.
   *
   * @param event Name of event.
   * @param value Value for event.
   * @param info Info for event.
   * @param params Dictionary of params for event.
   */
  public void trackMessageEvent(String event, double value, String info,
      Map<String, Object> params) {
    try {
      if (!Constants.isNoop() && this.messageId != null) {
        if (TextUtils.isEmpty(event)) {
          Log.e("trackMessageEvent - Invalid event parameter provided.");
          return;
        }

        event = eventWithParentEventNames(event);
        if (TextUtils.isEmpty(event)) {
          Log.e("trackMessageEvent - Failed to generate parent action names.");
          return;
        }

        Map<String, String> requestArgs = new HashMap<>();
        requestArgs.put(Constants.Params.MESSAGE_ID, messageId);
        LeanplumInternal.track(event, value, info, params, requestArgs);
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  public void track(String event, double value, Map<String, Object> params) {
    try {
      if (!Constants.isNoop() && this.messageId != null) {
        if (TextUtils.isEmpty(event)) {
          Log.e("track - Invalid event parameter provided.");
          return;
        }
        Map<String, String> requestArgs = new HashMap<>();
        requestArgs.put(Constants.Params.MESSAGE_ID, messageId);
        LeanplumInternal.track(event, value, null, params, requestArgs);
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  public void muteFutureMessagesOfSameKind() {
    try {
      ActionManager.getInstance().muteFutureMessagesOfKind(messageId);
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  public int compareTo(@NonNull ActionContext other) {
    return priority - other.getPriority();
  }

  /**
   * Returns path to requested file.
   */
  public static String filePath(String stringValue) {
    return FileManager.fileValue(stringValue);
  }

  public static JSONObject mapToJsonObject(Map<String, ?> map) throws JSONException {
    return JsonConverter.mapToJsonObject(map);
  }

  public static <T> Map<String, T> mapFromJson(JSONObject jsonObject) throws JSONException {
    return JsonConverter.mapFromJson(jsonObject);
  }
}
