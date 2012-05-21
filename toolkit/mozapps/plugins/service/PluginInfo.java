/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.pfs;

public class PluginInfo {
  private java.lang.String name;
  private int pid;
  private java.lang.String version;
  private java.lang.String iconUrl;
  private java.lang.String XPILocation;
  private boolean installerShowsUI;
  private java.lang.String manualInstallationURL;
  private java.lang.String requestedMimetype;
  private java.lang.String licenseURL;

  public PluginInfo() {
  }

  public java.lang.String getName() {
    return name;
  }

  public void setName(java.lang.String name) {
    this.name = name;
  }

  public int getPid() {
    return pid;
  }

  public void setPid(int pid) {
    this.pid = pid;
  }

  public java.lang.String getVersion() {
    return version;
  }

  public void setVersion(java.lang.String version) {
    this.version = version;
  }

  public java.lang.String getIconUrl() {
    return iconUrl;
  }

  public void setIconUrl(java.lang.String iconUrl) {
    this.iconUrl = iconUrl;
  }

  public java.lang.String getXPILocation() {
    return XPILocation;
  }

  public void setXPILocation(java.lang.String XPILocation) {
    this.XPILocation = XPILocation;
  }

  public boolean isInstallerShowsUI() {
    return installerShowsUI;
  }

  public void setInstallerShowsUI(boolean installerShowsUI) {
    this.installerShowsUI = installerShowsUI;
  }

  public java.lang.String getManualInstallationURL() {
    return manualInstallationURL;
  }

  public void setManualInstallationURL(java.lang.String manualInstallationURL) {
    this.manualInstallationURL = manualInstallationURL;
  }

  public java.lang.String getRequestedMimetype() {
    return requestedMimetype;
  }

  public void setRequestedMimetype(java.lang.String requestedMimetype) {
    this.requestedMimetype = requestedMimetype;
  }

  public java.lang.String getLicenseURL() {
    return licenseURL;
  }

  public void setLicenseURL(java.lang.String licenseURL) {
    this.licenseURL = licenseURL;
  }
}

