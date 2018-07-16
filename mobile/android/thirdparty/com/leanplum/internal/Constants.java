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


import org.mozilla.gecko.thirdparty_unused.BuildConfig;

/**
 * Leanplum constants.
 *
 * @author Andrew First.
 */
public class Constants {
  public static String API_HOST_NAME = "api.leanplum.com";
  public static String API_SERVLET = "api";
  public static boolean API_SSL = true;
  public static String SOCKET_HOST = "dev.leanplum.com";
  public static int SOCKET_PORT = 80;
  public static int NETWORK_TIMEOUT_SECONDS = 10;
  public static int NETWORK_TIMEOUT_SECONDS_FOR_DOWNLOADS = 10;
  static final String LEANPLUM_PACKAGE_IDENTIFIER = "s"; //TODO investigate what this should be

  public static String LEANPLUM_VERSION = "2.2.2-SNAPSHOT";
  public static String CLIENT = "android";

  static final String INVALID_MAC_ADDRESS = "02:00:00:00:00:00";
  static final String INVALID_MAC_ADDRESS_HASH = "0f607264fc6318a92b9e13c65db7cd3c";

  /**
   * From very old versions of the SDK, leading zeros were stripped from the mac address.
   */
  static final String OLD_INVALID_MAC_ADDRESS_HASH = "f607264fc6318a92b9e13c65db7cd3c";

  static final String INVALID_ANDROID_ID = "9774d56d682e549c";
  static final int MAX_DEVICE_ID_LENGTH = 400;
  static final int MAX_USER_ID_LENGTH = 400;

  public static String defaultDeviceId = null;
  public static boolean hashFilesToDetermineModifications = true;
  public static boolean isDevelopmentModeEnabled = false;
  public static boolean loggingEnabled = false;
  public static boolean isTestMode = false;
  public static boolean enableVerboseLoggingInDevelopmentMode = false;
  public static boolean enableFileUploadingInDevelopmentMode = true;
  public static boolean canDownloadContentMidSessionInProduction = false;
  static boolean isInPermanentFailureState = false;

  public static boolean isNoop() {
    return isTestMode || isInPermanentFailureState;
  }

  public static class Defaults {
    public static final String LEANPLUM = "__leanplum__";
    public static final String COUNT_KEY = "__leanplum_unsynced";
    public static final String ITEM_KEY = "__leanplum_unsynced_%d";
    public static final String UUID_KEY = "__leanplum_uuid";
    public static final String VARIABLES_KEY = "__leanplum_variables";
    public static final String ATTRIBUTES_KEY = "__leanplum_attributes";
    public static final String TOKEN_KEY = "__leanplum_token";
    public static final String MESSAGES_KEY = "__leanplum_messages";
    public static final String UPDATE_RULES_KEY = "__leanplum_update_rules";
    public static final String EVENT_RULES_KEY = "__leanplum_event_rules";
    public static final String REGIONS_KEY = "regions";
    public static final String MESSAGE_TRIGGER_OCCURRENCES_KEY =
        "__leanplum_message_trigger_occurrences_%s";
    public static final String MESSAGE_IMPRESSION_OCCURRENCES_KEY =
        "__leanplum_message_occurrences_%s";
    public static final String MESSAGE_MUTED_KEY = "__leanplum_message_muted_%s";
    public static final String LOCAL_NOTIFICATION_KEY = "__leanplum_local_message_%s";
    public static final String INBOX_KEY = "__leanplum_newsfeed";
    public static final String LEANPLUM_PUSH = "__leanplum_push__";
    public static final String APP_ID = "__app_id";
    public static final String PROPERTY_REGISTRATION_ID = "registration_id";
    public static final String PROPERTY_SENDER_IDS = "sender_ids";
    public static final String NOTIFICATION_CHANNELS_KEY = "__leanplum_notification_channels";
    public static final String DEFAULT_NOTIFICATION_CHANNEL_KEY = "__leanplum_default_notification_channels";
    public static final String NOTIFICATION_GROUPS_KEY = "__leanplum_notification_groups";
  }

  public static class Methods {
    public static final String ADVANCE = "advance";
    public static final String DELETE_INBOX_MESSAGE = "deleteNewsfeedMessage";
    public static final String DOWNLOAD_FILE = "downloadFile";
    public static final String GET_INBOX_MESSAGES = "getNewsfeedMessages";
    public static final String GET_VARS = "getVars";
    public static final String HEARTBEAT = "heartbeat";
    public static final String LOG = "log";
    public static final String MARK_INBOX_MESSAGE_AS_READ = "markNewsfeedMessageAsRead";
    public static final String MULTI = "multi";
    public static final String PAUSE_SESSION = "pauseSession";
    public static final String PAUSE_STATE = "pauseState";
    public static final String REGISTER_FOR_DEVELOPMENT = "registerDevice";
    public static final String RESUME_SESSION = "resumeSession";
    public static final String RESUME_STATE = "resumeState";
    public static final String SET_DEVICE_ATTRIBUTES = "setDeviceAttributes";
    public static final String SET_TRAFFIC_SOURCE_INFO = "setTrafficSourceInfo";
    public static final String SET_USER_ATTRIBUTES = "setUserAttributes";
    public static final String SET_VARS = "setVars";
    public static final String START = "start";
    public static final String STOP = "stop";
    public static final String TRACK = "track";
    public static final String UPLOAD_FILE = "uploadFile";
  }

  public static class Params {
    public static final String ACTION = "action";
    public static final String ACTION_DEFINITIONS = "actionDefinitions";
    public static final String APP_ID = "appId";
    public static final String BACKGROUND = "background";
    public static final String CLIENT = "client";
    public static final String CLIENT_KEY = "clientKey";
    public static final String DATA = "data";
    public static final String DEV_MODE = "devMode";
    public static final String DEVICE_ID = "deviceId";
    public static final String DEVICE_MODEL = "deviceModel";
    public static final String DEVICE_NAME = "deviceName";
    public static final String DEVICE_PUSH_TOKEN = "gcmRegistrationId";
    public static final String DEVICE_SYSTEM_NAME = "systemName";
    public static final String DEVICE_SYSTEM_VERSION = "systemVersion";
    public static final String EMAIL = "email";
    public static final String EVENT = "event";
    public static final String FILE = "file";
    public static final String FILE_ATTRIBUTES = "fileAttributes";
    public static final String GOOGLE_PLAY_PURCHASE_DATA = "googlePlayPurchaseData";
    public static final String GOOGLE_PLAY_PURCHASE_DATA_SIGNATURE =
        "googlePlayPurchaseDataSignature";
    public static final String IAP_CURRENCY_CODE = "currencyCode";
    public static final String IAP_ITEM = "item";
    public static final String INCLUDE_DEFAULTS = "includeDefaults";
    public static final String INCLUDE_MESSAGE_ID = "includeMessageId";
    public static final String INFO = "info";
    public static final String INSTALL_DATE = "installDate";
    public static final String KINDS = "kinds";
    public static final String LIMIT_TRACKING = "limitTracking";
    public static final String MESSAGE = "message";
    public static final String MESSAGE_ID = "messageId";
    public static final String NEW_USER_ID = "newUserId";
    public static final String INBOX_MESSAGE_ID = "newsfeedMessageId";
    public static final String INBOX_MESSAGES = "newsfeedMessages";
    public static final String PARAMS = "params";
    public static final String SDK_VERSION = "sdkVersion";
    public static final String STATE = "state";
    public static final String TIME = "time";
    public static final String TYPE = "type";
    public static final String TOKEN = "token";
    public static final String TRAFFIC_SOURCE = "trafficSource";
    public static final String UPDATE_DATE = "updateDate";
    public static final String USER_ID = "userId";
    public static final String USER_ATTRIBUTES = "userAttributes";
    public static final String VALUE = "value";
    public static final String VARS = "vars";
    public static final String VERSION_NAME = "versionName";
  }

  public static class Keys {
    public static final String CITY = "city";
    public static final String COUNTRY = "country";
    public static final String DELIVERY_TIMESTAMP = "deliveryTimestamp";
    public static final String EXPIRATION_TIMESTAMP = "expirationTimestamp";
    public static final String FILENAME = "filename";
    public static final String HASH = "hash";
    public static final String INSTALL_TIME_INITIALIZED = "installTimeInitialized";
    public static final String IS_READ = "isRead";
    public static final String IS_REGISTERED = "isRegistered";
    public static final String IS_REGISTERED_FROM_OTHER_APP = "isRegisteredFromOtherApp";
    public static final String LATEST_VERSION = "latestVersion";
    public static final String LOCALE = "locale";
    public static final String LOCATION = "location";
    public static final String LOCATION_ACCURACY_TYPE = "locationAccuracyType";
    public static final String MESSAGE_DATA = "messageData";
    public static final String MESSAGES = "messages";
    public static final String UPDATE_RULES = "interfaceRules";
    public static final String EVENT_RULES = "interfaceEvents";
    public static final String INBOX_MESSAGES = "newsfeedMessages";
    public static final String PUSH_MESSAGE_ACTION = "_lpx";
    public static final String PUSH_MESSAGE_ID_NO_MUTE_WITH_ACTION = "_lpm";
    public static final String PUSH_MESSAGE_ID_MUTE_WITH_ACTION = "_lpu";
    public static final String PUSH_MESSAGE_ID_NO_MUTE = "_lpn";
    public static final String PUSH_MESSAGE_ID_MUTE = "_lpv";
    public static final String PUSH_MESSAGE_ID = "lp_messageId";
    public static final String PUSH_MESSAGE_TEXT = "lp_message";
    public static final String PUSH_MESSAGE_IMAGE_URL = "lp_imageUrl";
    public static final String REGION = "region";
    public static final String REGION_STATE = "regionState";
    public static final String REGIONS = "regions";
    public static final String SIZE = "size";
    public static final String SUBTITLE = "Subtitle";
    public static final String SYNC_INBOX = "syncNewsfeed";
    public static final String LOGGING_ENABLED = "loggingEnabled";
    public static final String TIMEZONE = "timezone";
    public static final String TIMEZONE_OFFSET_SECONDS = "timezoneOffsetSeconds";
    public static final String TITLE = "Title";
    public static final String INBOX_IMAGE = "Image";
    public static final String DATA = "Data";
    public static final String TOKEN = "token";
    public static final String VARIANTS = "variants";
    public static final String VARS = "vars";
    public static final String VARS_FROM_CODE = "varsFromCode";
    public static final String NOTIFICATION_CHANNELS = "notificationChannels";
    public static final String DEFAULT_NOTIFICATION_CHANNEL = "defaultNotificationChannel";
    public static final String NOTIFICATION_GROUPS = "notificationChannelGroups";
  }

  public static class Kinds {
    public static final String INT = "integer";
    public static final String FLOAT = "float";
    public static final String STRING = "string";
    public static final String BOOLEAN = "bool";
    public static final String FILE = "file";
    public static final String DICTIONARY = "group";
    public static final String ARRAY = "list";
    public static final String ACTION = "action";
    public static final String COLOR = "color";
  }

  static class Files {
    public static final int MAX_UPLOAD_BATCH_SIZES = (25 * (1 << 20));
    public static final int MAX_UPLOAD_BATCH_FILES = 16;
  }

  public static final String HELD_BACK_EVENT_NAME = "Held Back";
  public static final String HELD_BACK_MESSAGE_PREFIX = "__held_back__";

  public static class Values {
    public static final String DETECT = "(detect)";
    public static final String RESOURCES_VARIABLE = "__Android Resources";
    public static final String ACTION_ARG = "__name__";
    public static final String CHAIN_MESSAGE_ARG = "Chained message";
    public static final String DEFAULT_PUSH_ACTION = "Open action";
    public static final String CHAIN_MESSAGE_ACTION_NAME = "Chain to Existing Message";
    public static final String DEFAULT_PUSH_MESSAGE = "Push message goes here.";
    public static final String SDK_LOG = "sdkLog";
    public static final String SDK_ERROR = "sdkError";
    public static final String FILE_PREFIX = "__file__";
  }

  public static class Crypt {
    public static final int ITER_COUNT = 1000;
    public static final int KEY_LENGTH = 256;
    public static final String SALT = "L3@nP1Vm"; // Must have 8 bytes
    public static final String IV = "__l3anplum__iv__"; // Must have 16 bytes
  }

  public static class Messaging {
    public static final int MAX_STORED_OCCURRENCES_PER_MESSAGE = 100;
    public static final int DEFAULT_PRIORITY = 1000;
  }

  public static class ClassUtil {
    public static final String UI_INTERFACE_EDITOR = "com.leanplum.uieditor.LeanplumUIEditor";
  }
}
