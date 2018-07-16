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

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;

import com.leanplum.internal.CollectionUtil;
import com.leanplum.internal.Constants;
import com.leanplum.internal.FileManager;
import com.leanplum.internal.Log;
import com.leanplum.internal.ResourceQualifiers;
import com.leanplum.internal.ResourceQualifiers.Qualifier;
import com.leanplum.internal.Util;
import com.leanplum.internal.VarCache;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

// Description of resources.asrc file (we don't use this right nwo)
// http://ekasiswanto.wordpress.com/2012/09/19/descriptions-of-androids-resources-arsc/
// Suppressing deprecation warnings for Resource methods,
// because the resource syncing feature will likely be refactored/replaced in the future.
@SuppressWarnings("deprecation")
public class LeanplumResources extends Resources {
  public LeanplumResources(Resources base) {
    super(base.getAssets(), base.getDisplayMetrics(), base.getConfiguration());
  }

  /* internal */
  <T> Var<T> getOverrideResource(int id) {
    try {
      String name = getResourceEntryName(id);
      String type = getResourceTypeName(id);
      if (FileManager.resources == null) {
        return null;
      }
      HashMap<String, Object> resourceValues = CollectionUtil.uncheckedCast(FileManager.resources
          .objectForKeyPath());
      Map<String, String> eligibleFolders = new HashMap<>();
      synchronized (VarCache.valuesFromClient) {
        for (String folder : resourceValues.keySet()) {
          if (!folder.toLowerCase().startsWith(type)) {
            continue;
          }
          HashMap<String, Object> files = CollectionUtil.uncheckedCast(resourceValues.get(folder));
          String eligibleFile = null;
          for (String filename : files.keySet()) {
            String currentName = filename.replace("\\.", ".");
            // Get filename without extension.
            int dotPos = currentName.lastIndexOf('.');
            if (dotPos >= 0) {
              currentName = currentName.substring(0, dotPos);
            }

            if (currentName.equals(name)) {
              eligibleFile = filename;
            }
          }
          if (eligibleFile == null) {
            continue;
          }
          eligibleFolders.put(folder, eligibleFile);
        }
      }

      Map<String, ResourceQualifiers> folderQualifiers = new HashMap<>();
      for (String folder : eligibleFolders.keySet()) {
        folderQualifiers.put(folder, ResourceQualifiers.fromFolder(folder));
      }

      // 1. Eliminate qualifiers that contradict the device configuration.
      // See http://developer.android.com/guide/topics/resources/providing-resources.html
      Configuration config = getConfiguration();
      DisplayMetrics display = getDisplayMetrics();
      Set<String> matchedFolders = new HashSet<>();
      for (String folder : eligibleFolders.keySet()) {
        ResourceQualifiers qualifiers = folderQualifiers.get(folder);
        for (Qualifier qualifier : qualifiers.qualifiers.keySet()) {
          if (qualifier.getFilter().isMatch(
              qualifiers.qualifiers.get(qualifier), config, display)) {
            matchedFolders.add(folder);
          }
        }
      }

      // 2. Identify the next qualifier in the table (MCC first, then MNC,
      // then language, and so on.
      for (Qualifier qualifier : ResourceQualifiers.Qualifier.values()) {
        Map<String, Object> betterMatchedFolders = new HashMap<>();
        for (String folder : matchedFolders) {
          ResourceQualifiers folderQualifier = folderQualifiers.get(folder);
          Object qualifierValue = folderQualifier.qualifiers.get(qualifier);
          if (qualifierValue != null) {
            betterMatchedFolders.put(folder, qualifierValue);
          }
        }
        betterMatchedFolders = qualifier.getFilter().bestMatch(
            betterMatchedFolders, config, display);

        // 3. Do any resource directories use this qualifier?
        if (!betterMatchedFolders.isEmpty()) {
          // Yes.
          // 4. Eliminate directories that do not include this qualifier.
          matchedFolders = betterMatchedFolders.keySet();
        }
      }

      // Return result.
      if (!eligibleFolders.isEmpty()) {
        String folder = eligibleFolders.entrySet().iterator().next().getValue();
        String varName = Constants.Values.RESOURCES_VARIABLE + "." + folder
            + "." + eligibleFolders.get(folder);
        return VarCache.getVariable(varName);
      }
    } catch (Exception e) {
      Log.e("Error getting resource", e);
    }
    return null;
  }

  @Override
  public Drawable getDrawable(int id) throws NotFoundException {
    try {
      Var<String> override = getOverrideResource(id);
      if (override != null) {
        int overrideResId = override.overrideResId();
        if (overrideResId != 0) {
          return super.getDrawable(overrideResId);
        }
        if (!override.stringValue.equals(override.defaultValue())) {
          Drawable result = Drawable.createFromStream(override.stream(), override.fileValue());
          if (result != null) {
            return result;
          }
        }
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
    return super.getDrawable(id);
  }
}
