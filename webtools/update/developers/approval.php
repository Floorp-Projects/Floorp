<?php
require"../core/config.php";
require"core/sessionconfig.php";
$function = $_GET["function"];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Manage Approval Queue</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>

<?php
if (!$function or $function=="approvalqueue") {
//Overview page for admins/editors to see all the waiting approval queue items...
?>

<h1>Extensions/Themes Awaiting Approval</h1>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%" class="listing">
<TR>
<TD STYLE="background-color: #FFFFFF; width: 20px"></TD>
<TD>Name</TD>
<TD>Requested by?</TD>
<TD>Last Updated</TD>
<TD>Works with</TD>
</TR>
<?php
$sql ="SELECT TM.ID, `vID`, `Name`, TV.Version,`AppName`, `OSName` FROM `t_main` TM
INNER JOIN `t_version` TV ON TM.ID = TV.ID
INNER JOIN `t_applications` TA ON TV.AppID = TA.AppID
INNER JOIN `t_os` TOS ON TV.OSID = TOS.OSID
WHERE `approved` = '?' ORDER BY TV.DateUpdated ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $sql2 = "SELECT `UserName`,`UserEmail`,`date` FROM `t_approvallog` TA INNER JOIN `t_userprofiles` TU ON TA.UserID = TU.UserID WHERE `ID`='$row[ID]' AND `vID`='$row[vID]' ORDER BY `date` DESC LIMIT 1";
    $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      $row2 = mysql_fetch_array($sql_result2);
        if ($row2[date]) {$date = $row2[date]; } else { $date = $row[DateUpdated]; } 

  echo"<TR>\n";
  echo"<TD>$i</TD>\n";
  echo"<TD><a href=\"?function=approval&id=$row[ID]&vid=$row[vID]\">$row[Name] $row[Version]</a></a></TD>\n";
  echo"<TD>$row2[UserName] - $row2[UserEmail]</TD>\n";
  echo"<TD>$date</TD>\n";
  echo"<TD>$row[AppName]"; if($row[OSName] != "ALL") { echo" ($row[OSName])"; } echo"</TD>\n";
  echo"</TR>\n";

 }

?>

</TABLE>
<?php
} else if ($function=="approval") {
$sql = "SELECT  TM.ID, TM.Type, TM.Name, TM.GUID, TV.Version, TV.URI, TV.Size, TA.AppName, TV.MinAppVer, TV.MaxAppVer FROM  `t_main`  TM 
INNER JOIN t_version TV ON TM.ID = TV.ID
INNER JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE TM.ID = '$_GET[id]' AND TV.vID = '$_GET[vid]'
ORDER  BY  `Type` , `Name`  ASC ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
    $v++;
    $id = $row["ID"];
    $guid = $row["GUID"];
    $type = $row["Type"];
    $name = $row["Name"];
    $version = $row["Version"];
    $uri = $row["URI"];
    $filesize = $row["Size"];
    $appname = $row["AppName"];
    $minappver = $row["MinAppVer"];
    $maxappver = $row["MaxAppVer"];
    $authors = "";

$i=0;
$sql = "SELECT TU.UserName, TU.UserEmail FROM `t_authorxref` TAX INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID WHERE TAX.ID = '$id'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $num_results = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
   $i++;
   $authors .= "$row[UserName]";
   if ($num_results > $i) {$authors .=", ";}
  }
$sql = "SELECT `UserName`, `UserEmail`,`date` FROM `t_approvallog` TA INNER JOIN t_userprofiles TU ON TA.UserID=TU.UserID WHERE `ID`='$id' AND `vID`='$_GET[vid]' AND `action`='Approval?' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
   $requestedby = $row["UserName"];
   $requestedon = $row["date"];
   $requestedbyemail = $row["UserEmail"];
?>

<h1>Mozilla Update - Approval Requested: <?php echo"$name $version"; ?></h1>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: 0px; width: 100%">
<TR><TD>
<FORM NAME="processapproval" METHOD="POST" ACTION="?function=processapproval">
<INPUT NAME="id" TYPE="HIDDEN" VALUE="<?php echo"$_GET[id]"; ?>">
<INPUT NAME="vid" TYPE="HIDDEN" VALUE="<?php echo"$_GET[vid]"; ?>">
<INPUT NAME="name" TYPE="HIDDEN" VALUE="<?php echo"$name"; ?>">
<INPUT NAME="version" TYPE="HIDDEN" VALUE="<?php echo"$version"; ?>">
<INPUT NAME="requestedbyemail" TYPE="HIDDEN" VALUE="<?php echo"$requestedbyemail"; ?>">
<PRE>
  name          <?php echo"$name\n"; ?>
  author(s)     <?php echo"$authors\n"; ?>
  id            <?php echo"$guid\n"; ?>
  version       <?php echo"$version\n"; ?>
  supported apps:
                <?php echo"$appname $minappver-$maxappver\n"; ?>
  requested by: <?php echo"$requestedby\n"; ?>
  requested on: <?php echo"$requestedon\n"; ?>
  
  <A HREF="<?php echo"$uri"; ?>">Install Now</A> (<?php echo"$filesize"; ?>KB)
  <A HREF="">Download</A>
  
  Comments: 
  <TEXTAREA NAME="comments" ROWS=5 COLS=40></TEXTAREA>
  <INPUT NAME="submit" TYPE="SUBMIT" VALUE="Approve"> <INPUT NAME="submit" TYPE="SUBMIT" VALUE="Deny">
</PRE>
</FORM>
<A HREF="?function=approvalqueue">&#171;&#171; Return to Approval Queue</A>
</TD></TR>
</TABLE>

<?php
} else if ($function=="processapproval") {
if ($_POST["submit"]) {
$action = $_POST["submit"];
if ($action=="Approve") {
  $approved = "YES";
  $action = "Approval+";
 } else if ($action=="Deny") {
  $approved = "NO";
  $action = "Approval-";
 }


 echo"<DIV style=\"border: 1px dotted #333; width: 750px; font-size: 14pt; font-weight: bold; text-align:center; margin: auto; margin-bottom: 10px\">";

 
//Firstly, log the comments and action taken..
$_POST["userid"] = $_SESSION["uid"];

$sql = "INSERT INTO `t_approvallog` (`ID`, `vID`, `UserID`, `action`, `date`, `comments`) VALUES ('$_POST[id]', '$_POST[vid]', '$_POST[userid]', '$action', NOW(NULL), '$_POST[comments]');";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

//Secondly, let's move the file to it's new home in /ftp/ for staging...
$sql = "SELECT `Type`,`Name`,`Version`,`URI` from `t_main` TM INNER JOIN t_version TV ON TM.ID=TV.ID WHERE `vID`='$_POST[vid]' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $uri = $row["URI"];
    $filename = str_replace ("http://$sitehostname", "$websitepath", $uri);
 if (file_exists($filename)) {
if ($row["Type"]=="T") {$type="themes";} else if ($row["Type"]=="E") {$type="extensions";}
$name = str_replace(" ","_",$row["Name"]);
$path = strtolower("$type/$name");
//http://mozupdates.psychoticwolf.net/files/approval/chatzilla-0.9.63c-ff.xpi
  $destination = str_replace("/files/approval",strtolower("/files/ftp/$path"),$filename);

$dirpath = "$websitepath/files/ftp/$path";
if (!file_exists($dirpath)) {
mkdir($dirpath,0775);
}
if (!file_exists($destination)) {
rename("$filename", "$destination");
}

$newurl = "http://ftp.mozilla.org/pub/mozilla.org";
$uri = str_replace("$websitepath/files/ftp","$newurl","$destination");
}

//Thirdly, update version record...
$sql = "UPDATE `t_version` SET `URI`='$uri', `approved`='$approved' WHERE `ID`='$_POST[id]' AND `URI`='$row[URI]' LIMIT 5";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);


@include"mail_approval.php";

echo"Processing Request...<br>\n";
echo"Approval Flag set to $action, for $_POST[name] $_POST[version]<br>\n";
echo"E-Mail sent to requestee informing them of the action taken along with your comments<br>\n";
echo"<A HREF=\"?function=approvalqueue\">&#171;&#171; Return to Approval Queue</A>";
echo"</DIV>";

}
?>

<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>