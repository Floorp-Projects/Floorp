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

package com.leanplum;

import android.net.Uri;
import android.text.TextUtils;

import com.leanplum.internal.CollectionUtil;
import com.leanplum.internal.Constants;
import com.leanplum.internal.JsonConverter;
import com.leanplum.internal.Log;
import com.leanplum.internal.Request;
import com.leanplum.internal.Util;

import org.json.JSONObject;

import java.io.File;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

import static com.leanplum.internal.FileManager.DownloadFileResult;
import static com.leanplum.internal.FileManager.fileExistsAtPath;
import static com.leanplum.internal.FileManager.fileValue;
import static com.leanplum.internal.FileManager.maybeDownloadFile;

/**
 * LeanplumInboxMessage class.
 *
 * @author Anna Orlova
 */
public class LeanplumInboxMessage {
  private String messageId;
  private Long deliveryTimestamp;
  private Long expirationTimestamp;
  private boolean isRead;
  private ActionContext context;
  private String imageUrl;
  private String imageFileName;

  private LeanplumInboxMessage(String messageId, Long deliveryTimestamp, Long expirationTimestamp,
      boolean isRead, ActionContext context) {
    this.messageId = messageId;
    this.deliveryTimestamp = deliveryTimestamp;
    this.expirationTimestamp = expirationTimestamp;
    this.isRead = isRead;
    this.context = context;
    imageUrl = context.stringNamed(Constants.Keys.INBOX_IMAGE);
    if (imageUrl != null) {
      try {
        imageFileName = Util.sha256(imageUrl);
      } catch (Exception ignored) {
      }
    }
  }

  /**
   * Returns the image file path of the inbox message. Can be null.
   */
  public String getImageFilePath() {
    String path = fileValue(imageFileName);
    if (fileExistsAtPath(path)) {
      return new File(path).getAbsolutePath();
    }
    if (!LeanplumInbox.getInstance().isInboxImagePrefetchingEnabled()) {
      Log.w("Inbox Message image path is null because you're calling disableImagePrefetching. " +
          "Consider using imageURL method or remove disableImagePrefetching.");
    }
    return null;
  }

  /**
   * Returns the image Uri of the inbox message.
   * You can safely use this with prefetching enabled.
   * It will return the file Uri path instead if the image is in cache.
   */
  public Uri getImageUrl() {
    String path = fileValue(imageFileName);
    if (fileExistsAtPath(path)) {
      return Uri.fromFile(new File(path));
    }
    if (TextUtils.isEmpty(imageUrl)) {
      return null;
    }

    return Uri.parse(imageUrl);
  }

  /**
   * Returns the data of the inbox message. Advanced use only.
   */
  public JSONObject getData() {
    JSONObject object = null;
    try {
      Map<String, ?> mapData =
          CollectionUtil.uncheckedCast(getContext().objectNamed(Constants.Keys.DATA));
      object = JsonConverter.mapToJsonObject(mapData);
    } catch (Throwable t) {
      Log.w("Unable to parse JSONObject for Data field of inbox message.");
    }
    return object;
  }

  /**
   * Returns the message identifier of the newsfeed message.
   */
  public String getMessageId() {
    return messageId;
  }

  /**
   * Returns the title of the inbox message.
   */
  public String getTitle() {
    return context.stringNamed(Constants.Keys.TITLE);
  }

  /**
   * Returns the subtitle of the inbox message.
   */
  public String getSubtitle() {
    return context.stringNamed(Constants.Keys.SUBTITLE);
  }

  /**
   * Returns the delivery timestamp of the inbox message,
   * or null if delivery timestamp is not present.
   */
  public Date getDeliveryTimestamp() {
    if (deliveryTimestamp == null) {
      return null;
    }
    return new Date(deliveryTimestamp);
  }


  /**
   * Return the expiration timestamp of the inbox message.
   */
  public Date getExpirationTimestamp() {
    if (expirationTimestamp == null) {
      return null;
    }
    return new Date(expirationTimestamp);
  }

  /**
   * Returns 'true' if the inbox message is read.
   */
  public boolean isRead() {
    return isRead;
  }

  /**
   * Read the inbox message, marking it as read and invoking its open action.
   */
  public void read() {
    try {
      if (Constants.isNoop()) {
        return;
      }

      if (!this.isRead) {
        setIsRead(true);

        int unreadCount = LeanplumInbox.getInstance().unreadCount() - 1;
        LeanplumInbox.getInstance().updateUnreadCount(unreadCount);

        Map<String, Object> params = new HashMap<>();
        params.put(Constants.Params.INBOX_MESSAGE_ID, messageId);
        Request req = Request.post(Constants.Methods.MARK_INBOX_MESSAGE_AS_READ,
            params);
        req.send();
      }
      this.context.runTrackedActionNamed(Constants.Values.DEFAULT_PUSH_ACTION);
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  /**
   * Remove the inbox message from the inbox.
   */
  public void remove() {
    try {
      LeanplumInbox.getInstance().removeMessage(messageId);
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }

  static LeanplumInboxMessage createFromJsonMap(String messageId, Map<String, Object> map) {
    Map<String, Object> messageData = CollectionUtil.uncheckedCast(map.get(Constants.Keys
        .MESSAGE_DATA));
    Long deliveryTimestamp = CollectionUtil.uncheckedCast(map.get(Constants.Keys
        .DELIVERY_TIMESTAMP));
    Long expirationTimestamp = CollectionUtil.uncheckedCast(map.get(Constants.Keys
        .EXPIRATION_TIMESTAMP));
    Boolean isRead = CollectionUtil.uncheckedCast(map.get(Constants.Keys.IS_READ));
    return constructMessage(messageId, deliveryTimestamp, expirationTimestamp,
        isRead != null ? isRead : false, messageData);
  }

  static LeanplumInboxMessage constructMessage(String messageId, Long deliveryTimestamp,
      Long expirationTimestamp, boolean isRead, Map<String, Object> actionArgs) {
    if (!isValidMessageId(messageId)) {
      Log.e("Malformed inbox messageId: " + messageId);
      return null;
    }

    String[] messageIdParts = messageId.split("##");
    ActionContext context = new ActionContext((String) actionArgs.get(Constants.Values.ACTION_ARG),
        actionArgs, messageIdParts[0]);
    context.preventRealtimeUpdating();
    context.update();
    return new LeanplumInboxMessage(messageId, deliveryTimestamp, expirationTimestamp, isRead,
        context);
  }


  /**
   * Download image if prefetching is enabled.
   * Uses {@link LeanplumInbox#downloadedImageUrls} to make sure we don't call fileExist method
   * multiple times for same URLs.
   *
   * @return Boolean True if the image will be downloaded, otherwise false.
   */
  boolean downloadImageIfPrefetchingEnabled() {
    if (!LeanplumInbox.isInboxImagePrefetchingEnabled) {
      return false;
    }

    if (TextUtils.isEmpty(imageUrl) || LeanplumInbox.downloadedImageUrls.contains(imageUrl)) {
      return false;
    }

    DownloadFileResult result = maybeDownloadFile(true, imageFileName,
        imageUrl, imageUrl, null);
    LeanplumInbox.downloadedImageUrls.add(imageUrl);
    return DownloadFileResult.DOWNLOADING == result;
  }

  Map<String, Object> toJsonMap() {
    Map<String, Object> map = new HashMap<>();
    map.put(Constants.Keys.DELIVERY_TIMESTAMP, this.deliveryTimestamp);
    map.put(Constants.Keys.EXPIRATION_TIMESTAMP, this.expirationTimestamp);
    map.put(Constants.Keys.MESSAGE_DATA, this.actionArgs());
    map.put(Constants.Keys.IS_READ, this.isRead());
    return map;
  }

  boolean isActive() {
    if (expirationTimestamp == null) {
      return true;
    }

    Date now = new Date();
    return now.before(new Date(expirationTimestamp));
  }

  private static boolean isValidMessageId(String messageId) {
    return messageId.split("##").length == 2;
  }

  ActionContext getContext() {
    return context;
  }

  private Map<String, Object> actionArgs() {
    return context.getArgs();
  }

  private void setIsRead(boolean isRead) {
    this.isRead = isRead;
  }
}
