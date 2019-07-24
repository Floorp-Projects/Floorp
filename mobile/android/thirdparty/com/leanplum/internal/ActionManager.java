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

package com.leanplum.internal;

import android.content.Context;
import android.content.SharedPreferences;

import com.leanplum.ActionContext;
import com.leanplum.ActionContext.ContextualValues;
import com.leanplum.Leanplum;
import com.leanplum.LocationManager;
import com.leanplum.callbacks.ActionCallback;
import com.leanplum.utils.SharedPreferencesUtil;

import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Handles in-app and push messaging.
 *
 * @author Andrew First
 */
public class ActionManager {
  private Map<String, Map<String, Number>> messageImpressionOccurrences;
  private Map<String, Number> messageTriggerOccurrences;
  private Map<String, Number> sessionOccurrences;

  private static ActionManager instance;

  public static final String PUSH_NOTIFICATION_ACTION_NAME = "__Push Notification";
  public static final String HELD_BACK_ACTION_NAME = "__held_back";
  private static final String LEANPLUM_LOCAL_PUSH_HELPER =
      "com.leanplum.internal.LeanplumLocalPushHelper";
  private static final String PREFERENCES_NAME = "__leanplum_messaging__";
  private static LocationManager locationManager;
  private static boolean loggedLocationManagerFailure = false;

  public static class MessageMatchResult {
    public boolean matchedTrigger;
    public boolean matchedUnlessTrigger;
    public boolean matchedLimit;
    public boolean matchedActivePeriod;
  }

  public static synchronized ActionManager getInstance() {
    if (instance == null) {
      instance = new ActionManager();
    }
    return instance;
  }

  public static synchronized LocationManager getLocationManager() {
    if (locationManager != null) {
      return locationManager;
    }

    if (Util.hasPlayServices()) {
      try {
        // Reflection here prevents linker errors
        // if Google Play Services is not used in the client app.
        locationManager = (LocationManager) Class
            .forName("com.leanplum.LocationManagerImplementation")
            .getMethod("instance").invoke(null);
        return locationManager;
      } catch (Throwable t) {
        if (!loggedLocationManagerFailure) {
          Log.w("Geofencing support requires leanplum-location module and Google Play " +
              "Services v8.1 and higher.\n" +
              "Add this to your build.gradle file:\n" +
              "implementation 'com.google.android.gms:play-services-location:8.3.0+'\n" +
              "implementation 'com.leanplum:leanplum-location:+'");
          loggedLocationManagerFailure = true;
        }
      }
    }
    return null;
  }

  private ActionManager() {
    listenForLocalNotifications();
    sessionOccurrences = new HashMap<>();
    messageImpressionOccurrences = new HashMap<>();
    messageTriggerOccurrences = new HashMap<>();
  }

  private void listenForLocalNotifications() {
    Leanplum.onAction(PUSH_NOTIFICATION_ACTION_NAME, new ActionCallback() {
      @Override
      public boolean onResponse(ActionContext actionContext) {
        try {
          String messageId = actionContext.getMessageId();

          // Get eta.
          Object countdownObj;
          if (((BaseActionContext) actionContext).isPreview()) {
            countdownObj = 5.0;
          } else {
            Map<String, Object> messageConfig = CollectionUtil.uncheckedCast(
                VarCache.getMessageDiffs().get(messageId));
            if (messageConfig == null) {
              Log.e("Could not find message options for ID " + messageId);
              return false;
            }
            countdownObj = messageConfig.get("countdown");
          }
          if (!(countdownObj instanceof Number)) {
            Log.e("Invalid notification countdown: " + countdownObj);
            return false;
          }
          long eta = System.currentTimeMillis() + ((Number) countdownObj).longValue() * 1000L;
          // Schedule notification.
          try {
            return (boolean) Class.forName(LEANPLUM_LOCAL_PUSH_HELPER)
                .getDeclaredMethod("scheduleLocalPush", ActionContext.class, String.class,
                    long.class).invoke(new Object(), actionContext, messageId, eta);
          } catch (Throwable throwable) {
            return false;
          }
        } catch (Throwable t) {
          Util.handleException(t);
          return false;
        }
      }

    });

    Leanplum.onAction("__Cancel" + PUSH_NOTIFICATION_ACTION_NAME, new ActionCallback() {
      @Override
      public boolean onResponse(ActionContext actionContext) {
        try {
          String messageId = actionContext.getMessageId();

          // Get existing eta and clear notification from preferences.
          Context context = Leanplum.getContext();
          SharedPreferences preferences = context.getSharedPreferences(
              PREFERENCES_NAME, Context.MODE_PRIVATE);
          String preferencesKey = String.format(Constants.Defaults.LOCAL_NOTIFICATION_KEY,
              messageId);
          long existingEta = preferences.getLong(preferencesKey, 0L);
          SharedPreferences.Editor editor = preferences.edit();
          editor.remove(preferencesKey);
          SharedPreferencesUtil.commitChanges(editor);

          // Cancel notification.
          try {
            Class.forName(LEANPLUM_LOCAL_PUSH_HELPER)
                .getDeclaredMethod("cancelLocalPush", Context.class, String.class)
                .invoke(new Object(), context, messageId);
            boolean didCancel = existingEta > System.currentTimeMillis();
            if (didCancel) {
              Log.i("Cancelled notification");
            }
            return didCancel;
          } catch (Throwable throwable) {
            return false;
          }
        } catch (Throwable t) {
          Util.handleException(t);
          return false;
        }
      }
    });
  }

  public Map<String, Number> getMessageImpressionOccurrences(String messageId) {
    Map<String, Number> occurrences = messageImpressionOccurrences.get(messageId);
    if (occurrences != null) {
      return occurrences;
    }
    Context context = Leanplum.getContext();
    SharedPreferences preferences = context.getSharedPreferences(
        PREFERENCES_NAME, Context.MODE_PRIVATE);
    String savedValue = preferences.getString(
        String.format(Constants.Defaults.MESSAGE_IMPRESSION_OCCURRENCES_KEY, messageId),
        "{}");
    occurrences = CollectionUtil.uncheckedCast(JsonConverter.fromJson(savedValue));
    messageImpressionOccurrences.put(messageId, occurrences);
    return occurrences;
  }

  public void saveMessageImpressionOccurrences(Map<String, Number> occurrences, String messageId) {
    Context context = Leanplum.getContext();
    SharedPreferences preferences = context.getSharedPreferences(
        PREFERENCES_NAME, Context.MODE_PRIVATE);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putString(
        String.format(Constants.Defaults.MESSAGE_IMPRESSION_OCCURRENCES_KEY, messageId),
        JsonConverter.toJson(occurrences));
    messageImpressionOccurrences.put(messageId, occurrences);
    SharedPreferencesUtil.commitChanges(editor);
  }

  public int getMessageTriggerOccurrences(String messageId) {
    Number occurrences = messageTriggerOccurrences.get(messageId);
    if (occurrences != null) {
      return occurrences.intValue();
    }
    Context context = Leanplum.getContext();
    SharedPreferences preferences = context.getSharedPreferences(
        PREFERENCES_NAME, Context.MODE_PRIVATE);
    int savedValue = preferences.getInt(
        String.format(Constants.Defaults.MESSAGE_TRIGGER_OCCURRENCES_KEY, messageId), 0);
    messageTriggerOccurrences.put(messageId, savedValue);
    return savedValue;
  }

  public void saveMessageTriggerOccurrences(int occurrences, String messageId) {
    Context context = Leanplum.getContext();
    SharedPreferences preferences = context.getSharedPreferences(
        PREFERENCES_NAME, Context.MODE_PRIVATE);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putInt(
        String.format(Constants.Defaults.MESSAGE_TRIGGER_OCCURRENCES_KEY, messageId), occurrences);
    messageTriggerOccurrences.put(messageId, occurrences);
    SharedPreferencesUtil.commitChanges(editor);
  }

  public MessageMatchResult shouldShowMessage(String messageId, Map<String, Object> messageConfig,
      String when, String eventName, ContextualValues contextualValues) {
    MessageMatchResult result = new MessageMatchResult();

    // 1. Must not be muted.
    Context context = Leanplum.getContext();
    SharedPreferences preferences = context.getSharedPreferences(
        PREFERENCES_NAME, Context.MODE_PRIVATE);
    if (preferences.getBoolean(
        String.format(Constants.Defaults.MESSAGE_MUTED_KEY, messageId), false)) {
      return result;
    }

    // 2. Must match at least one trigger.
    result.matchedTrigger = matchedTriggers(messageConfig.get("whenTriggers"), when, eventName,
        contextualValues);
    result.matchedUnlessTrigger =
        matchedTriggers(messageConfig.get("unlessTriggers"), when, eventName, contextualValues);
    if (!result.matchedTrigger && !result.matchedUnlessTrigger) {
      return result;
    }

    // 3. Must match all limit conditions.
    Object limitConfigObj = messageConfig.get("whenLimits");
    Map<String, Object> limitConfig = null;
    if (limitConfigObj instanceof Map<?, ?>) {
      limitConfig = CollectionUtil.uncheckedCast(limitConfigObj);
    }
    result.matchedLimit = matchesLimits(messageId, limitConfig);

    // 4. Must be within active period.
    Object messageStartTime = messageConfig.get("startTime");
    Object messageEndTime = messageConfig.get("endTime");
    if (messageStartTime == null || messageEndTime == null) {
      result.matchedActivePeriod = true;
    } else {
      long currentTime = new Date().getTime();
      result.matchedActivePeriod = currentTime >= (long) messageStartTime &&
          currentTime <= (long) messageEndTime;
    }

    return result;
  }

  private boolean matchesLimits(String messageId, Map<String, Object> limitConfig) {
    if (limitConfig == null) {
      return true;
    }
    List<Object> limits = CollectionUtil.uncheckedCast(limitConfig.get("children"));
    if (limits.isEmpty()) {
      return true;
    }
    Map<String, Number> impressionOccurrences = getMessageImpressionOccurrences(messageId);
    int triggerOccurrences = getMessageTriggerOccurrences(messageId) + 1;
    for (Object limitObj : limits) {
      Map<String, Object> limit = CollectionUtil.uncheckedCast(limitObj);
      String subject = limit.get("subject").toString();
      String noun = limit.get("noun").toString();
      String verb = limit.get("verb").toString();

      // E.g. 5 times per session; 2 times per 7 minutes.
      if (subject.equals("times")) {
        List<Object> objects = CollectionUtil.uncheckedCast(limit.get("objects"));
        int perTimeUnit = objects.size() > 0 ?
            Integer.parseInt(objects.get(0).toString()) : 0;
        if (!matchesLimitTimes(Integer.parseInt(noun),
            perTimeUnit, verb, impressionOccurrences, messageId)) {
          return false;
        }

        // E.g. On the 5th occurrence.
      } else if (subject.equals("onNthOccurrence")) {
        int amount = Integer.parseInt(noun);
        if (triggerOccurrences != amount) {
          return false;
        }

        // E.g. Every 5th occurrence.
      } else if (subject.equals("everyNthOccurrence")) {
        int multiple = Integer.parseInt(noun);
        if (multiple == 0 || triggerOccurrences % multiple != 0) {
          return false;
        }
      }
    }
    return true;
  }

  private boolean matchesLimitTimes(int amount, int time, String units,
      Map<String, Number> occurrences, String messageId) {
    Number existing = 0L;
    if (units.equals("limitSession")) {
      existing = sessionOccurrences.get(messageId);
      if (existing == null) {
        existing = 0L;
      }
    } else {
      if (occurrences == null || occurrences.isEmpty()) {
        return true;
      }
      Number min = occurrences.get("min");
      Number max = occurrences.get("max");
      if (min == null) {
        min = 0L;
      }
      if (max == null) {
        max = 0L;
      }
      if (units.equals("limitUser")) {
        existing = max.longValue() - min.longValue() + 1;
      } else {
        if (units.equals("limitMinute")) {
          time *= 60;
        } else if (units.equals("limitHour")) {
          time *= 3600;
        } else if (units.equals("limitDay")) {
          time *= 86400;
        } else if (units.equals("limitWeek")) {
          time *= 604800;
        } else if (units.equals("limitMonth")) {
          time *= 2592000;
        }
        long now = System.currentTimeMillis();
        int matchedOccurrences = 0;
        for (long i = max.longValue(); i >= min.longValue(); i--) {
          if (occurrences.containsKey("" + i)) {
            long timeAgo = (now - occurrences.get("" + i).longValue()) / 1000;
            if (timeAgo > time) {
              break;
            }
            matchedOccurrences++;
            if (matchedOccurrences >= amount) {
              return false;
            }
          }
        }
      }
    }
    return existing.longValue() < amount;
  }

  private boolean matchedTriggers(Object triggerConfigObj, String when, String eventName,
      ContextualValues contextualValues) {
    if (triggerConfigObj instanceof Map<?, ?>) {
      Map<String, Object> triggerConfig = CollectionUtil.uncheckedCast(triggerConfigObj);
      List<Object> triggers = CollectionUtil.uncheckedCast(triggerConfig.get("children"));
      for (Object triggerObj : triggers) {
        Map<String, Object> trigger = CollectionUtil.uncheckedCast(triggerObj);
        if (matchedTrigger(trigger, when, eventName, contextualValues)) {
          return true;
        }
      }
    }
    return false;
  }

  private boolean matchedTrigger(Map<String, Object> trigger, String when, String eventName,
      ContextualValues contextualValues) {
    String subject = (String) trigger.get("subject");
    if (subject.equals(when)) {
      String noun = (String) trigger.get("noun");
      if ((noun == null && eventName == null) || (noun != null && noun.equals(eventName))) {
        String verb = (String) trigger.get("verb");
        List<Object> objects = CollectionUtil.uncheckedCast(trigger.get("objects"));

        // Evaluate user attribute changed to value.
        if ("changesTo".equals(verb)) {
          if (contextualValues != null && objects != null) {
            for (Object object : objects) {
              if ((object == null && contextualValues.attributeValue == null) ||
                  (object != null && object.toString().equalsIgnoreCase(
                      contextualValues.attributeValue.toString()))) {
                return true;
              }
            }
          }
          return false;
        }

        // Evaluate user attribute changed from value to value.
        if ("changesFromTo".equals(verb)) {
          return contextualValues != null &&
              objects.size() == 2 && objects.get(0) != null && objects.get(1) != null &&
              contextualValues.previousAttributeValue != null &&
              contextualValues.attributeValue != null &&
              objects.get(0).toString().equalsIgnoreCase(
                  contextualValues.previousAttributeValue.toString()) &&
              objects.get(1).toString().equalsIgnoreCase(
                  contextualValues.attributeValue.toString());
        }

        // Evaluate event parameter is value.
        if ("triggersWithParameter".equals(verb)) {
          if (contextualValues != null &&
              objects.size() == 2 && objects.get(0) != null && objects.get(1) != null &&
              contextualValues.parameters != null) {
            Object parameterValue = contextualValues.parameters.get(objects.get(0));
            return parameterValue != null && parameterValue.toString().equalsIgnoreCase(
                objects.get(1).toString());
          }
          return false;
        }

        return true;
      }
    }
    return false;
  }

  public void recordMessageTrigger(String messageId) {
    int occurrences = getMessageTriggerOccurrences(messageId);
    occurrences++;
    saveMessageTriggerOccurrences(occurrences, messageId);
    Leanplum.countAggregator().incrementCount("record_message_trigger");
  }

  /**
   * Tracks the "Held Back" event for a message and records the held back occurrences.
   *
   * @param messageId The spoofed ID of the message.
   * @param originalMessageId The original ID of the held back message.
   */
  public void recordHeldBackImpression(String messageId, String originalMessageId) {
    recordImpression(messageId, originalMessageId);
  }

  /**
   * Tracks the "Open" event for a message and records it's occurrence.
   *
   * @param messageId The ID of the message
   */
  public void recordMessageImpression(String messageId) {
    recordImpression(messageId, null);
  }

  /**
   * Records the occurrence of a message and tracks the correct impression event.
   *
   * @param messageId The ID of the message.
   * @param originalMessageId The original message ID of the held back message. Supply this only if
   * the message is held back. Otherwise, use null.
   */
  private void recordImpression(String messageId, String originalMessageId) {
    Map<String, String> requestArgs = new HashMap<>();
    if (originalMessageId != null) {
      // This is a held back message - track the event with the original message ID.
      requestArgs.put(Constants.Params.MESSAGE_ID, originalMessageId);
      LeanplumInternal.track(Constants.HELD_BACK_EVENT_NAME, 0.0, null, null, requestArgs);
    } else {
      // Track the message impression and occurrence.
      requestArgs.put(Constants.Params.MESSAGE_ID, messageId);
      LeanplumInternal.track(null, 0.0, null, null, requestArgs);
    }

    // Record session occurrences.
    Number existing = sessionOccurrences.get(messageId);
    if (existing == null) {
      existing = 0L;
    }
    existing = existing.longValue() + 1L;
    sessionOccurrences.put(messageId, existing);

    // Record cross-session occurrences.
    Map<String, Number> occurrences = getMessageImpressionOccurrences(messageId);
    if (occurrences == null || occurrences.isEmpty()) {
      occurrences = new HashMap<>();
      occurrences.put("min", 0L);
      occurrences.put("max", 0L);
      occurrences.put("0", System.currentTimeMillis());
    } else {
      Number min = occurrences.get("min");
      Number max = occurrences.get("max");
      if (min == null) {
        min = 0L;
      }
      if (max == null) {
        max = 0L;
      }
      max = max.longValue() + 1L;
      occurrences.put("" + max, System.currentTimeMillis());
      if (max.longValue() - min.longValue() + 1 >
          Constants.Messaging.MAX_STORED_OCCURRENCES_PER_MESSAGE) {
        occurrences.remove("" + min);
        min = min.longValue() + 1L;
        occurrences.put("min", min);
      }
      occurrences.put("max", max);
    }
    saveMessageImpressionOccurrences(occurrences, messageId);
  }

  public void muteFutureMessagesOfKind(String messageId) {
    if (messageId != null) {
      Context context = Leanplum.getContext();
      SharedPreferences preferences = context.getSharedPreferences(
          PREFERENCES_NAME, Context.MODE_PRIVATE);
      SharedPreferences.Editor editor = preferences.edit();
      editor.putBoolean(
          String.format(Constants.Defaults.MESSAGE_MUTED_KEY, messageId),
          true);
      SharedPreferencesUtil.commitChanges(editor);
    }
  }


  public static void getForegroundandBackgroundRegionNames(Set<String> foregroundRegionNames,
      Set<String> backgroundRegionNames) {
    Map<String, Object> messages = VarCache.messages();
    for (String messageId : messages.keySet()) {
      Map<String, Object> messageConfig = CollectionUtil.uncheckedCast(messages.get(messageId));
      Set<String> regionNames;
      Object action = messageConfig.get("action");
      if (action instanceof String) {
        if (action.equals(PUSH_NOTIFICATION_ACTION_NAME)) {
          regionNames = backgroundRegionNames;
        } else {
          regionNames = foregroundRegionNames;
        }

        Map<String, Object> whenTriggers = CollectionUtil.uncheckedCast(messageConfig.get
            ("whenTriggers"));
        Map<String, Object> unlessTriggers = CollectionUtil.uncheckedCast(messageConfig.get
            ("unlessTriggers"));

        addRegionNamesFromTriggersToSet(whenTriggers, regionNames);
        addRegionNamesFromTriggersToSet(unlessTriggers, regionNames);
      }
    }
  }

  public static void addRegionNamesFromTriggersToSet(
      Map<String, Object> triggerConfig, Set<String> set) {
    if (triggerConfig == null) {
      return;
    }
    List<Map<String, Object>> triggers = CollectionUtil.uncheckedCast(triggerConfig.get
        ("children"));
    for (Map<String, Object> trigger : triggers) {
      String subject = (String) trigger.get("subject");
      if (subject.equals("enterRegion") || subject.equals("exitRegion")) {
        set.add((String) trigger.get("noun"));
      }
    }
  }
}
