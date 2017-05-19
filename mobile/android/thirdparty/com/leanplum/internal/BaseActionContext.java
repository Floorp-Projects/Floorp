/*
 * Copyright 2016, Leanplum, Inc. All rights reserved.
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

import java.util.Map;

/**
 * Base class for ActionContext that contains internal methods.
 *
 * @author Andrew First
 */
public abstract class BaseActionContext {
  protected String messageId = null;
  protected String originalMessageId = null;
  protected int priority;
  protected Map<String, Object> args;
  protected boolean isRooted = true;
  private boolean isPreview = false;

  public BaseActionContext(String messageId, String originalMessageId) {
    this.messageId = messageId;
    this.originalMessageId = originalMessageId;
  }

  void setIsRooted(boolean value) {
    isRooted = value;
  }

  void setIsPreview(boolean isPreview) {
    this.isPreview = isPreview;
  }

  boolean isPreview() {
    return isPreview;
  }

  public String getMessageId() {
    return messageId;
  }

  public String getOriginalMessageId() {
    return originalMessageId;
  }

  public int getPriority() {
    return priority;
  }

  public Map<String, Object> getArgs() {
    return args;
  }
}
