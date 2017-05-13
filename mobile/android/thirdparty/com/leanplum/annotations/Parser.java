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

package com.leanplum.annotations;

import android.text.TextUtils;

import com.leanplum.Var;
import com.leanplum.callbacks.VariableCallback;
import com.leanplum.internal.Log;

import java.lang.ref.WeakReference;
import java.lang.reflect.Field;
import java.util.List;
import java.util.Map;

/**
 * Parses Leanplum annotations.
 *
 * @author Andrew First
 */
public class Parser {
  private static <T> void defineVariable(
      final Object instance,
      String name,
      T value,
      String kind,
      final Field field) {
    final Var<T> var = Var.define(name, value, kind);
    final boolean hasInstance = instance != null;
    final WeakReference<Object> weakInstance = new WeakReference<>(instance);
    var.addValueChangedHandler(new VariableCallback<T>() {
      @Override
      public void handle(Var<T> variable) {
        Object instance = weakInstance.get();
        if ((hasInstance && instance == null) || field == null) {
          var.removeValueChangedHandler(this);
          return;
        }
        try {
          boolean accessible = field.isAccessible();
          if (!accessible) {
            field.setAccessible(true);
          }
          field.set(instance, var.value());
          if (!accessible) {
            field.setAccessible(false);
          }
        } catch (IllegalArgumentException e) {
          Log.e("Leanplum", "Invalid value " + var.value() +
              " for field " + var.name(), e);
        } catch (IllegalAccessException e) {
          Log.e("Leanplum", "Error setting value for field " + var.name(), e);
        }
      }
    });
  }

  private static void defineFileVariable(
      final Object instance,
      String name,
      String value,
      final Field field) {
    final Var<String> var = Var.defineFile(name, value);
    final boolean hasInstance = instance != null;
    final WeakReference<Object> weakInstance = new WeakReference<>(instance);
    var.addFileReadyHandler(new VariableCallback<String>() {
      @Override
      public void handle(Var<String> variable) {
        Object instance = weakInstance.get();
        if ((hasInstance && instance == null) || field == null) {
          var.removeFileReadyHandler(this);
          return;
        }
        try {
          boolean accessible = field.isAccessible();
          if (!accessible) {
            field.setAccessible(true);
          }
          field.set(instance, var.fileValue());
          if (!accessible) {
            field.setAccessible(false);
          }
        } catch (IllegalArgumentException e) {
          Log.e("Leanplum", "Invalid value " + var.value() +
              " for field " + var.name(), e);
        } catch (IllegalAccessException e) {
          Log.e("Leanplum", "Error setting value for field " + var.name(), e);
        }
      }
    });
  }

  /**
   * Parses Leanplum annotations for all given object instances.
   */
  public static void parseVariables(Object... instances) {
    try {
      for (Object instance : instances) {
        parseVariablesHelper(instance, instance.getClass());
      }
    } catch (Throwable t) {
      Log.e("Leanplum", "Error parsing variables", t);
    }
  }

  /**
   * Parses Leanplum annotations for all given classes.
   */
  public static void parseVariablesForClasses(Class<?>... classes) {
    try {
      for (Class<?> clazz : classes) {
        parseVariablesHelper(null, clazz);
      }
    } catch (Throwable t) {
      Log.e("Leanplum", "Error parsing variables", t);
    }
  }

  private static void parseVariablesHelper(Object instance, Class<?> clazz)
      throws IllegalArgumentException, IllegalAccessException {
    Field[] fields = clazz.getFields();

    for (final Field field : fields) {
      String group, name;
      boolean isFile = false;
      if (field.isAnnotationPresent(Variable.class)) {
        Variable annotation = field.getAnnotation(Variable.class);
        group = annotation.group();
        name = annotation.name();
      } else if (field.isAnnotationPresent(File.class)) {
        File annotation = field.getAnnotation(File.class);
        group = annotation.group();
        name = annotation.name();
        isFile = true;
      } else {
        continue;
      }

      String variableName = name;
      if (TextUtils.isEmpty(variableName)) {
        variableName = field.getName();
      }
      if (!TextUtils.isEmpty(group)) {
        variableName = group + "." + variableName;
      }

      Class<?> fieldType = field.getType();
      String fieldTypeString = fieldType.toString();
      if (fieldTypeString.equals("int")) {
        defineVariable(instance, variableName, field.getInt(instance), "integer", field);
      } else if (fieldTypeString.equals("byte")) {
        defineVariable(instance, variableName, field.getByte(instance), "integer", field);
      } else if (fieldTypeString.equals("short")) {
        defineVariable(instance, variableName, field.getShort(instance), "integer", field);
      } else if (fieldTypeString.equals("long")) {
        defineVariable(instance, variableName, field.getLong(instance), "integer", field);
      } else if (fieldTypeString.equals("char")) {
        defineVariable(instance, variableName, field.getChar(instance), "integer", field);
      } else if (fieldTypeString.equals("float")) {
        defineVariable(instance, variableName, field.getFloat(instance), "float", field);
      } else if (fieldTypeString.equals("double")) {
        defineVariable(instance, variableName, field.getDouble(instance), "float", field);
      } else if (fieldTypeString.equals("boolean")) {
        defineVariable(instance, variableName, field.getBoolean(instance), "bool", field);
      } else if (fieldType.isPrimitive()) {
        Log.e("Variable " + variableName + " is an unsupported primitive type.");
      } else if (fieldType.isArray()) {
        Log.e("Variable " + variableName + " should be a List instead of an Array.");
      } else if (fieldType.isAssignableFrom(List.class)) {
        defineVariable(instance, variableName, field.get(instance), "list", field);
      } else if (fieldType.isAssignableFrom(Map.class)) {
        defineVariable(instance, variableName, field.get(instance), "group", field);
      } else {
        Object value = field.get(instance);
        String stringValue = value == null ? null : value.toString();
        if (isFile) {
          defineFileVariable(instance, variableName, stringValue, field);
        } else {
          defineVariable(instance, variableName, stringValue, "string", field);
        }
      }
    }
  }
}
