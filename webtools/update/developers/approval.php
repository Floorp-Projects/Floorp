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
if ($_SESSION["level"]=="admin" or $_SESSION["level"]=="editor") {
    //Do Nothing, they're good. :-)
} else {
    echo"<h1>Access Denied</h1>\n";
    echo"You do not have access to the Approval Queue.";
    include"$page_footer";
    echo"</body></html>\n";
    exit;
}
?>

<?php
if (!$function or $function=="approvalqueue") {
//Overview page for admins/editors to see all the waiting approval queue items...
?>
<?php
if ($_POST["submit"]=="Submit") {
include"inc_approval.php"; //Get the resuable process_approval() function.

echo"<h2>Processing changes to approval queue, please wait...</h2>\n";
//echo"<pre>"; print_r($_POST); echo"</pre>\n";

for ($i=1; $_POST["maxvid"]>=$i; $i++) {
$type = $_POST["type_$i"];
$testos = $_POST["testos_$i"];
$testbuild = $_POST["testbuild_$i"];
$comments = $_POST["comments_$i"];
$approval = $_POST["approval_$i"];
$file = $_POST["file_$i"];

if ($_POST["installation_$i"]) { $installation = $_POST["installation_$i"]; } else { $installation = "NO";}
if ($_POST["uninstallation_$i"]) { $uninstallation = $_POST["uninstallation_$i"]; } else { $uninstallation = "NO";}
if ($_POST["appworks_$i"]) { $appworks = $_POST["appworks_$i"]; } else { $appworks = "NO";}
if ($_POST["cleanprofile_$i"]) { $cleanprofile = $_POST["cleanprofile_$i"]; } else { $cleanprofile = "NO";}

if ($type=="E") {
if ($_POST["newchrome_$i"]) { $newchrome = $_POST["newchrome_$i"]; } else { $newchrome = "NO";}
if ($_POST["worksasdescribed_$i"]) { $worksasdescribed = $_POST["worksasdescribed_$i"]; } else { $worksasdescribed = "NO";}
} else if ($type=="T") {
if ($_POST["visualerrors_$i"]) { $visualerrors = $_POST["visualerrors_$i"]; } else { $visualerrors = "NO";}
if ($_POST["allelementsthemed_$i"]) { $allelementsthemed = $_POST["allelementsthemed_$i"]; } else { $allelementsthemed = "NO";}
}

if ($approval !="noaction") {
//echo"$i - $file $testos $testbuild $comments $approval<br>\n";
//echo"$type - $installation $uninstallation $appworks $cleanprofile $newchrome $worksasdescribed $visualerrors $allelementsthemed<br>\n";

if ($type=="T") {
    if ($approval=="YES") {
        if ($installation=="YES" and $uninstallation=="YES" and $appworks=="YES" and $cleanprofile=="YES" and $visualerrors=="YES" and $allelementsthemed=="YES" and $testos and $testbuild) {
            $approval_result = process_approval($type, $file, "approve");
        }
    } else {
        if ($testos and $testbuild) {
            $approval_result = process_approval($type, $file, "deny");
        }

    }

} else if ($type=="E") {
    if ($approval=="YES") {
        if ($installation=="YES" and $uninstallation=="YES" and $appworks=="YES" and $cleanprofile=="YES" and $newchrome=="YES" and $worksasdescribed=="YES" and $testos and $testbuild) {
            $approval_result = process_approval($type, $file, "approve");
        }
    } else {
        if ($testos and $testbuild) {
            $approval_result = process_approval($type, $file, "deny");    
        }
    }
}

//Approval for this file was successful, print the output message.
$name = $_POST["name_$i"];
$version = $_POST["version_$i"];
if ($approval_result) {
   if ($approval=="YES") {
       echo"$name $version was granted approval<br>\n";
   } else if ($approval=="NO") {
       echo"$name $version was denied approval<br>\n";
   }    

}


}

}

}
?>

<h1>Extensions/Themes Awaiting Approval</h1>
<div style="margin-left: 20px; font-size: 8pt;"><a href="?function=approvalhistory">Approval Log History</a></div>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%;">
<form name="approvalqueue" method="post" action="?">
<?php
$i=0;
$sql ="SELECT TM.ID, `Type`, `vID`, `Name`, `Description`, TV.Version, `OSName`, `URI` FROM `t_main` TM
INNER JOIN `t_version` TV ON TM.ID = TV.ID
INNER JOIN `t_os` TOS ON TV.OSID=TOS.OSID
WHERE `approved` = '?' GROUP BY TV.URI ORDER BY TV.DateUpdated ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $num_results = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
   $i++;
   $type = $row["Type"];
   $uri = $row["URI"];
   $authors = ""; $j="";
   $sql2 = "SELECT `UserName` from `t_authorxref` TAX INNER JOIN `t_userprofiles` TU ON TAX.UserID = TU.UserID WHERE TAX.ID='$row[ID]' ORDER BY `UserName` ASC";
     $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
     while ($row2 = mysql_fetch_array($sql_result2)) { $j++;
      $authors .="$row2[UserName]"; if (mysql_num_rows($sql_result2) > $j) { $authors .=", "; } 
     }
   $categories = ""; $j="";
   $sql2 = "SELECT `CatName` from `t_categoryxref` TCX INNER JOIN `t_categories` TC ON TCX.CategoryID = TC.CategoryID WHERE TCX.ID='$row[ID]' ORDER BY `CatName` ASC";
     $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
     while ($row2 = mysql_fetch_array($sql_result2)) { $j++;
      $categories .="$row2[CatName]"; if (mysql_num_rows($sql_result2) > $j) { $categories .=", "; } 
     }

   $sql2 = "SELECT `UserName`,`UserEmail`,`date` FROM `t_approvallog` TA INNER JOIN `t_userprofiles` TU ON TA.UserID = TU.UserID WHERE `ID`='$row[ID]' AND `vID`='$row[vID]' and `action`='Approval?' ORDER BY `date` DESC LIMIT 1";
    $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      $row2 = mysql_fetch_array($sql_result2);
        if ($row2[date]) {$date = $row2[date]; } else { $date = $row[DateUpdated]; } 
        $date = date("l, F d, Y, g:i:sa", strtotime("$date"));
  echo"<TR><TD COLSPAN=4><h3><a href=\"listmanager.php?function=editversion&id=$row[ID]&vid=$row[vID]\">$row[Name] $row[Version]</a> by $authors</h3></TD></TR>\n";

    if ($type=="T") {
        $installink = "javascript:void(InstallTrigger.installChrome(InstallTrigger.SKIN,'$row[URI]','$row[Name] $row[Version]'))";
    } else if ($type=="E") {
          $installink = "javascript:void(InstallTrigger.install({'$row[Name] $row[Version]':'$row[URI]'}))";
    }

  echo"<TR><TD COLSPAN=4 style=\"font-size: 8pt;\"><a href=\"$installink\">( Install Now )</a> $row[Description] ($categories)</TD></TR>\n";
  echo"<TR>\n";
  if ($row2[UserName]) {
    echo"<TD COLSPAN=2 style=\"font-size: 8pt;\"><strong>Requested by:</strong> <a href=\"mailto:$row2[UserEmail]\">$row2[UserName]</a> on $date</TD>\n";
  } else {
    echo"<TD COLSPAN=2></TD>\n";
  }
  echo"</TR>\n<TR>";
  echo"<TD style=\"font-size: 8pt;\"><strong>Works with: </strong>";
  $sql3 = "SELECT `shortname`, `MinAppVer`, `MaxAppVer` FROM `t_version` TV INNER JOIN `t_applications` TA ON TV.AppID = TA.AppID WHERE `URI`='$row[URI]' ORDER BY `AppName` ASC";
    $sql_result3 = mysql_query($sql3, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
     while ($row3 = mysql_fetch_array($sql_result3)) {
  echo"".ucwords($row3[shortname])." $row3[MinAppVer]-$row3[MaxAppVer] \n";
    }
  if($row[OSName] != "ALL") { echo" ($row[OSName])"; }
  echo"</TD>\n";
  echo"</TR>\n";
 
//Approval Form for this Extension Item
  echo"<TR><TD COLSPAN=4 style=\"font-size: 8pt\">\n";
  echo"Install? <input name=\"installation_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"Installs OK?\">\n";
  echo"Uninstall? <input name=\"uninstallation_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"Uninstalls OK?\">\n";
  echo"App Works? <input name=\"appworks_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"App Works OK? (Loading pages/messages, Tabs, Back/Forward)\">\n";
  echo"Clean Profile? <input name=\"cleanprofile_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"Using a Clean Profile? (I.E. No Major Extensions Installed, like TBE)\">\n";
if ($type=="E") {
  echo"New Chrome? <input name=\"newchrome_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"Extension Added Chrome to the UI?\">\n";
  echo"Works? <input name=\"worksasdescribed_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"Item Works as AuthorDescribes\">\n";
} else if ($type=="T") {
  echo"Visual Errors? <input name=\"visualerrors_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"No Visual Errors / Rendering Problems\">\n";
  echo"Theme Complete? <input name=\"allelementsthemed_$i\" type=\"checkbox\" value=\"YES\" TITLE=\"All Components Themed? (Including No Missing Icons?)\">\n";
}
  echo"</TD></TR>\n";
  echo"<TR><TD COLSPAN=4 style=\"font-size: 8pt\">\n";
  echo"<input name=\"type_$i\" type=\"hidden\" value=\"$type\">\n";
  echo"<input name=\"file_$i\" type=\"hidden\" value=\"$uri\">\n";
  echo"<input name=\"name_$i\" type=\"hidden\" value=\"$row[Name]\">\n";
  echo"<input name=\"version_$i\" type=\"hidden\" value=\"$row[Version]\">\n";
  echo"Tested On: OSes: <input name=\"testos_$i\" type=\"text\" size=10 title=\"What OS(es) did you test in? Windows, Linux, MacOSX, etc\">\n";
  echo"Apps: <input name=\"testbuild_$i\" type=\"text\" size=10 title=\"What app(s) version(s)/buildid(s)? (Ex. Firefox 1.0RC1 or 0.10+ 20041010)\">\n";
  echo"Comments: <input name=\"comments_$i\" type=\"text\" size=\"35\" title=\"Comments to Author (Will Be E-Mailed w/ Notice of your Action)\">"; 
  echo"</TD></TR>\n";

  echo"<TR><TD COLSPAN=4 style=\"font-size: 8pt\">\n";
  echo"Approve? <input name=\"approval_$i\" type=\"radio\" value=\"YES\"> Deny? <input name=\"approval_$i\" type=\"radio\" value=\"NO\"> No Action? <input name=\"approval_$i\" type=\"radio\" checked=\"checked\" VALUE=\"noaction\">\n";
  echo"</TD></TR>\n";
 }

echo"<input name=\"maxvid\" type=\"hidden\" value=\"$i\">\n";
?>
<?php if ($num_results > "0") { ?>
<TR><TD COLSPAN=4 ALIGN=CENTER><input name="submit" type="submit" value="Submit">&nbsp;&nbsp;<input name="reset" type="reset" value="Reset"></TD></TR>
<?php } else { ?>
<TR><TD COLSPAN=4 ALIGN=CENTER>No items are pending approval at this time</TD></TR>
<?php } ?>
</form>
</TABLE>

<?php 
} else if ($function=="approvalhistory") {
?>
<style type="text/css">
TD { font-size: 8pt }
</style>
<h1>Approval History Log</h2>
<span style="font-size: 8pt;">Incomplete Basic UI for the Approval History Log. <a href="https://bugzilla.mozilla.org/show_bug.cgi?id=255305">Bug 255305</a>.</span>
<table border=0 cellpadding=2 cellspacing=0 align="Center">
<tr>
<td></td>
<td style="font-size: 8pt">ID</td>
<td style="font-size: 8pt">vID</td>
<td style="font-size: 8pt">uID</td>
<td style="font-size: 8pt">Action</td>
<td style="font-size: 8pt">Date</td>
<td style="font-size: 6pt">Install?</td>
<td style="font-size: 6pt">Unistall?</td>
<td style="font-size: 6pt">New Chrome?</td>
<td style="font-size: 6pt">App Works?</td>
<td style="font-size: 6pt">Visual Errors?</td>
<td style="font-size: 6pt">All Elements Themed?</td>
<td style="font-size: 6pt">Clean Profile?</td>
<td style="font-size: 6pt">Works As Described?</td>
<td style="font-size: 6pt">Test Build:</td>
<td style="font-size: 7pt">Test OS:</td>
<td style="font-size: 7pt">Comments:</td>
</tr>
<?php
$sql ="SELECT * FROM `t_approvallog` ORDER BY `date` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $num_results = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $logid = $row["LogID"];
    $id = $row["ID"];
    $vid = $row["vID"];
    $userid = $row["UserID"];
    $action = $row["action"];
    $date = $row["date"];
    $installation = $row["Installation"];
    $uninstallation = $row["Uninstallation"];
    $newchrome = $row["NewChrome"];
    $appworks = $row["AppWorks"];
    $visualerrors = $row["VisualErrors"];
    $allelementsthemed = $row["AllElementsThemed"];
    $cleanprofile = $row["CleanProfile"];
    $worksasdescribed = $row["WorksAsDescribed"];
    $testbuild = $row["TestBuild"];
    $testos = $row["TestOS"];
    $comments = $row["comments"];

    echo"<tr>
    <td>$i.</td>
    <td>$id</td>
    <td>$vid</td>
    <td>$userid</td>
    <td>$action</td>
    <td>$date</td>
    <td>$installation</td>
    <td>$uninstallation</td>
    <td>$newchrome</td>
    <td>$appworks</td>
    <td>$visualerrors</td>
    <td>$allelementsthemed</td>
    <td>$cleanprofile</td>
    <td>$worksasdescribed</td>
    <td>$testbuild</td>
    <td>$testos</td>
    <td>$comments</td>
    </tr>\n";
  }
?>
</table>


<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>