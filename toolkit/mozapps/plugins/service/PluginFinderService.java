/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.pfs;

public class PluginFinderService {

  public org.mozilla.pfs.PluginInfo getPluginInfo(java.lang.String aMimetype, java.lang.String aClientOS, java.lang.String aLocale) {
    org.mozilla.pfs.PluginInfo response = new org.mozilla.pfs.PluginInfo();
    
    if (aMimetype.equals("application/x-shockwave-flash")) {
      response.setPid(1);
      response.setName("Flash Player");
      response.setVersion("7");
      response.setIconUrl("http://goat.austin.ibm.com:8080/flash.gif");
      response.setXPILocation("http://www.nexgenmedia.net/flashlinux/flash-linux.xpi");
      response.setInstallerShowsUI(false);
      response.setManualInstallationURL("");
      response.setLicenseURL("");
    } else if (aMimetype.equals("application/x-mtx")) {
      response.setPid(2);
      response.setName("Viewpoint Media Player");
      response.setVersion("5");
      response.setIconUrl(null);
      response.setXPILocation("http://www.nexgenmedia.net/flashlinux/invalid.xpi");
      response.setInstallerShowsUI(false);
      response.setManualInstallationURL("http://www.viewpoint.com/pub/products/vmp.html");   
      response.setLicenseURL("http://www.viewpoint.com/pub/privacy.html");
    } else {
      response.setPid(-1);
    }
    
    response.setRequestedMimetype(aMimetype);
    return response;
    
  }
}

