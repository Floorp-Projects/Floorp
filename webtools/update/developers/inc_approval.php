<?php
//Function: process_approval(file, action)
// Reusable function for handling the approval process, file should be the DB URI and action is either approve or deny.
// Returns true on success, false on failure.

function process_approval($type, $file, $action) {
global $connection, $sitehostname, $ftpurl, $repositorypath;
if ($action=="approve") {
  $action_comment = "Approval+";
  $approved = "YES";
 } else if ($action=="deny") {
  $action_comment = "Approval-";
  $approved = "NO";
 }

//Firstly, log the comments and action taken..
$userid = $_SESSION["uid"];
$sql = "SELECT TM.ID, `Name`, `vID` from `t_main` TM INNER JOIN `t_version` TV ON TM.ID = TV.ID WHERE TV.URI = '$file' ORDER BY `vID` ASC";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
      $id = $row["ID"];
      $vid = $row["vID"];
      $name = $row["Name"];

global $installation, $uninstallation, $newchrome, $appworks, $visualerrors, $allelementsthemed, $cleanprofile, $worksasdescribed, $testbuild, $testos, $comments;
        $sql2 = "INSERT INTO `t_approvallog` (`ID`, `vID`, `UserID`, `action`, `date`, `Installation`, `Uninstallation`, `NewChrome`, `AppWorks`, `VisualErrors`, `AllElementsThemed`, `CleanProfile`, `WorksAsDescribed`, `TestBuild`, `TestOS`, `comments`) VALUES ('$id', '$vid', '$userid', '$action_comment', NOW(NULL), '$installation', '$uninstallation', '$newchrome', '$appworks', '$visualerrors', '$allelementsthemed', '$cleanprofile', '$worksasdescribed', '$testbuild', '$testos', '$comments');";
          $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
          if ($sql_result2) {} else { $operations_failed=="true";}
    }
    if ($sql_result) {} else { $operations_failed=="true";}

//Secondly, If Stage 1 was successful (and we're approving the file), let's move the file to it's new home in /ftp/ for staging...
$filename = str_replace ("http://$sitehostname/developers/approvalfile.php", "$repositorypath/approval", $file);
if ($action=="approve") {
    if (file_exists($filename)) {
        if ($type=="T") {$type="themes";} else if ($type=="E") {$type="extensions";}
        $path = strtolower("$type/$name");
        $destination = str_replace("approval",strtolower("ftp/$path"),$filename);
        $dirpath = "$repositorypath/ftp/$path";
        if (!file_exists($dirpath)) {
            mkdir($dirpath,0755);
        }
        if (rename("$filename", "$destination")) {
            //Rename Successfull
        } else {
            //Rename Unsuccessfull
            $operations_failed=="true";
        }
        //$ftpurl defined in config.php
        $uri = str_replace("$repositorypath/ftp","$ftpurl","$destination");
    }

} else if ($action=="deny") {
    //We're denying approval on this item, delete the file and set URI to null.
    if (file_exists($filename)) {unlink($filename); }
    $uri = "";

}

//Thirdly, update version record...
$sql = "UPDATE `t_version` SET `URI`='$uri', `approved`='$approved', `DateUpdated`=NOW(NULL) WHERE `URI`='$file'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if ($sql_result) {} else { $operations_failed=="true";}

@include"mail_approval.php";

if ($operations_failed=="true") { return false; } else { return true; }

}

?>