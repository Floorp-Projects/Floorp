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

public class UpdateItem
{
  private java.lang.String id;
  private java.lang.String version;
  private java.lang.String minAppVersion;
  private java.lang.String maxAppVersion;
  private java.lang.String name;
  private int              row;
  private java.lang.String xpiURL;
  private java.lang.String iconURL;
  private int              type;

  public UpdateItem() 
  {
  }

  public int getRow() 
  {
    return row;
  }

  public void setRow(int row) 
  {
    this.row = row;
  }

  public java.lang.String getId() 
  {
    return id;
  }

  public void setId(java.lang.String id) 
  {
    this.id = id;
  }

  public java.lang.String getVersion() 
  {
    return version;
  }

  public void setVersion(java.lang.String version) 
  {
    this.version = version;
  }

  public java.lang.String getMinAppVersion() 
  {
    return minAppVersion;
  }

  public void setMinAppVersion(java.lang.String minAppVersion) 
  {
    this.minAppVersion = minAppVersion;
  }

  public java.lang.String getMaxAppVersion() 
  {
    return maxAppVersion;
  }

  public void setMaxAppVersion(java.lang.String maxAppVersion) 
  {
    this.maxAppVersion = maxAppVersion;
  }

  public java.lang.String getName() 
  {
    return name;
  }

  public void setName(java.lang.String name) 
  {
    this.name = name;
  }

  public java.lang.String getXpiURL() 
  {
    return xpiURL;
  }

  public void setXpiURL(java.lang.String xpiURL) 
  {
    this.xpiURL = xpiURL;
  }
  
  public java.lang.String getIconURL() 
  {
    return iconURL;
  }

  public void setIconURL(java.lang.String iconURL) 
  {
    this.iconURL = iconURL;
  }
  
  public int getType() 
  {
    return type;
  }

  public void setType(int type) 
  {
    this.type = type;
  }
}
