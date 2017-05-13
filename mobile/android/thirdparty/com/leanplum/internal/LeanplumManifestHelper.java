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

package com.leanplum.internal;

import android.content.Context;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import com.leanplum.Leanplum;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/**
 * LeanplumManifestHelper class to work with AndroidManifest components.
 *
 * @author Anna Orlova
 */
public class LeanplumManifestHelper {
  private static final String MANIFEST = "manifest";
  private static final String APPLICATION = "application";
  private static final String SERVICE = "service";
  private static final String ACTIVITY = "activity";
  private static final String ACTIVITY_ALIAS = "activity-alias";
  private static final String RECEIVER = "receiver";
  private static final String PROVIDER = "provider";
  private static final String ANDROID_NAME = "android:name";
  private static final String ANDROID_SCHEME = "android:scheme";
  private static final String ACTION = "action";
  private static final String CATEGORY = "category";
  private static final String DATA = "data";
  private static final String ANDROID_HOST = "android:host";
  private static final String ANDROID_PORT = "android:port";
  private static final String ANDROID_PATH = "android:path";
  private static final String ANDROID_PATH_PATTERN = "android:pathPattern";
  private static final String ANDROID_PATH_PREFIX = "android:pathPrefix";
  private static final String ANDROID_MIME_TYPE = "android:mimeType";
  private static final String ANDROID_TYPE = "android:type";
  private static final String INTENT_FILTER = "intent-filter";
  private static final String ANDROID_PERMISSION = "android:permission";
  private static final String ANDROID_MANIFEST = "AndroidManifest.xml";
  private static final String VERSION_NAME = "versionName";

  private static ManifestData manifestData;

  /**
   * Gets application components from AndroidManifest.xml file.
   */
  private static void parseManifestNodeChildren() {
    manifestData = new ManifestData();
    byte[] manifestXml = getByteArrayOfManifest();
    Document manifestDocument = getManifestDocument(manifestXml);
    parseManifestDocument(manifestDocument);
  }

  /**
   * Parse AndroidManifest.xml file to byte array.
   *
   * @return byte[] Byte array of AndroidManifest.xml file
   */
  private static byte[] getByteArrayOfManifest() {
    Context context = Leanplum.getContext();
    if (context == null) {
      Log.e("Context is null. Cannot parse " + ANDROID_MANIFEST + " file.");
      return null;
    }
    byte[] manifestXml = null;
    try {
      JarFile jarFile = new JarFile(context.getPackageResourcePath());
      ZipEntry entry = jarFile.getEntry(ANDROID_MANIFEST);
      manifestXml = new byte[(int) entry.getSize()];
      DataInputStream dataInputStream = new DataInputStream(jarFile.getInputStream(entry));
      dataInputStream.readFully(manifestXml);
      dataInputStream.close();
    } catch (Exception e) {
      Log.e("Cannot parse " + ANDROID_MANIFEST + " file: " + e.getMessage());
    }
    return manifestXml;
  }

  /**
   * Gets Document {@link Document} of AndroidManifest.xml file.
   *
   * @param manifestXml Byte array of AndroidManifest.xml file data.
   * @return Document Document with date of AndroidManifest.xml file.
   */
  private static Document getManifestDocument(byte[] manifestXml) {
    Document document = null;
    try {
      DocumentBuilderFactory builderFactory = DocumentBuilderFactory.newInstance();
      DocumentBuilder builder = builderFactory.newDocumentBuilder();
      document = builder.parse(new ByteArrayInputStream(
          LeanplumManifestParser.decompressXml(manifestXml).getBytes("UTF-8")));
    } catch (Exception e) {
      Log.e("Cannot parse " + ANDROID_MANIFEST + " file: " + e.getMessage());
    }
    return document;
  }

  /**
   * Parse data from Document {@link Document} with date of AndroidManifest.xml file.
   *
   * @param document Document {@link Document} with date of AndroidManifest.xml file.
   */
  private static void parseManifestDocument(Document document) {
    if (document == null) {
      return;
    }
    parseManifestNode(document.getElementsByTagName(MANIFEST).item(0));
  }

  private static void parseManifestNode(Node manifestNode) {
    if (manifestNode == null) {
      return;
    }
    manifestData.appVersionName = getAttribute(manifestNode.getAttributes(), VERSION_NAME);
    NodeList manifestChildren = manifestNode.getChildNodes();
    if (manifestChildren == null) {
      return;
    }
    for (int i = 0; i < manifestChildren.getLength(); i++) {
      Node currentNode = manifestChildren.item(i);
      if (currentNode == null) {
        continue;
      }
      String currentNodeName = currentNode.getNodeName();
      if (APPLICATION.equals(currentNodeName)) {
        parseChildNodeList(currentNode.getChildNodes());
      }
    }
  }

  private static void parseChildNodeList(NodeList childrenList) {
    if (childrenList == null) {
      return;
    }
    for (int j = 0; j < childrenList.getLength(); j++) {
      parseChildNode(childrenList.item(j));
    }
  }

  private static void parseChildNode(Node child) {
    if (child == null) {
      return;
    }
    String childName = child.getNodeName();
    if (childName == null) {
      return;
    }
    switch (childName) {
      case SERVICE:
        manifestData.services.add(parseManifestComponent(child,
            ManifestComponent.ApplicationComponent.SERVICE));
        break;
      case RECEIVER:
        manifestData.receivers.add(parseManifestComponent(child,
            ManifestComponent.ApplicationComponent.RECEIVER));
        break;
      case ACTIVITY:
      case ACTIVITY_ALIAS:
        manifestData.activities.add(parseManifestComponent(child,
            ManifestComponent.ApplicationComponent.ACTIVITY));
        break;
      case PROVIDER:
        manifestData.providers.add(parseManifestComponent(child,
            ManifestComponent.ApplicationComponent.PROVIDER));
        break;
      default:
        break;
    }
  }

  /**
   * Parse AndroidManifest.xml components from XML.
   *
   * @param node XML node to parse.
   * @param type Type of application component {@link ManifestComponent.ApplicationComponent}.
   * @return Return ManifestComponent {@link ManifestComponent} with information from manifest.
   */
  private static ManifestComponent parseManifestComponent(Node node,
      ManifestComponent.ApplicationComponent type) {
    ManifestComponent manifestComponent = new ManifestComponent(type);
    NamedNodeMap attributes = node.getAttributes();
    manifestComponent.name = getAttribute(attributes, ANDROID_NAME);
    manifestComponent.permission = getAttribute(attributes, ANDROID_PERMISSION);
    List<ManifestIntentFilter> intentFilters = new ArrayList<>();
    NodeList childrenList = node.getChildNodes();
    for (int i = 0; i < childrenList.getLength(); i++) {
      Node child = childrenList.item(i);
      String childName = child.getNodeName();
      if (INTENT_FILTER.equals(childName)) {
        ManifestIntentFilter intentFilter = parseManifestIntentFilter(child);
        if (intentFilter != null) {
          intentFilters.add(intentFilter);
        }
      }
    }
    manifestComponent.intentFilters = intentFilters;
    return manifestComponent;
  }

  /**
   * Parse intent filter from XML node.
   *
   * @param intentNode XML node to parse.
   * @return Return ManifestIntentFilter {@link ManifestIntentFilter} with information from
   * manifest.
   */
  private static ManifestIntentFilter parseManifestIntentFilter(Node intentNode) {
    if (intentNode == null) {
      return null;
    }

    NodeList intentChildren = intentNode.getChildNodes();
    if (intentChildren == null) {
      return null;
    }

    ManifestIntentFilter intentFilter = new ManifestIntentFilter();
    intentFilter.attributes = intentNode.getAttributes();
    for (int j = 0; j < intentChildren.getLength(); j++) {
      Node intentChild = intentChildren.item(j);
      String intentChildName = intentChild.getNodeName();
      NamedNodeMap intentChildAttributes = intentChild.getAttributes();
      switch (intentChildName) {
        case ACTION:
          intentFilter.actions.add(getAttribute(intentChildAttributes, ANDROID_NAME));
          break;
        case CATEGORY:
          intentFilter.categories.add(getAttribute(intentChildAttributes, ANDROID_NAME));
          break;
        case DATA:
          String scheme = getAttribute(intentChildAttributes, ANDROID_SCHEME);
          String host = getAttribute(intentChildAttributes, ANDROID_HOST);
          String port = getAttribute(intentChildAttributes, ANDROID_PORT);
          String path = getAttribute(intentChildAttributes, ANDROID_PATH);
          String pathPattern = getAttribute(intentChildAttributes, ANDROID_PATH_PATTERN);
          String pathPrefix = getAttribute(intentChildAttributes, ANDROID_PATH_PREFIX);
          String mimeType = getAttribute(intentChildAttributes, ANDROID_MIME_TYPE);
          String type = getAttribute(intentChildAttributes, ANDROID_TYPE);
          intentFilter.dataList.add(new ManifestIntentFilter.IntentData(scheme, host, port, path,
              pathPattern, pathPrefix, mimeType, type));
          break;
        default:
          break;
      }
    }
    return intentFilter;
  }

  /**
   * @return Return String with attribute value or null if attribute with this name not found.
   */
  private static String getAttribute(NamedNodeMap namedNodeMap, String name) {
    Node node = namedNodeMap.getNamedItem(name);
    if (node == null) {
      if (name.startsWith("android:")) {
        name = name.substring("android:".length());
      }
      node = namedNodeMap.getNamedItem(name);
      if (node == null) {
        return null;
      }
    }
    return node.getNodeValue();
  }

  /**
   * @return Return List of services from manifest.
   */
  public static List<ManifestComponent> getServices() {
    if (manifestData == null) {
      parseManifestNodeChildren();
    }
    return manifestData.services;
  }

  /**
   * @return Return List of activities from manifest.
   */
  static List<ManifestComponent> getActivities() {
    if (manifestData == null) {
      parseManifestNodeChildren();
    }
    return manifestData.activities;
  }

  /**
   * @return Return List of providers from manifest.
   */
  static List<ManifestComponent> getProviders() {
    if (manifestData == null) {
      parseManifestNodeChildren();
    }
    return manifestData.providers;
  }

  /**
   * @return Return List of receivers from manifest.
   */
  public static List<ManifestComponent> getReceivers() {
    if (manifestData == null) {
      parseManifestNodeChildren();
    }
    return manifestData.receivers;
  }

  /**
   * @return String String of application version name.
   */
  public static String getAppVersionName() {
    if (manifestData == null) {
      parseManifestNodeChildren();
    }
    return manifestData.appVersionName;
  }

  /**
   * Verifies that a certain component (receiver or service) is implemented in the
   * AndroidManifest.xml file or the application, in order to make sure that push notifications
   * work.
   *
   * @param componentsList List of application components(services or receivers).
   * @param name The name of the class.
   * @param exported What the exported option should be.
   * @param permission Whether we need any permission.
   * @param actions What actions we need to check for in the intent-filter.
   * @param packageName The package name for the category tag, if we require one.
   * @return true if the respective component is in the manifest file, and false otherwise.
   */
  public static boolean checkComponent(List<ManifestComponent> componentsList,
      String name, boolean exported, String permission, List<String> actions, String packageName) {
    boolean hasComponent = hasComponent(componentsList, name, permission, actions);
    if (!hasComponent && !componentsList.isEmpty()) {
      Log.e(getComponentError(componentsList.get(0).type, name, exported, permission, actions,
          packageName));
    }
    return hasComponent;
  }

  /**
   * Check if list of application components contains class instance of class with name className.
   *
   * @param componentsList List of application components(services or receivers).
   * @param className The name of the class.
   * @param permission Whether we need any permission..
   * @param actions What actions we need to check for in the intent-filter.
   * @return boolean True if componentList contains class instance of class with name className.
   */
  private static boolean hasComponent(List<ManifestComponent> componentsList, String className,
      String permission, List<String> actions) {
    for (ManifestComponent component : componentsList) {
      if (isInstance(component, className)) {
        if (hasPermission(component, permission, actions)) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Check if component instance of class with name className.
   *
   * @param component Application component(service or receiver).
   * @param className The name of the class.
   * @return boolean True if component instance of class with name className.
   */
  private static boolean isInstance(ManifestComponent component, String className) {
    try {
      if (component.name.equals(className)) {
        return true;
      } else {
        Class clazz = null;
        try {
          clazz = Class.forName(component.name);
        } catch (Throwable ignored) {
        }
        if (clazz == null) {
          Log.w("Cannot find class with name: " + component.name);
          return false;
        }
        while (clazz != Object.class) {
          clazz = clazz.getSuperclass();
          if (clazz.getName().equals(className)) {
            return true;
          }
        }
      }
      return false;
    } catch (Exception e) {
      Util.handleException(e);
      return false;
    }
  }

  /**
   * Check if application component has permission with provided actions.
   *
   * @param component application component(service or receiver).
   * @param permission Whether we need any permission.
   * @param actions What actions we need to check for in the intent-filter.
   * @return boolean True if component has permission with actions.
   */
  private static boolean hasPermission(ManifestComponent component, String permission,
      List<String> actions) {
    Boolean hasPermissions = TextUtils.equals(component.permission, permission);
    if (hasPermissions && actions != null) {
      HashSet<String> actionsToCheck = new HashSet<>(actions);
      for (ManifestIntentFilter intentFilter : component.intentFilters) {
        actionsToCheck.removeAll(intentFilter.actions);
      }
      if (actionsToCheck.isEmpty()) {
        return true;
      }
    } else if (hasPermissions) {
      return true;
    }
    return false;
  }

  /**
   * Gets string of error message with instruction how to set up AndroidManifest.xml for push
   * notifications.
   *
   * @return String String of error message with instruction how to set up AndroidManifest.xml for
   * push notifications.
   */
  private static String getComponentError(ManifestComponent.ApplicationComponent componentType,
      String name, boolean exported, String permission, List<String> actions, String packageName) {
    StringBuilder errorMessage = new StringBuilder("Push notifications requires you to add the " +
        componentType.name().toLowerCase() + " " + name + " to your AndroidManifest.xml file." +
        "Add this code within the <application> section:\n");
    errorMessage.append("<").append(componentType.name().toLowerCase()).append("\n");
    errorMessage.append("    android:name=\"").append(name).append("\"\n");
    errorMessage.append("    android:exported=\"").append(Boolean.toString(exported)).append("\"");
    if (permission != null) {
      errorMessage.append("\n    android:permission=\"").append(permission).append("\"");
    }
    errorMessage.append(">\n");
    if (actions != null) {
      errorMessage.append("    <intent-filter>\n");
      for (String action : actions) {
        errorMessage.append("        <action android:name=\"").append(action).append("\" />\n");
      }
      if (packageName != null) {
        errorMessage.append("        <category android:name=\"").append(packageName)
            .append("\" />\n");
      }
      errorMessage.append("    </intent-filter>\n");
    }
    errorMessage.append("</").append(componentType.name().toLowerCase()).append(">");
    return errorMessage.toString();
  }

  /**
   * Check if the application has registered for a certain permission.
   *
   * @param permission Requested permission.
   * @param definesPermission Permission need definition or not.
   * @param logError Need print log or not.
   * @return boolean True if application has permission.
   */
  public static boolean checkPermission(String permission, boolean definesPermission,
      boolean logError) {
    Context context = Leanplum.getContext();
    if (context == null) {
      return false;
    }
    if (context.checkCallingOrSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
      String definition;
      if (definesPermission) {
        definition = "<permission android:name=\"" + permission +
            "\" android:protectionLevel=\"signature\" />\n";
      } else {
        definition = "";
      }
      if (logError) {
        Log.e("In order to use push notifications, you need to enable " +
            "the " + permission + " permission in your AndroidManifest.xml file. " +
            "Add this within the <manifest> section:\n" +
            definition + "<uses-permission android:name=\"" + permission + "\" />");
      }
      return false;
    }
    return true;
  }

  /**
   * Class with Android manifest data.
   */
  private static class ManifestData {
    private List<ManifestComponent> services = new ArrayList<>();
    private List<ManifestComponent> activities = new ArrayList<>();
    private List<ManifestComponent> receivers = new ArrayList<>();
    private List<ManifestComponent> providers = new ArrayList<>();
    private String appVersionName;
  }

  /**
   * Class with application component from AndroidManifest.xml.
   */
  private static class ManifestComponent {
    enum ApplicationComponent {SERVICE, RECEIVER, ACTIVITY, PROVIDER}

    ApplicationComponent type;
    String name;
    String permission;
    List<ManifestIntentFilter> intentFilters = new ArrayList<>();

    ManifestComponent(ApplicationComponent type) {
      this.type = type;
    }
  }

  /**
   * Class for declaration of intent filter from AndroidManifest.
   */
  private static class ManifestIntentFilter {
    final List<String> actions = new ArrayList<>();
    final List<String> categories = new ArrayList<>();
    final List<IntentData> dataList = new ArrayList<>();
    public NamedNodeMap attributes;

    /**
     * Class for data of intent filter from AndroidManifest.
     */
    static class IntentData {
      final String scheme;
      final String host;
      final String port;
      final String path;
      final String pathPattern;
      final String pathPrefix;
      final String mimeType;
      final String type;

      IntentData(String scheme, String host, String port, String path, String pathPattern,
          String pathPrefix, String mimeType, String type) {
        this.scheme = scheme;
        this.host = host;
        this.port = port;
        this.path = path;
        this.pathPattern = pathPattern;
        this.pathPrefix = pathPrefix;
        this.mimeType = mimeType;
        this.type = type;
      }
    }
  }
}
