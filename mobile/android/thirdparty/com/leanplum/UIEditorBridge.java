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

import com.leanplum.internal.FileManager;
import com.leanplum.internal.Socket;
import com.leanplum.internal.Util;
import com.leanplum.internal.VarCache;

import java.util.List;
import java.util.Map;

/**
 * Bridge class for the UI editor package to access LP internal methods.
 *
 * @author Ben Marten
 */
public class UIEditorBridge {
  public static void setInterfaceUpdateBlock(CacheUpdateBlock block) {
    VarCache.onInterfaceUpdate(block);
  }

  public static void setEventsUpdateBlock(CacheUpdateBlock block) {
    VarCache.onEventsUpdate(block);
  }

  public static List<Map<String, Object>> getUpdateRuleDiffs() {
    return VarCache.getUpdateRuleDiffs();
  }

  public static List<Map<String, Object>> getEventRuleDiffs() {
    return VarCache.getEventRuleDiffs();
  }

  public static boolean isSocketConnected() {
    return Socket.getInstance() != null && Socket.getInstance().isConnected();
  }

  public static <T> void socketSendEvent(String eventName, Map<String, T> data) {
    if (Socket.getInstance() != null && eventName != null) {
      Socket.getInstance().sendEvent(eventName, data);
    }
  }

  public static String fileRelativeToDocuments(String path) {
    return FileManager.fileRelativeToDocuments(path);
  }

  public static void handleException(Throwable t) {
    Util.handleException(t);
  }
}
