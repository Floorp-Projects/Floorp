import java.sql.*;

public class VersionCheck
{
  protected Connection getConnection()
  {
    Class.forName("com.mysql.jdbc.Driver");
    return DriverManager.getConnection("jdbc:mysql://localhost/", "", "");
  }

  public class VersionResult
  {
    public String xpiURL;
    public int version;
  }

  public VersionResult checkVersion(String aExtensionGUID, String aInstalledVersion, String aTargetApp, String aTargetAppVersion)
  {
    int extensionVersionParts = getPartCount(aInstalledVersion);
    int extensionVersion = parseVersion(aInstalledVersion, extensionVersionParts);
    int targetAppVersionParts = getPartCount(aTargetAppVersion);
    int targetAppVersion = parseVersion(aTargetAppVersion, targetAppVersionParts);

    Connection c = getConnection();

    Statement s = c.createStatement();

    // We need to find all rows matching aExtensionGUID, and filter like so:
    // 1) version > extensionVersion
    // 2) targetapp == aTargetApp
    // 3) mintargetappversion <= targetAppVersion <= maxtargetappversion
    String sql = "SELECT * FROM extensions WHERE targetapp = '" + aTargetAppVersion + "'AND guid = '" + aExtensionGUID + "'";

    ResultSet rs = s.executeQuery(sql);

    String xpiURL = "";
    int newestExtensionVersion = extensionVersion;

    while (rs.next()) 
    {
      int minTargetAppVersion = parseVersion(rs.getObject("mintargetappversion").toString(), targetAppVersionParts);
      int maxTargetAppVersion = parseVersion(rs.getObject("maxtargetappversion").toString(), targetAppVersionParts);
      
      int version = parseVersion(rs.getObject("version").toString(), extensionVersionParts);
      if (version > extensionVersion && 
          version > newestExtensionVersion &&
          minTargetAppVersion <= targetAppVersion && 
          targetAppVersion < maxTargetAppVersion) 
      {
        newestExtensionVersion = version;
        xpiURL = rs.getObject("xpiurl").toString();
      }
    }
    rs.close();

    VersionResult vr = new VersionResult();
    vr.xpiURL = xpiURL;
    vr.version = newestExtensionVersion;

    return vr;
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
  }

  protected int getPartCount(String aVersionString)
  {
    return aVersionString.split(".").length;
  }
}
