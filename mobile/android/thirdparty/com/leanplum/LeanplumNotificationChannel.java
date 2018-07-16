package com.leanplum;

/*
 * Copyright 2017, Leanplum, Inc. All rights reserved.
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

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import com.leanplum.internal.CollectionUtil;
import com.leanplum.internal.Constants;
import com.leanplum.internal.JsonConverter;
import com.leanplum.internal.Log;
import com.leanplum.internal.Util;
import com.leanplum.utils.BuildUtil;
import com.leanplum.utils.SharedPreferencesUtil;

import org.json.JSONArray;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Push notification channels manipulation utilities. Please use this class for Android O and upper
 * with targetSdkVersion 26 and upper.
 *
 * @author Anna Orlova
 */
@TargetApi(26)
class LeanplumNotificationChannel {

    /**
     * Configures notification channels.
     *
     * @param context  The application context.
     * @param channels Notification channels.
     */
    static void configureNotificationChannels(Context context, JSONArray channels) {
        try {
            if (context == null || channels == null) {
                return;
            }

            List<HashMap<String, Object>> definedChannels = retrieveNotificationChannels(context);
            List<HashMap<String, Object>> notificationChannels = JsonConverter.listFromJson(channels);

            if (definedChannels != null && notificationChannels != null) {
                // Find difference between present and newly received channels.
                definedChannels.removeAll(notificationChannels);
                // Delete channels that are no longer present.
                for (HashMap<String, Object> channel : definedChannels) {
                    if (channel == null) {
                        continue;
                    }
                    String id = (String) channel.get("id");
                    deleteNotificationChannel(context, id);
                }
            }

            // Store newly received channels.
            storeNotificationChannels(context, notificationChannels);

            // Configure channels.
            if (notificationChannels != null) {
                for (HashMap<String, Object> channel : notificationChannels) {
                    if (channel == null) {
                        continue;
                    }
                    createNotificationChannel(context, channel);
                }
            }
        } catch (Throwable t) {
            Util.handleException(t);
        }
    }

    /**
     * Configures default notification channel, which will be used when channel isn't specified on
     * Android O.
     *
     * @param context The application context.
     * @param channel Default channel details.
     */
    static void configureDefaultNotificationChannel(Context context, String channel) {
        try {
            if (context == null || TextUtils.isEmpty(channel)) {
                return;
            }
            storeDefaultNotificationChannel(context, channel);
        } catch (Throwable t) {
            Util.handleException(t);
        }
    }

    /**
     * Configures notification groups.
     *
     * @param context The application context.
     * @param groups  Notification groups.
     */
    static void configureNotificationGroups(Context context, JSONArray groups) {
        try {
            if (context == null || groups == null) {
                return;
            }
            List<HashMap<String, Object>> definedGroups = retrieveNotificationGroups(context);
            List<HashMap<String, Object>> notificationGroups = JsonConverter.listFromJson(groups);

            if (definedGroups != null && notificationGroups != null) {
                definedGroups.removeAll(notificationGroups);

                // Delete groups that are no longer present.
                for (HashMap<String, Object> group : definedGroups) {
                    if (group == null) {
                        continue;
                    }
                    String id = (String) group.get("id");
                    deleteNotificationGroup(context, id);
                }
            }

            // Store newly received groups.
            storeNotificationGroups(context, notificationGroups);

            // Configure groups.
            if (notificationGroups != null) {
                for (HashMap<String, Object> group : notificationGroups) {
                    if (group == null) {
                        continue;
                    }
                    createNotificationGroup(context, group);
                }
            }
        } catch (Throwable t) {
            Util.handleException(t);
        }
    }

    /**
     * Retrieves stored notification channels.
     *
     * @param context The application context.
     * @return List of stored channels or null.
     */
    private static List<HashMap<String, Object>> retrieveNotificationChannels(Context context) {
        if (context == null) {
            return null;
        }
        try {
            SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                    Context.MODE_PRIVATE);
            String jsonChannels = preferences.getString(Constants.Defaults.NOTIFICATION_CHANNELS_KEY,
                    null);
            if (jsonChannels == null) {
                return null;
            }
            JSONArray json = new JSONArray(jsonChannels);
            return JsonConverter.listFromJson(json);
        } catch (Exception e) {
            Log.e("Failed to convert notification channels json.");
        }
        return null;
    }

    /**
     * Retrieves stored default notification channel id.
     *
     * @param context The Application context.
     * @return Id of default channel.
     */
    private static String retrieveDefaultNotificationChannel(Context context) {
        if (context == null) {
            return null;
        }
        try {
            SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                    Context.MODE_PRIVATE);
            return preferences.getString(Constants.Defaults.DEFAULT_NOTIFICATION_CHANNEL_KEY, null);
        } catch (Exception e) {
            Log.e("Failed to convert default notification channels json.");
        }
        return null;
    }

    /**
     * Retrieves stored notification groups.
     *
     * @param context The application context.
     * @return List of stored groups or null.
     */
    private static List<HashMap<String, Object>> retrieveNotificationGroups(Context context) {
        if (context == null) {
            return null;
        }
        try {
            SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                    Context.MODE_PRIVATE);
            String jsonChannels = preferences.getString(Constants.Defaults.NOTIFICATION_GROUPS_KEY, null);
            if (jsonChannels == null) {
                return null;
            }
            JSONArray json = new JSONArray(jsonChannels);
            return JsonConverter.listFromJson(json);
        } catch (Exception e) {
            Log.e("Failed to convert notification channels json.");
        }
        return null;
    }

    /**
     * Stores notification channels.
     *
     * @param context  The application context.
     * @param channels Channels to store.
     */
    private static void storeNotificationChannels(Context context,
                                                  List<HashMap<String, Object>> channels) {
        if (context == null || channels == null) {
            return;
        }
        SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();

        String jsonChannels = new JSONArray(channels).toString();
        editor.putString(Constants.Defaults.NOTIFICATION_CHANNELS_KEY, jsonChannels);

        SharedPreferencesUtil.commitChanges(editor);
    }

    /**
     * Stores default notification channel id.
     *
     * @param context   The application context.
     * @param channelId Channel Id to store.
     */
    private static void storeDefaultNotificationChannel(Context context, String channelId) {
        if (context == null || channelId == null) {
            return;
        }
        SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();

        editor.putString(Constants.Defaults.DEFAULT_NOTIFICATION_CHANNEL_KEY, channelId);

        SharedPreferencesUtil.commitChanges(editor);
    }

    /**
     * Stores notification groups.
     *
     * @param context The application context.
     * @param groups  Groups to store.
     */
    private static void storeNotificationGroups(Context context,
                                                List<HashMap<String, Object>> groups) {
        if (context == null || groups == null) {
            return;
        }

        SharedPreferences preferences = context.getSharedPreferences(Constants.Defaults.LEANPLUM,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();

        String jsonGroups = new JSONArray(groups).toString();
        editor.putString(Constants.Defaults.NOTIFICATION_GROUPS_KEY, jsonGroups);

        SharedPreferencesUtil.commitChanges(editor);
    }

    /**
     * Creates push notification channel.
     *
     * @param context The application context.
     * @param channel Map containing channel details.
     * @return Id of newly created channel or null if it fails.
     */
    static String createNotificationChannel(Context context, Map<String, Object> channel) {
        try {
            if (context == null || channel == null) {
                return null;
            }
            NotificationChannelData data = new NotificationChannelData(channel);
            createNotificationChannel(context,
                    data.id,
                    data.name,
                    data.importance,
                    data.description,
                    data.groupId,
                    data.enableLights,
                    data.lightColor,
                    data.enableVibration,
                    data.vibrationPattern,
                    data.lockscreenVisibility,
                    data.bypassDnd,
                    data.showBadge);
            return data.id;
        } catch (Exception e) {
            Log.e("Failed to create notification channel.");
        }
        return null;
    }


    /**
     * Create push notification channel with provided id, name and importance of the channel.
     * You can call this method also when you need to update the name or description of a channel, at
     * this case you should use original channel id.
     *
     * @param context              The application context.
     * @param channelId            The id of the channel.
     * @param channelName          The user-visible name of the channel.
     * @param channelImportance    The importance of the channel. Use value from 0 to 5. 3 is default.
     *                             Read more https://developer.android.com/reference/android/app/NotificationManager.html#IMPORTANCE_DEFAULT
     *                             Once you create a notification channel, only the system can modify its importance.
     * @param channelDescription   The user-visible description of the channel.
     * @param groupId              The id of push notification channel group.
     * @param enableLights         True if lights enable for this channel.
     * @param lightColor           Light color for notifications posted to this channel, if the device supports
     *                             this feature.
     * @param enableVibration      True if vibration enable for this channel.
     * @param vibrationPattern     Vibration pattern for notifications posted to this channel.
     * @param lockscreenVisibility How to be shown on the lockscreen.
     * @param bypassDnd            Whether should notification bypass DND.
     * @param showBadge            Whether should notification show badge.
     */
    private static void createNotificationChannel(Context context, String channelId, String
            channelName, int channelImportance, String channelDescription, String groupId, boolean
                                                          enableLights, int lightColor, boolean enableVibration, long[] vibrationPattern, int
                                                          lockscreenVisibility, boolean bypassDnd, boolean showBadge) {
        if (context == null || TextUtils.isEmpty(channelId)) {
            return;
        }
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                NotificationManager notificationManager =
                        context.getSystemService(NotificationManager.class);
                if (notificationManager == null) {
                    Log.e("Notification manager is null");
                    return;
                }

                NotificationChannel notificationChannel = new NotificationChannel(channelId,
                        channelName, channelImportance);
                if (!TextUtils.isEmpty(channelDescription)) {
                    notificationChannel.setDescription(channelDescription);
                }
                if (enableLights) {
                    notificationChannel.enableLights(true);
                    notificationChannel.setLightColor(lightColor);
                }
                if (enableVibration) {
                    notificationChannel.enableVibration(true);
                    // Workaround for https://issuetracker.google.com/issues/63427588
                    if (vibrationPattern != null && vibrationPattern.length != 0) {
                        notificationChannel.setVibrationPattern(vibrationPattern);
                    }
                }
                if (!TextUtils.isEmpty(groupId)) {
                    notificationChannel.setGroup(groupId);
                }
                notificationChannel.setLockscreenVisibility(lockscreenVisibility);
                notificationChannel.setBypassDnd(bypassDnd);
                notificationChannel.setShowBadge(showBadge);

                notificationManager.createNotificationChannel(notificationChannel);
            } catch (Throwable t) {
                Util.handleException(t);
            }
        }
    }

    /**
     * Delete push notification channel.
     *
     * @param context   The application context.
     * @param channelId The id of the channel.
     */
    private static void deleteNotificationChannel(Context context, String channelId) {
        if (context == null) {
            return;
        }
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                NotificationManager notificationManager =
                        (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
                if (notificationManager == null) {
                    Log.e("Notification manager is null");
                    return;
                }

                notificationManager.deleteNotificationChannel(channelId);
            } catch (Throwable t) {
                Util.handleException(t);
            }
        }
    }

    /**
     * Create push notification channel group.
     *
     * @param context The application context.
     * @param group   Map containing group details.
     * @return Id of newly created group or null if its failed.
     */
    private static String createNotificationGroup(Context context, Map<String, Object> group) {
        if (context == null || group == null) {
            return null;
        }
        NotificationGroupData data = new NotificationGroupData(group);
        createNotificationGroup(context, data.id, data.name);
        return data.id;
    }

    /**
     * Create push notification channel group.
     *
     * @param context   The application context.
     * @param groupId   The id of the group.
     * @param groupName The user-visible name of the group.
     */
    private static void createNotificationGroup(Context context, String groupId, String groupName) {
        if (context == null || TextUtils.isEmpty(groupId)) {
            return;
        }
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                NotificationManager notificationManager =
                        (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
                if (notificationManager == null) {
                    Log.e("Notification manager is null");
                    return;
                }

                notificationManager.createNotificationChannelGroup(new NotificationChannelGroup(groupId,
                        groupName));
            } catch (Throwable t) {
                Util.handleException(t);
            }
        }
    }

    /**
     * Delete push notification channel group.
     *
     * @param context The application context.
     * @param groupId The id of the channel.
     */
    private static void deleteNotificationGroup(Context context, String groupId) {
        if (context == null || TextUtils.isEmpty(groupId)) {
            return;
        }
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                NotificationManager notificationManager =
                        (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
                if (notificationManager == null) {
                    Log.e("Notification manager is null");
                    return;
                }

                notificationManager.deleteNotificationChannelGroup(groupId);
            } catch (Throwable t) {
                Util.handleException(t);
            }
        }
    }

    /**
     * Get list of NotificationChannel.
     *
     * @param context The application context.
     * @return Returns all notification channels belonging to the calling package.
     */
    static List<NotificationChannel> getNotificationChannels(Context context) {
        if (BuildUtil.isNotificationChannelSupported(context)) {
            NotificationManager notificationManager =
                    (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (notificationManager == null) {
                Log.e("Notification manager is null");
                return null;
            }
            return notificationManager.getNotificationChannels();
        }
        return null;
    }

    /**
     * Get default notification channel id to be used.
     *
     * @param context The application context.
     * @return Id of default notification channel.
     */
    static String getDefaultNotificationChannelId(Context context) {
        if (BuildUtil.isNotificationChannelSupported(context)) {
            return retrieveDefaultNotificationChannel(context);
        }
        return null;
    }

    /**
     * Get list of Notification groups.
     *
     * @param context The application context.
     * @return Returns all notification groups.
     */
    static List<NotificationChannelGroup> getNotificationGroups(Context context) {
        if (BuildUtil.isNotificationChannelSupported(context)) {
            NotificationManager notificationManager = (NotificationManager) context.getSystemService(
                    Context.NOTIFICATION_SERVICE);
            if (notificationManager == null) {
                Log.e("Cannot get Notification Channel Groups, notificationManager is null.");
                return null;
            }
            return notificationManager.getNotificationChannelGroups();
        }
        return null;
    }

    /**
     * Helper class holding Notification Channel data parsed from JSON.
     */
    @TargetApi(26)
    private static class NotificationChannelData {
        String id;
        String name;
        String description;
        String groupId;
        int importance = NotificationManager.IMPORTANCE_DEFAULT;
        boolean enableLights = false;
        int lightColor = 0;
        boolean enableVibration = false;
        long[] vibrationPattern = null;
        int lockscreenVisibility = Notification.VISIBILITY_PUBLIC;
        boolean bypassDnd = false;
        boolean showBadge = false;

        NotificationChannelData(Map<String, Object> channel) {
            id = (String) channel.get("id");
            name = (String) channel.get("name");
            description = (String) channel.get("description");
            groupId = (String) channel.get("groupId");

            importance = (int) CollectionUtil.getOrDefault(channel, "importance",
                    importance);
            enableLights = (boolean) CollectionUtil.getOrDefault(channel, "enable_lights", enableLights);
            lightColor = (int) CollectionUtil.getOrDefault(channel, "light_color", lightColor);
            enableVibration = (boolean) CollectionUtil.getOrDefault(channel, "enable_vibration",
                    enableVibration);
            lockscreenVisibility = (int) CollectionUtil.getOrDefault(channel, "lockscreen_visibility",
                    lockscreenVisibility);
            bypassDnd = (boolean) CollectionUtil.getOrDefault(channel, "bypass_dnd", bypassDnd);
            showBadge = (boolean) CollectionUtil.getOrDefault(channel, "show_badge", showBadge);

            try {
                List<Number> pattern = CollectionUtil.uncheckedCast(
                        CollectionUtil.getOrDefault(channel, "vibration_pattern", null));
                if (pattern != null) {
                    vibrationPattern = new long[pattern.size()];
                    Iterator<Number> iterator = pattern.iterator();
                    for (int i = 0; i < vibrationPattern.length; i++) {
                        Number next = iterator.next();
                        if (next != null) {
                            vibrationPattern[i] = next.longValue();
                        }
                    }
                }
            } catch (Exception e) {
                Log.w("Failed to parse vibration pattern.");
            }

            // Sanity checks.
            if (importance < NotificationManager.IMPORTANCE_NONE &&
                    importance > NotificationManager.IMPORTANCE_MAX) {
                importance = NotificationManager.IMPORTANCE_DEFAULT;
            }
            if (lockscreenVisibility < Notification.VISIBILITY_SECRET &&
                    lockscreenVisibility > Notification.VISIBILITY_PUBLIC) {
                lockscreenVisibility = Notification.VISIBILITY_PUBLIC;
            }
        }
    }

    /**
     * Helper class holding Notification Group data parsed from JSON.
     */
    @TargetApi(26)
    private static class NotificationGroupData {
        String id;
        String name;

        NotificationGroupData(Map<String, Object> group) {
            id = (String) group.get("id");
            name = (String) group.get("name");
        }
    }
}
