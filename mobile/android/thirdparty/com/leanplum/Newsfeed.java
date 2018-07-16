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

import com.leanplum.callbacks.InboxChangedCallback;
import com.leanplum.callbacks.NewsfeedChangedCallback;

/**
 * Newsfeed class.
 *
 * @author Aleksandar Gyorev
 */
public class Newsfeed extends LeanplumInbox {
  private static Newsfeed instance = new Newsfeed();
  /**
   * A private constructor, which prevents any other class from instantiating.
   */
  private Newsfeed() {
      super();
  }

  /**
   * Static 'getInstance' method.
   */
  static Newsfeed getInstance() {
    return instance;
  }

  /**
   * Add a callback for when the newsfeed receives new values from the server.
   *
   * @deprecated use {@link #addChangedHandler(InboxChangedCallback)} instead
   */
  @Deprecated
  public void addNewsfeedChangedHandler(NewsfeedChangedCallback handler) {
    super.addChangedHandler(handler);
  }

  /**
   * Removes a newsfeed changed callback.
   *
   * @deprecated use {@link #removeChangedHandler(InboxChangedCallback)} instead
   */
  @Deprecated
  public void removeNewsfeedChangedHandler(NewsfeedChangedCallback handler) {
    super.removeChangedHandler(handler);
  }
}
