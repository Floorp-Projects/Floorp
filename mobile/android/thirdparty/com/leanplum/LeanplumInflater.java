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

package com.leanplum;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.leanplum.internal.Log;
import com.leanplum.internal.Util;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Inflates layout files that may be overridden by other files.
 *
 * @author Andrew First
 */
public class LeanplumInflater {
  private Context context;
  private LeanplumResources res;

  public static LeanplumInflater from(Context context) {
    return new LeanplumInflater(context);
  }

  private LeanplumInflater(Context context) {
    this.context = context;
  }

  public LeanplumResources getLeanplumResources() {
    return getLeanplumResources(null);
  }

  public LeanplumResources getLeanplumResources(Resources baseResources) {
    if (res != null) {
      return res;
    }
    if (baseResources == null) {
      baseResources = context.getResources();
    }
    if (baseResources instanceof LeanplumResources) {
      return (LeanplumResources) baseResources;
    }
    res = new LeanplumResources(baseResources);
    return res;
  }

  /**
   * Creates a view from the corresponding resource ID.
   */
  public View inflate(int layoutResID) {
    return inflate(layoutResID, null, false);
  }

  /**
   * Creates a view from the corresponding resource ID.
   */
  public View inflate(int layoutResID, ViewGroup root) {
    return inflate(layoutResID, root, root != null);
  }

  /**
   * Creates a view from the corresponding resource ID.
   */
  public View inflate(int layoutResID, ViewGroup root, boolean attachToRoot) {
    Var<String> var;
    try {
      LeanplumResources res = getLeanplumResources(context.getResources());
      var = res.getOverrideResource(layoutResID);
      if (var == null || var.stringValue.equals(var.defaultValue())) {
        return LayoutInflater.from(context).inflate(layoutResID, root, attachToRoot);
      }
      int overrideResId = var.overrideResId();
      if (overrideResId != 0) {
        return LayoutInflater.from(context).inflate(overrideResId, root, attachToRoot);
      }
    } catch (Throwable t) {
      if (!(t instanceof InflateException)) {
        Util.handleException(t);
      }
      return LayoutInflater.from(context).inflate(layoutResID, root, attachToRoot);
    }

    InputStream stream = null;

    try {
      ByteArrayOutputStream fileData = new ByteArrayOutputStream();
      stream = var.stream();
      byte[] buffer = new byte[8192];
      int bytesRead;
      while ((bytesRead = stream.read(buffer)) > -1) {
        fileData.write(buffer, 0, bytesRead);
      }
      Object xmlBlock = Class.forName("android.content.res.XmlBlock").getConstructor(
          byte[].class).newInstance((Object) fileData.toByteArray());
      XmlResourceParser parser = null;
      try {
        parser = (XmlResourceParser) xmlBlock.getClass().getMethod(
            "newParser").invoke(xmlBlock);
        return LayoutInflater.from(context).inflate(parser, root, attachToRoot);
      } catch (Throwable t) {
        throw new RuntimeException(t);
      } finally {
        if (parser != null) {
          parser.close();
        }
      }
    } catch (Throwable t) {
      Log.e("Could not inflate resource " + layoutResID + ":" + var.stringValue(), t);
    } finally {
      if (stream != null) {
        try {
          stream.close();
        } catch (IOException e) {
          Log.e("Failed to close input stream.");
        }
      }
    }
    return LayoutInflater.from(context).inflate(layoutResID, root, attachToRoot);
  }
}
