package org.mozilla.update.extensions;

public class UpdateItem
{
  private int row;
  private java.lang.String id;
  private java.lang.String version;
  private java.lang.String minAppVersion;
  private java.lang.String maxAppVersion;
  private java.lang.String name;
  private java.lang.String updateURL;
  private java.lang.String iconURL;
  private int type;

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

  public java.lang.String getUpdateURL() 
  {
    return updateURL;
  }

  public void setUpdateURL(java.lang.String updateURL) 
  {
    this.updateURL = updateURL;
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

//public class ExtensionType
//{
//  public ExtensionType()
//  {
//  }
//
//  public int row;
//  public String id;
//  public String version;
//  public String name;
//  public String updateURL;
//  public String iconURL;
//  public int type;
//}

