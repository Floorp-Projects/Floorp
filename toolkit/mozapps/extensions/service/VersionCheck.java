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
 * The Original Code is the Extension Manager.
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

  protected Connection getConnection()
  {
    Connection c = null;
    try 
    {
      Class.forName("com.mysql.jdbc.Driver");
      c = DriverManager.getConnection("jdbc:mysql://localhost/", "", "");
    }
    catch (Exception e)
    {
    }
    return c;
  }

  public String getProperty(int aRowID, String aProperty)
  {
    String result = null;
    try 
    {
      Connection c = getConnection();
      Statement s = c.createStatement();

      String sql = "SELECT * FROM extensions WHERE id = '" + aRowID + "'";
      ResultSet rs = s.executeQuery(sql);

      result = rs.next() ? rs.getString(aProperty) : null;
    }
    catch (Exception e) 
    {
    }
    return result;
  }

  public int getNewestExtension(String aExtensionGUID, 
                                String aInstalledVersion, 
                                String aTargetApp, 
                                String aTargetAppVersion)
  {
    int id = -1;
    try 
    {
      int extensionVersionParts = getPartCount(aInstalledVersion);
      int targetAppVersionParts = getPartCount(aTargetAppVersion);

      int extensionVersion = parseVersion(aInstalledVersion, extensionVersionParts);
      int targetAppVersion = parseVersion(aTargetAppVersion, targetAppVersionParts);

      Connection c = getConnection();

      Statement s = c.createStatement();

      // We need to find all rows matching aExtensionGUID, and filter like so:
      // 1) version > extensionVersion
      // 2) targetapp == aTargetApp
      // 3) mintargetappversion <= targetAppVersion <= maxtargetappversion
      String sql = "SELECT * FROM extensions WHERE targetapp = '" + aTargetAppVersion + "'AND guid = '" + aExtensionGUID + "'";

      ResultSet rs = s.executeQuery(sql);

      int newestExtensionVersion = extensionVersion;

      while (rs.next()) 
      {
        int minTargetAppVersion = parseVersion(rs.getString("mintargetappversion"), targetAppVersionParts);
        int maxTargetAppVersion = parseVersion(rs.getString("maxtargetappversion"), targetAppVersionParts);
        
        int version = parseVersion(rs.getString("version"), extensionVersionParts);
        if (version > extensionVersion && 
          version > newestExtensionVersion &&
          minTargetAppVersion <= targetAppVersion && 
          targetAppVersion < maxTargetAppVersion) 
        {
          newestExtensionVersion = version;
          id = rs.getInt("id");
        }
      }
      rs.close();
    }
    catch (Exception e) 
    {
    }

    return id;
  }

  protected int parseVersion(String aVersionString, int aPower)
  {
    int version = 0;
    String[] parts = aVersionString.split(".");

    if (aPower == 0)
      aPower = parts.length;
    
    for (int i = 0; i < parts.length; ++i) 
    {
      if (parts[i] != "+") 
      {
        version += Integer.parseInt(parts[i]) * Math.pow(10, aPower - i);
      }
    }
    return version;
  }

  protected int getPartCount(String aVersionString)
  {
    return aVersionString.split(".").length;
  }
}
