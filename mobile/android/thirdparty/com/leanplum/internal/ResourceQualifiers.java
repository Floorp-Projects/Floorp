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

import android.content.res.Configuration;
import android.os.Build;
import android.util.DisplayMetrics;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

public class ResourceQualifiers {
  public static abstract class QualifierFilter {
    abstract Object getMatch(String str);

    public abstract boolean isMatch(Object value, Configuration config, DisplayMetrics display);

    public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
      return values;
    }
  }

  public enum Qualifier {
    MCC(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("mcc")) {
          return Integer.getInteger(str.substring(3));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.mcc == ((Integer) value);
      }
    }),
    MNC(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("mnc")) {
          return Integer.getInteger(str.substring(3));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.mnc == ((Integer) value);
      }
    }),
    LANGUAGE(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.length() == 2) {
          return str;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        // Suppressing deprecated locale.
        //noinspection deprecation
        return config.locale.getLanguage().equals(value);
      }
    }),
    REGION(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("r") && str.length() == 3) {
          return str.substring(1);
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        // Suppressing deprecated locale.
        //noinspection deprecation
        return config.locale.getCountry().toLowerCase().equals(value);
      }
    }),
    LAYOUT_DIRECTION(new QualifierFilter() {
      // From http://developer.android.com/reference/android/content/res/Configuration.html#SMALLEST_SCREEN_WIDTH_DP_UNDEFINED
      public static final int SCREENLAYOUT_LAYOUTDIR_LTR = 0x00000040;
      public static final int SCREENLAYOUT_LAYOUTDIR_RTL = 0x00000080;
      public static final int SCREENLAYOUT_LAYOUTDIR_MASK = 0x000000c0;

      @Override
      public Object getMatch(String str) {
        if ("ldrtl".equals(str)) {
          return SCREENLAYOUT_LAYOUTDIR_RTL;
        } else if ("ldltr".equals(str)) {
          return SCREENLAYOUT_LAYOUTDIR_LTR;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (config.screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK) == (Integer) value;
      }
    }),
    SMALLEST_WIDTH(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("sw") && str.endsWith("dp")) {
          return Integer.getInteger(str.substring(2, str.length() - 2));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        try {
          Field field = config.getClass().getField("smallestScreenWidthDp");
          int smallestWidthDp = (int) (Integer) field.get(config);
          return smallestWidthDp >= (Integer) value;
        } catch (Throwable t) {
          Util.handleException(t);
        }
        return false;
      }

      @Override
      public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
        Map<String, Object> result = new HashMap<>();
        int max = Integer.MIN_VALUE;
        for (Map.Entry<String, Object> entry : values.entrySet()) {
          Integer intObj = (Integer) entry.getValue();
          if (intObj > max) {
            max = intObj;
            result.clear();
          }
          if (intObj == max) {
            result.put(entry.getKey(), intObj);
          }
        }
        return result;
      }
    }),
    AVAILABLE_WIDTH(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("w") && str.endsWith("dp")) {
          return Integer.getInteger(str.substring(1, str.length() - 2));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        try {
          Field field = config.getClass().getField("screenWidthDp");
          int screenWidthDp = (int) (Integer) field.get(config);
          return screenWidthDp >= (Integer) value;
        } catch (Throwable t) {
          Util.handleException(t);
        }
        return false;
      }

      @Override
      public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
        Map<String, Object> result = new HashMap<>();
        int max = Integer.MIN_VALUE;
        for (Map.Entry<String, Object> entry : values.entrySet()) {
          Integer intObj = (Integer) entry.getValue();
          if (intObj > max) {
            max = intObj;
            result.clear();
          }
          if (intObj == max) {
            result.put(entry.getKey(), intObj);
          }
        }
        return result;
      }
    }),
    AVAILABLE_HEIGHT(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("h") && str.endsWith("dp")) {
          return Integer.getInteger(str.substring(1, str.length() - 2));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        try {
          Field field = config.getClass().getField("screenHeightDp");
          int screenHeightDp = (int) (Integer) field.get(config);
          return screenHeightDp >= (Integer) value;
        } catch (Throwable t) {
          Util.handleException(t);
        }
        return false;
      }

      @Override
      public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
        Map<String, Object> result = new HashMap<>();
        int max = Integer.MIN_VALUE;
        for (Map.Entry<String, Object> entry : values.entrySet()) {
          Integer intObj = (Integer) entry.getValue();
          if (intObj > max) {
            max = intObj;
            result.clear();
          }
          if (intObj == max) {
            result.put(entry.getKey(), intObj);
          }
        }
        return result;
      }
    }),
    SCREEN_SIZE(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("small".equals(str)) {
          return Configuration.SCREENLAYOUT_SIZE_SMALL;
        } else if ("normal".equals(str)) {
          return Configuration.SCREENLAYOUT_SIZE_NORMAL;
        } else if ("large".equals(str)) {
          return Configuration.SCREENLAYOUT_SIZE_LARGE;
        } else if ("xlarge".equals(str)) {
          return Configuration.SCREENLAYOUT_SIZE_XLARGE;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (config.screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) <= (Integer) value;
      }

      @Override
      public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
        Map<String, Object> result = new HashMap<>();
        int max = Integer.MIN_VALUE;
        for (Map.Entry<String, Object> entry : values.entrySet()) {
          Integer intObj = (Integer) entry.getValue();
          if (intObj > max) {
            max = intObj;
            result.clear();
          }
          if (intObj == max) {
            result.put(entry.getKey(), intObj);
          }
        }
        return result;
      }
    }),
    SCREEN_ASPECT(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("long".equals(str)) {
          return Configuration.SCREENLAYOUT_LONG_YES;
        } else if ("notlong".equals(str)) {
          return Configuration.SCREENLAYOUT_LONG_NO;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (config.screenLayout & Configuration.SCREENLAYOUT_LONG_MASK) == (Integer) value;
      }
    }),
    SCREEN_ORIENTATION(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("port".equals(str)) {
          return Configuration.ORIENTATION_PORTRAIT;
        } else if ("land".equals(str)) {
          return Configuration.ORIENTATION_LANDSCAPE;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.orientation == (Integer) value;
      }
    }),
    UI_MODE(new QualifierFilter() {
      public static final int UI_MODE_TYPE_TELEVISION = 0x00000004;
      public static final int UI_MODE_TYPE_APPLIANCE = 0x00000005;

      @Override
      public Object getMatch(String str) {
        if ("car".equals(str)) {
          return Configuration.UI_MODE_TYPE_CAR;
        } else if ("desk".equals(str)) {
          return Configuration.UI_MODE_TYPE_DESK;
        } else if ("television".equals(str)) {
          return UI_MODE_TYPE_TELEVISION;
        } else if ("appliance".equals(str)) {
          return UI_MODE_TYPE_APPLIANCE;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (config.uiMode & Configuration.UI_MODE_TYPE_MASK) == (Integer) value;
      }
    }),
    NIGHT_MODE(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("night".equals(str)) {
          return Configuration.UI_MODE_NIGHT_YES;
        } else if ("notnight".equals(str)) {
          return Configuration.UI_MODE_NIGHT_NO;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (config.uiMode & Configuration.UI_MODE_NIGHT_MASK) == (Integer) value;
      }
    }),
    SCREEN_PIXEL_DENSITY(new QualifierFilter() {
      public static final int DENSITY_TV = 0x000000d5;
      public static final int DENSITY_XXHIGH = 0x000001e0;
      public static final int DENSITY_NONE = 0;

      @Override
      public Object getMatch(String str) {
        if ("ldpi".equals(str)) {
          return DisplayMetrics.DENSITY_LOW;
        } else if ("mdpi".equals(str)) {
          return DisplayMetrics.DENSITY_MEDIUM;
        } else if ("hdpi".equals(str)) {
          return DisplayMetrics.DENSITY_HIGH;
        } else if ("xhdpi".equals(str)) {
          return DisplayMetrics.DENSITY_XHIGH;
        } else if ("nodpi".equals(str)) {
          return DENSITY_NONE;
        } else if ("tvdpi".equals(str)) {
          return DENSITY_TV;
        } else if ("xxhigh".equals(str)) {
          return DENSITY_XXHIGH;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return true;
      }

      @Override
      public Map<String, Object> bestMatch(Map<String, Object> values, Configuration config, DisplayMetrics display) {
        Map<String, Object> result = new HashMap<>();
        int min = Integer.MAX_VALUE;

        for (Map.Entry<String, Object> entry : values.entrySet()) {
          Integer intObj = (Integer) entry.getValue();
          if (intObj < min && intObj >= display.densityDpi) {
            min = intObj;
            result.clear();
          }
          if (intObj == min) {
            result.put(entry.getKey(), intObj);
          }
        }
        if (result.size() == 0) {
          int max = Integer.MIN_VALUE;
          for (String key : values.keySet()) {
            Integer intObj = (Integer) values.get(key);
            if (intObj > max) {
              max = intObj;
              result.clear();
            }
            if (intObj == max) {
              result.put(key, intObj);
            }
          }
        }
        return result;
      }
    }),
    TOUCHSCREEN_TYPE(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("notouch".equals(str)) {
          return Configuration.TOUCHSCREEN_NOTOUCH;
        } else if ("finger".equals(str)) {
          return Configuration.TOUCHSCREEN_FINGER;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.touchscreen == (Integer) value;
      }
    }),
    KEYBOARD_AVAILABILITY(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("keysexposed".equals(str)) {
          return Configuration.KEYBOARDHIDDEN_NO;
        } else if ("keyshidden".equals(str)) {
          return Configuration.KEYBOARDHIDDEN_YES;
        } else if ("keyssoft".equals(str)) {
          return 0;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return (Integer) value == 0 || config.keyboardHidden == (Integer) value;
      }
    }),
    PRIMARY_TEXT_INPUTMETHOD(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("nokeys".equals(str)) {
          return Configuration.KEYBOARD_NOKEYS;
        } else if ("qwerty".equals(str)) {
          return Configuration.KEYBOARD_QWERTY;
        } else if ("12key".equals(str)) {
          return Configuration.KEYBOARD_12KEY;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.keyboard == (Integer) value;
      }
    }),
    NAVIGATION_KEY_AVAILABILITY(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("navexposed".equals(str)) {
          return Configuration.NAVIGATIONHIDDEN_NO;
        } else if ("navhidden".equals(str)) {
          return Configuration.NAVIGATIONHIDDEN_YES;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.navigationHidden == (Integer) value;
      }
    }),
    PRIMARY_NON_TOUCH_NAVIGATION_METHOD(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if ("nonav".equals(str)) {
          return Configuration.NAVIGATION_NONAV;
        } else if ("dpad".equals(str)) {
          return Configuration.NAVIGATION_DPAD;
        } else if ("trackball".equals(str)) {
          return Configuration.NAVIGATION_TRACKBALL;
        } else if ("wheel".equals(str)) {
          return Configuration.NAVIGATION_WHEEL;
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return config.navigation == (Integer) value;
      }
    }),
    PLATFORM_VERSION(new QualifierFilter() {
      @Override
      public Object getMatch(String str) {
        if (str.startsWith("v")) {
          return Integer.getInteger(str.substring(1));
        }
        return null;
      }

      @Override
      public boolean isMatch(Object value, Configuration config, DisplayMetrics display) {
        return Build.VERSION.SDK_INT >= (Integer) value;
      }
    });

    private QualifierFilter filter;

    Qualifier(QualifierFilter filter) {
      this.filter = filter;
    }

    public QualifierFilter getFilter() {
      return filter;
    }
  }

  public Map<Qualifier, Object> qualifiers = new HashMap<>();

  public static ResourceQualifiers fromFolder(String folderName) {
    ResourceQualifiers result = new ResourceQualifiers();
    String[] nameParts = folderName.toLowerCase().split("-");
    int qualifierIndex = 0;
    for (String part : nameParts) {
      boolean isMatch = false;
      while (!isMatch && qualifierIndex < Qualifier.values().length) {
        Qualifier qualifier = Qualifier.values()[qualifierIndex];
        Object match = qualifier.getFilter().getMatch(part);
        if (match != null) {
          result.qualifiers.put(qualifier, match);
          isMatch = true;
        }
        qualifierIndex++;
      }
    }
    return result;
  }
}
