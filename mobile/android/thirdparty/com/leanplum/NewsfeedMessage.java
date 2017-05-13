/*
 * Copyright 2015, Leanplum, Inc. All rights reserved.
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

import com.leanplum.internal.Constants;
import com.leanplum.internal.Request;
import com.leanplum.internal.Util;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

/**
 * NewsfeedMessage class.
 *
 * @author Aleksandar Gyorev
 */
public abstract class NewsfeedMessage {
  private String messageId;
  private Long deliveryTimestamp;
  private Long expirationTimestamp;
  private boolean isRead;
  private ActionContext context;

  NewsfeedMessage(String messageId, Long deliveryTimestamp, Long expirationTimestamp,
      boolean isRead, ActionContext context) {
    this.messageId = messageId;
    this.deliveryTimestamp = deliveryTimestamp;
    this.expirationTimestamp = expirationTimestamp;
    this.isRead = isRead;
    this.context = context;
  }

  Map<String, Object> toJsonMap() {
    Map<String, Object> map = new HashMap<>();
    map.put(Constants.Keys.DELIVERY_TIMESTAMP, this.deliveryTimestamp);
    map.put(Constants.Keys.EXPIRATION_TIMESTAMP, this.expirationTimestamp);
    map.put(Constants.Keys.MESSAGE_DATA, this.actionArgs());
    map.put(Constants.Keys.IS_READ, this.isRead());
    return map;
  }

  Map<String, Object> actionArgs() {
    return context.getArgs();
  }

  void setIsRead(boolean isRead) {
    this.isRead = isRead;
  }

  boolean isActive() {
    if (expirationTimestamp == null) {
      return true;
    }

    Date now = new Date();
    return now.before(new Date(expirationTimestamp));
  }

  static boolean isValidMessageId(String messageId) {
    return messageId.split("##").length == 2;
  }

  ActionContext getContext() {
    return context;
  }

  /**
   * Returns the message identifier of the newsfeed message.
   *
   * @deprecated As of release 1.3.0, replaced by {@link #getMessageId()}
   */
  @Deprecated
  public String messageId() {
    return getMessageId();
  }

  /**
   * Returns the message identifier of the newsfeed message.
   */
  public String getMessageId() {
    return messageId;
  }

  /**
   * Returns the title of the newsfeed message.
   *
   * @deprecated As of release 1.3.0, replaced by {@link #getTitle()}
   */
  @Deprecated
  public String title() {
    return getTitle();
  }

  /**
   * Returns the title of the newsfeed message.
   */
  public String getTitle() {
    return context.stringNamed(Constants.Keys.TITLE);
  }

  /**
   * Returns the subtitle of the newsfeed message.
   *
   * @deprecated As of release 1.3.0, replaced by {@link #getSubtitle()}
   */
  @Deprecated
  public String subtitle() {
    return getSubtitle();
  }

  /**
   * Returns the subtitle of the newsfeed message.
   */
  public String getSubtitle() {
    return context.stringNamed(Constants.Keys.SUBTITLE);
  }

  /**
   * Returns the delivery timestamp of the newsfeed message.
   *
   * @deprecated As of release 1.3.0, replaced by {@link #getDeliveryTimestamp()}
   */
  @Deprecated
  public Date deliveryTimestamp() {
    return getDeliveryTimestamp();
  }

  /**
   * Returns the delivery timestamp of the newsfeed message.
   */
  public Date getDeliveryTimestamp() {
    return new Date(deliveryTimestamp);
  }

  /**
   * Return the expiration timestamp of the newsfeed message.
   *
   * @deprecated As of release 1.3.0, replaced by {@link #getExpirationTimestamp()}
   */
  @Deprecated
  public Date expirationTimestamp() {
    return getExpirationTimestamp();
  }

  /**
   * Return the expiration timestamp of the newsfeed message.
   */
  public Date getExpirationTimestamp() {
    if (expirationTimestamp == null) {
      return null;
    }
    return new Date(expirationTimestamp);
  }

  /**
   * Returns 'true' if the newsfeed message is read.
   */
  public boolean isRead() {
    return isRead;
  }

  /**
   * Read the newsfeed message, marking it as read and invoking its open action.
   */
  public void read() {
    try {
      if (Constants.isNoop()) {
        return;
      }

      if (!this.isRead) {
        setIsRead(true);

        int unreadCount = Newsfeed.getInstance().unreadCount() - 1;
        Newsfeed.getInstance().updateUnreadCount(unreadCount);

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
   * Remove the newsfeed message from the newsfeed.
   */
  public void remove() {
    try {
      Newsfeed.getInstance().removeMessage(messageId);
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }
}
