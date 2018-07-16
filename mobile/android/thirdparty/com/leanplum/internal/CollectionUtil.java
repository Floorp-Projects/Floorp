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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Helper class to easily create new list, map or set objects containing provided parameters.
 *
 * @author Ben Marten
 */
public class CollectionUtil {
  /**
   * Creates a new ArrayList and adds the passed arguments to it.
   *
   * @param items The items to add to the list.
   * @param <T> The type of the list to be created.
   * @return A typed list that contains the passed arguments.
   */
  @SafeVarargs
  public static <T> ArrayList<T> newArrayList(T... items) {
    ArrayList<T> result = new ArrayList<>((items != null) ? items.length : 0);
    if (items != null) {
      Collections.addAll(result, items);
    }
    return result;
  }

  /**
   * Creates a new HashSet and adds the passed arguments to it.
   *
   * @param items The items to add to the set.
   * @param <T> The type of the set to be created.
   * @return A typed set that contains the passed arguments.
   */
  @SafeVarargs
  static <T> HashSet<T> newHashSet(T... items) {
    HashSet<T> result = new HashSet<>((items != null) ? items.length : 0);
    if (items != null) {
      Collections.addAll(result, items);
    }
    return result;
  }

  /**
   * Creates a new HashMap and adds the passed arguments to it in pairs.
   *
   * @param items The keys and values, to add to the map in pairs.
   * @param <T> The type of the map to be created.
   * @return A typed map that contains the passed arguments.
   * @throws IllegalArgumentException Throws an exception when an uneven number of arguments are
   * passed.
   */
  @SuppressWarnings("unchecked")
  public static <T, U> HashMap<T, U> newHashMap(Object... items) {
    return (HashMap<T, U>) newMap(
        new HashMap((items != null) ? items.length : 0),
        (items != null) ? items : null);
  }

  /**
   * Creates a new HashMap and adds the passed arguments to it in pairs.
   *
   * @param items The keys and values, to add to the map in pairs.
   * @param <T> The type of the map to be created.
   * @return A typed map that contains the passed arguments.
   * @throws IllegalArgumentException Throws an exception when an uneven number of arguments are
   * passed.
   */
  @SuppressWarnings("unchecked")
  static <T, U> LinkedHashMap<T, U> newLinkedHashMap(Object... items) {
    return (LinkedHashMap<T, U>) newMap(
        new LinkedHashMap((items != null) ? items.length : 0),
        (items != null) ? items : null);
  }

  /**
   * Creates a new Map and adds the passed arguments to it in pairs.
   *
   * @param items The keys and values, to add to the map in pairs.
   * @param <T> The type of the map to be created.
   * @return A typed map that contains the passed arguments.
   * @throws IllegalArgumentException Throws an exception when an even number of arguments are
   * passed, or the type parameter is not a subclass of map.
   */
  @SuppressWarnings("unchecked")
  private static <T, U> Map<T, U> newMap(Map<T, U> map, T[] items) {
    if (items == null || items.length == 0) {
      return map;
    }
    if (items.length % 2 != 0) {
      throw new IllegalArgumentException("newMap requires an even number of items.");
    }

    for (int i = 0; i < items.length; i += 2) {
      map.put(items[i], (U) items[i + 1]);
    }
    return map;
  }

  /**
   * Returns the components of an array as concatenated String by calling toString() on each item.
   *
   * @param array The array to be concatenated.
   * @param separator The separator between elements.
   * @return A concatenated string of the items in list.
   */
  static <T> String concatenateArray(T[] array, String separator) {
    if (array == null) {
      return null;
    }
    return concatenateList(Arrays.asList(array), separator);
  }

  /**
   * Returns the components of a list as concatenated String by calling toString() on each item.
   *
   * @param list The list to be concatenated.
   * @param separator The separator between elements.
   * @return A concatenated string of the items in list.
   */
  static String concatenateList(List<?> list, String separator) {
    if (list == null) {
      return null;
    }
    if (separator == null) {
      separator = "";
    }
    StringBuilder stringBuilder = new StringBuilder();
    for (Object item : list) {
      if (item != null) {
        stringBuilder.append(item.toString());
        stringBuilder.append(separator);
      }
    }
    String result = stringBuilder.toString();

    if (result.length() > 0) {
      return result.substring(0, result.length() - separator.length());
    } else {
      return result;
    }
  }

  @SuppressWarnings({"unchecked"})
  public static <T> T uncheckedCast(Object obj) {
    return (T) obj;
  }

  /**
   * Gets value from map or default if key isn't found.
   *
   * @param map Map to get value from.
   * @param key Key we are looking for.
   * @param defaultValue Default value if key isn't found.
   * @return Value or default if not found.
   */
  public static <K, V> V getOrDefault(Map<K, V> map, K key, V defaultValue) {
    if (map == null) {
      return defaultValue;
    }
    return map.containsKey(key) ? map.get(key) : defaultValue;
  }

  /**
   * Converts an array of object Longs to primitives.
   *
   * @param array Array to convert.
   * @return Array of long primitives.
   */
  public static long[] toPrimitive(final Long[] array) {
    if (array == null) {
      return null;
    } else if (array.length == 0) {
      return new long[0];
    }
    final long[] result = new long[array.length];
    for (int i = 0; i < array.length; i++) {
      result[i] = array[i].longValue();
    }
    return result;
  }
}
