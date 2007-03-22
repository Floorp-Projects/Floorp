/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Plugin Finder Service.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *  Doron Rosenberg <doronr@us.ibm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

