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
 * The Original Code is the Extension Update Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

package org.mozilla.update.extensions;

import java.sql.*;

public class VersionCheck
{
  public VersionCheck()
  {
  }

/*
  public static void main(String[] args)
  {
    VersionCheck impl = new VersionCheck();

    UpdateItem item = new UpdateItem();
    item.setId("{1ffc34af-6d8b-45a8-9765-92887262edfe}");
    item.setVersion("3.0");
    item.setMinAppVersion("0.9");
    item.setMaxAppVersion("0.10");
    item.setName("NewExtension 5");
    item.setRow(-1);
    item.setXpiURL("");
    item.setIconURL("");
    item.setUserCookie("Goats");
    item.setItemCookie("Goats 2");
    UpdateItem newer = impl.getNewestExtension(item, "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}", "0.9");
    if (newer != null) {
      System.out.println("*** result row = " + newer.getRow() + ", xpiUrl = " + newer.getXpiURL());
    }
    else {
      System.out.println("*** NADA NOTHING ZILCH");
    }
  }
*/

  public UpdateItem getNewestExtension(UpdateItem aInstalledItem,
                                       String aTargetApp,
                                       String aTargetAppVersion,
                                       String aUserCookie,
                                       String aSessionCookie)
  {
    return getExtensionUpdates(aInstalledItem, aTargetApp, aTargetAppVersion, true);
  }
    
  public UpdateItem getVersionUpdate(UpdateItem aInstalledItem,
                                     String aTargetApp,
                                     String aTargetAppVersion,
                                     String aUserCookie,
                                     String aSessionCookie)
  {
    return getExtensionUpdates(aInstalledItem, aTargetApp, aTargetAppVersion, false);
  }
    
  protected UpdateItem getExtensionUpdates(UpdateItem aInstalledItem, 
                                           String aTargetApp, 
                                           String aTargetAppVersion,
                                           boolean aNewest)
  {
    UpdateItem remoteItem = new UpdateItem();

    Version installedVersion = new Version(aInstalledItem.getVersion());
    Version targetAppVersion = new Version(aTargetAppVersion);

    Connection c;
    Statement sMain, sVersion, sApps;
    ResultSet rsMain, rsVersion, rsApps;
    try 
    {
      c = getConnection();
      sMain = c.createStatement();
  
      // We need to find all rows matching aInstalledItem.id, and filter like so:
      // 1) version > installedVersion
      // 2) targetapp == aTargetApp
      // 3) mintargetappversion <= targetAppVersion <= maxtargetappversion
      String sqlMain = "SELECT * FROM t_main WHERE GUID = '" + aInstalledItem.getId() + "'";
      boolean temp = sMain.execute(sqlMain);
      rsMain = sMain.getResultSet();
      
      Version newestRemoteVersion = installedVersion;
      while (rsMain.next()) 
      {
        String sqlVersion = "SELECT * FROM t_version WHERE ID = '" + rsMain.getInt("ID") + "'";

        sVersion = c.createStatement();
        temp = sVersion.execute(sqlVersion);
        rsVersion = sVersion.getResultSet();
        
        while (rsVersion.next()) 
        {
          String sqlApps = "SELECT * FROM t_applications WHERE AppID = '" + rsVersion.getInt("AppID") + "' AND GUID = '" + aTargetApp + "'";
          
          sApps = c.createStatement();
          temp = sApps.execute(sqlApps);
          rsApps = sApps.getResultSet();
          
          if (rsApps.next())
          {
            Version minTargetAppVersion = new Version(rsVersion.getString("MinAppVer"));
            Version maxTargetAppVersion = new Version(rsVersion.getString("MaxAppVer"));
            
            Version currentRemoteVersion = new Version(rsVersion.getString("Version"));

            boolean suitable = false;
            if (aNewest) 
            {
              // If we're looking for the _newest_ version only, check to see if it's really newer
              suitable = currentRemoteVersion.compare(installedVersion)     >   0 && 
                         currentRemoteVersion.compare(newestRemoteVersion)  >   0 &&
                          minTargetAppVersion.compare(targetAppVersion)     <=  0 && 
                             targetAppVersion.compare(maxTargetAppVersion)  <=  0;
              if (suitable)
                newestRemoteVersion = currentRemoteVersion;
            }
            else 
            {
              // ... otherwise, if the version exactly matches...
              suitable = currentRemoteVersion.compare(installedVersion)     ==  0 && 
                          minTargetAppVersion.compare(targetAppVersion)     <=  0 &&
                             targetAppVersion.compare(maxTargetAppVersion)  <=  0;
            }

            if (suitable) 
            {
              remoteItem.setRow(rsMain.getInt("ID"));
              remoteItem.setId(rsMain.getString("GUID"));
              remoteItem.setName(rsMain.getString("Name"));
              remoteItem.setVersion(rsVersion.getString("Version"));
              remoteItem.setMinAppVersion(rsVersion.getString("MinAppVer"));
              remoteItem.setMaxAppVersion(rsVersion.getString("MaxAppVer"));
              remoteItem.setXpiURL(rsVersion.getString("URI"));
              remoteItem.setIconURL("");
            }
          }
          rsApps.close();
          sApps.close();
        }
        rsVersion.close();
        sVersion.close();
      }
      rsMain.close();
      sMain.close();
      c.close();
    }
    catch (Exception e)
    {
    }

    return remoteItem;
  }

  protected Connection getConnection() throws Exception
  {
    Class.forName("com.mysql.jdbc.Driver");
    return DriverManager.getConnection("jdbc:mysql://localhost/update", "root", "");
  }
}

