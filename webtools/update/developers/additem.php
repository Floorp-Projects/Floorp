<?php
require"../core/config.php";
require"core/sessionconfig.php";
$function = $_GET["function"];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Add Item</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>

<?php
if (!$function or $function=="additem") {
if (!$_GET["type"]) {$_GET["type"] = "E"; }
$typearray = array("E"=>"Extension","T"=>"Theme");
$typename = $typearray[$_GET["type"]];
?>
<h1>Add New <?php echo"$typename"; ?></h1>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 0px #000000; width: 100%">
<FORM NAME="additem" METHOD="POST" ACTION="?function=additem2" enctype="multipart/form-data">
<INPUT NAME="type" TYPE="hidden" VALUE="<?php echo"$_GET[type]"; ?>">
<TR><TD style="padding-left: 20px">
Your <?php echo"$typename"?> File:<BR>
<INPUT NAME="file" SIZE=40 TYPE="FILE"><BR>
<BR>
<INPUT NAME="button" TYPE="BUTTON" VALUE="Cancel" onclick="javascript:history.back()"> <INPUT NAME="submit" TYPE="SUBMIT" VALUE="Next &#187;"> 
</TD></TR>
</FORM>
</TABLE>

<?php
} else if ($function=="additem2") {

$filename=$_FILES['file']['name'];
$filetype=$_FILES['file']['type'];
$filesize=$_FILES['file']['size'];
$uploadedfile=$_FILES['file']['tmp_name'];
$status=$_FILES['file']['error'];

//Convert File-Size to Kilobytes
$filesize = round($filesize/1024, 1);

//Status
if ($status==0) {$statusresult="Success!";
} else if ($status==1) {$statusresult="Error: File Exceeds upload_max_filesize (PHP)";
} else if ($status==2) {$statusresult="Error: File Exceeds max_file_size (HTML)";
} else if ($status==3) {$statusresult="Error: File Incomplete, Partial File Received";
} else if ($status==4) {$statusresult="Error: No File Was Uploaded";
}
$manifest_exists = "FALSE";
$destination = "$websitepath/files/temp/$filename";
if (move_uploaded_file($uploadedfile, $destination)) {
$uploadedfile = $destination;
$chmod_result = chmod("$uploadedfile", 0644); //Make the file world readable. prevent nasty permissions issues.
}

//If this was legacy mode, we're coming back from step1b so the file wasn't just submitted and we need to just pick it up again.
if ($_POST["legacy"]=="TRUE") {
$filename = $_POST["filename"];
$filesize = $_POST["filesize"];
$uploadedfile="$websitepath/files/temp/$filename";
}
$zip = zip_open("$uploadedfile");
if ($zip) {

   while ($zip_entry = zip_read($zip)) {
     if (zip_entry_name($zip_entry)=="install.rdf") {
     $manifest_exists = "TRUE";
      // echo "Name:              " . zip_entry_name($zip_entry) . "\n";
      // echo "Actual Filesize:    " . zip_entry_filesize($zip_entry) . "\n";
      // echo "Compressed Size:    " . zip_entry_compressedsize($zip_entry) . "\n";
      // echo "Compression Method: " . zip_entry_compressionmethod($zip_entry) . "\n";

       if (zip_entry_open($zip, $zip_entry, "r")) {
       //    echo "File Contents:\n";
           $buf = zip_entry_read($zip_entry, zip_entry_filesize($zip_entry));
          // echo "$buf\n";

           zip_entry_close($zip_entry);
       }
       echo "\n";
   }
   }

   zip_close($zip);

}
if ($manifest_exists=="TRUE" or $_POST["legacy"]=="TRUE") {
//echo"install.rdf is present, use standard mode...<BR>\n";

//------------------
//  Construct $manifestdata[] array from install.rdf info.
//  Thanks to Chewie[] with the Perl-Compatable RegExps Help. :-)
//-------------------

// em:id
preg_match("/<em:id>(.*?)<\/em:id>/", $buf, $matches);
$manifestdata["id"]=$matches[1];

//	em:version
preg_match("/<em:version>(.*?)<\/em:version>/", $buf, $matches);
$manifestdata["version"]=$matches[1];

//em:targetApplication
preg_match_all("/<em:targetApplication>(.*?)<\/em:targetApplication>/s", $buf, $matches);
//echo"<pre>";
//print_r($buf);
//print_r($matches);
//echo"</pre>";
  foreach ($matches[0] as $targetapp ) {
  //em:targetApplication --> em:id
    preg_match("/<em:id>(.*?)<\/em:id>/", $targetapp, $matches);
    $i = $matches[1];
    $manifestdata["targetapplication"][$i]["id"]=$matches[1];

  //em:targetApplication --> em:minVersion
    preg_match("/<em:minVersion>(.*?)<\/em:minVersion>/", $targetapp, $matches);
    $manifestdata["targetapplication"][$i]["minversion"]=$matches[1];

  //em:targetApplication --> em:maxVersion
    preg_match("/<em:maxVersion>(.*?)<\/em:maxVersion>/", $targetapp, $matches);
    $manifestdata["targetapplication"][$i]["maxversion"]=$matches[1];
  }

//em:name
preg_match("/<em:name>(.*?)<\/em:name>/", $buf, $matches);
$manifestdata["name"]=$matches[1];

//em:description
preg_match("/<em:description>(.*?)<\/em:description>/", $buf, $matches);
$manifestdata["description"]=$matches[1];

//em:homepageURL
preg_match("/<em:homepageURL>(.*?)<\/em:homepageURL>/", $buf, $matches);
$manifestdata["homepageurl"]=$matches[1];


//echo"<h1>Adding Extension... Checking file...</h1>\n";
//echo"<pre>"; print_r($manifestdata); echo"</pre>\n";
//Populate Form Variables from manifestdata.
$id = $manifestdata[id];
$name = $manifestdata[name];
$version = $manifestdata[version];
$homepage = $manifestdata[homepageurl];
$description = $manifestdata[description];


//Check GUID for validity/existance, if it exists, check the logged in author for permission
$sql = "SELECT ID, GUID from `t_main` WHERE `GUID` = '$manifestdata[id]' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  if (mysql_num_rows($sql_result)=="1") {
//    echo"This is a updated extension... Checking author data...<br>\n";
    $mode = "update";
    $row = mysql_fetch_array($sql_result);
    $item_id = $row["ID"];
if ($_POST["legacy"]=="TRUE") {$item_id = $_POST["existingitems"]; }
    $sql = "SELECT `UserID` from `t_authorxref` WHERE `ID`='$item_id' AND `UserID` = '$_SESSION[uid]' LIMIT 1";
      $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      if (mysql_num_rows($sql_result)=="1" or ($_SESSION["level"]="admin" or $_SESSION["level"]="editor")) {
//        echo"This extension belongs to the author logged in<br>\n";
      } else {
        echo"ERROR!! This extension does NOT belong to the author logged in.<br>\n";
        die("Terminating...");
      }

  } else {
  $mode = "new";
//  echo"This is a new extension...<br>\n";
  }

//Verify MinAppVer and MaxAppVer per app for validity, if they're invalid, reject the file.
if ($_POST["legacy"]=="TRUE" AND !$manifestdata[targetapplication]) {$manifestdata[targetapplication]=array(); }
foreach ($manifestdata[targetapplication] as $key=>$val) {
//echo"$key -- $val[minversion] $val[maxversion]<br>\n";

$i=0;
  $sql = "SELECT `AppName`, `major`, `minor`, `release`, `SubVer` FROM `t_applications` WHERE `GUID`='$key' ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
   $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $appname = $row["AppName"];
    $subver = $row["SubVer"];
    $release = "$row[major].$row[minor]";
    if ($row["release"]) {$release = "$release.$row[release]";}
    if ($subver !=="final") {$release="$release$subver";}
      if ($release == $val[minversion]) { $versioncheck[$key][minversion_valid] = "true"; }
      if ($release == $val[maxversion]) { $versioncheck[$key][maxversion_valid] = "true"; }

    }
  if (!$versioncheck[$key][minversion_valid]) {
    $versioncheck[$key][minversion_valid]="false";
    echo"Error! The MinAppVer for $appname of $val[minversion] in install.rdf is invalid.<br>\n";
    $versioncheck[errors]="true";
  }
  if (!$versioncheck[$key][maxversion_valid]) {
    $versioncheck[$key][maxversion_valid]="false";
    echo"Error! The MaxAppVer for $appname of $val[maxversion] in install.rdf is invalid.<br>\n";
    $versioncheck[errors]="true";
  }
}

//echo"<pre>"; print_r($versioncheck); echo"</pre>\n";

if ($versioncheck[errors]=="true") {
  echo"Errors were encountered during install.rdf checking...<br>\n";
  die("Aborting...");
} else { 
//  echo"install.rdf minAppVer and maxAppVer valid...<br>\n";
}


} else {
//echo"install.rdf is not present, use legacy mode...<br>\n";
//header("Location: http://$_SERVER[HTTP_HOST]/developers/additem.php?function=step1b&filename=$filename");
echo"<h1>Add Step 1b: Legacy Item Data Entry: ($filename)</h1>\n";
?>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 0px #000000; width: 100%">
<FORM NAME="additem" METHOD="POST" ACTION="?function=additem2" enctype="multipart/form-data">
<INPUT NAME="type" TYPE="hidden" VALUE="<?php echo"$_POST[type]"; ?>">
<TR><TD style="padding-left: 20px">
  <INPUT NAME="legacy" TYPE="HIDDEN" VALUE="TRUE">
  <INPUT NAME="mode" TYPE="RADIO" VALUE="new"<?php if ($_GET["mode"] != "update") {echo" CHECKED"; }?>> New <?php echo"$typename"; ?><br>
  <INPUT NAME="mode" TYPE="RADIO" VALUE="update"<?php if ($_GET["mode"] == "update") {echo" CHECKED"; } ?>> Update to:
<SELECT NAME="existingitems">
<?php

$sql = "SELECT  TM.ID, TM.Name FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TM.Type = '$_POST[type]'";
if ($_GET["admin"] =="true" AND $_SESSION[level] =="admin") {} else{ $sql .= "AND TU.UserEmail = '$_SESSION[email]'"; }
$sql .="GROUP BY `name` ORDER  BY  `Name`  ASC ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $name = $row["Name"];
	echo"<OPTION value=\"$id\""; if ($_GET[id]==$id) {echo" SELECTED"; } echo">$name</OPTION>\n";
  }
?>
</SELECT><BR>
Your file: <?php echo"$filename"; ?> <INPUT name="filename" TYPE=HIDDEN VALUE="<?php echo"$filename"; ?>"> <INPUT name="filesize" TYPE=HIDDEN VALUE="<?php echo"$filesize"; ?>">
<BR>
<INPUT NAME="button" TYPE="BUTTON" VALUE="&#171;&nbsp;Back" onclick="javascript:history.back()"> <INPUT NAME="submit" TYPE="SUBMIT" VALUE="Next &#187;"> 
</TD></TR>
</FORM>
</TABLE>




<?php
exit;
}
//exit;


$typearray = array("E"=>"Extension","T"=>"Theme");
$type = $_POST["type"];
$typename = $typearray[$type];

if ($mode=="update") {
 $sql = "SELECT  `Name`, `Homepage`, `Description` FROM  `t_main` WHERE `ID` = '$item_id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    if (!$name) { $name=$row["Name"]; }
    $homepage = $row["Homepage"];
    $description = $row["Description"];

 $authors = ""; $i="";
 $sql = "SELECT TU.UserEmail FROM  `t_authorxref` TAX INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID WHERE `ID` = '$item_id'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $numresults = mysql_num_rows($sql_result);
   while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $email = $row["UserEmail"];
   $authors .= "$email";
   if ($i < $numresults) { $authors .=", "; }
   }

//Get Currently Set Categories for this Object...
 $sql = "SELECT  TCX.CategoryID, TC.CatName FROM  `t_categoryxref`  TCX 
INNER JOIN t_categories TC ON TCX.CategoryID = TC.CategoryID
WHERE TCX.ID = '$item_id'
ORDER  BY  `CatName`  ASC ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $n++;
   $catid = $row["CategoryID"];
   $categories[$n] = $catid;
  }
unset($n);
}

if (!$categories) {$categories = array(); }
?>
<h1>Add New <?php echo"$typename"; ?> &#187;&#187; Step 2:</h2>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 0px #000000; width: 100%">
<FORM NAME="addstep2" METHOD="POST" ACTION="?function=additem3">
<INPUT NAME="mode" TYPE="HIDDEN" VALUE="<?php echo"$mode"; ?>">
<?php if ($mode=="update") { ?>
<INPUT NAME="item_id" TYPE="HIDDEN" VALUE="<?php echo"$item_id"; ?>">
<?php } ?>
<INPUT NAME="guid" TYPE="HIDDEN" VALUE="<?php echo"$id"; ?>">
<INPUT NAME="type" TYPE="HIDDEN" VALUE="<?php echo"$type"; ?>">
<TR><TD><SPAN class="global">Name*</SPAN></TD> <TD><INPUT NAME="name" TYPE="TEXT" VALUE="<?php echo"$name"; ?>" SIZE=45 MAXLENGTH=100></TD>


<?php
//Get the Category Table Data for the Select Box
 $sql = "SELECT  `CategoryID`, `CatName` FROM  `t_categories` WHERE `CatType` = '$type' ORDER  BY  `CatName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
//  $sqlnum = mysql_num_rows($sql_result);
?>
<TD ROWSPAN=8 VALIGN=TOP><SPAN class="global">Categories:</SPAN><BR>&nbsp;&nbsp;&nbsp;&nbsp;<SELECT NAME="categories[]" MULTIPLE="YES" SIZE="10">
<?php
  while ($row = mysql_fetch_array($sql_result)) {
    $catid = $row["CategoryID"];
    $catname = $row["CatName"];

    echo"<OPTION value=\"$catid\"";
    foreach ($categories as $validcat) {
    if ($validcat==$catid) { echo" SELECTED"; }
    }
    echo">$catname</OPTION>\n";

  }
?>
</SELECT></TD></TR>

<?php
if (!$authors) {$authors="$_SESSION[email]"; }
?>
<TR><TD><SPAN class="global">Author(s):*</SPAN></TD><TD><INPUT NAME="authors" TYPE="TEXT" VALUE="<?php echo"$authors"; ?>" SIZE=45></TD></TR>
<?php
if ($version) {
    echo"<TR><TD><SPAN class=\"file\">Version:*</SPAN></TD><TD>$version<INPUT NAME=\"version\" TYPE=\"HIDDEN\" VALUE=\"$version\"></TD></TR>\n";
} else {
    echo"<TR><TD><SPAN class=\"file\">Version:*</SPAN></TD><TD><INPUT NAME=\"version\" TYPE=\"TEXT\" VALUE=\"$version\"></TD></TR>\n";
}
    echo"<TR><TD><SPAN class=\"file\">OS*</SPAN></TD><TD><SELECT NAME=\"osid\">";
 $sql = "SELECT * FROM `t_os` ORDER BY `OSName` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $osid = $row["OSID"];
  $osname = $row["OSName"];
   echo"<OPTION value=\"$osid\">$osname</OPTION>\n";
  }
    echo"</SELECT></TD></TR>\n";
    echo"<TR><TD><SPAN class=\"file\">Filename:</SPAN></TD><TD>$filename ($filesize"."kb) <INPUT name=\"filename\" type=\"hidden\" value=\"$filename\"><INPUT name=\"filesize\" type=\"hidden\" value=\"$filesize\"></TD></TR>\n";

echo"<TR><TD COLSPAN=2><SPAN class=\"file\">Target Application(s):</SPAN></TD></TR>\n";
 $sql2 = "SELECT `AppName`,`GUID` FROM `t_applications` GROUP BY `AppName` ORDER BY `AppName` ASC";
 $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row2 = mysql_fetch_array($sql_result2)) {
   $appname = $row2["AppName"];
   $guid = $row2["GUID"];
   if ($appname == "Mozilla") { $mozguid = $guid; }
   $minappver = $manifestdata["targetapplication"]["$guid"]['minversion'];
   $maxappver = $manifestdata["targetapplication"]["$guid"]['maxversion'];
    echo"<TR><TD></TD><TD>$appname ";

if (($mode=="new" or $mode=="update") and (strtolower($appname) !="mozilla" or $manifestdata["targetapplication"]["$mozguid"])) {
 //Based on Extension Manifest (New Mode)
    if ($minappver and $maxappver) {
      echo"$minappver - $maxappver\n";
      echo"<INPUT name=\"$appname-minappver\" TYPE=\"HIDDEN\" VALUE=\"$minappver\">\n";
      echo"<INPUT name=\"$appname-maxappver\" TYPE=\"HIDDEN\" VALUE=\"$maxappver\">\n";
    } else {
      echo"N/A";
    }
} else {
 //Legacy Mode Code...
    if ($appname =="Firefox" or $appname == "Thunderbird") { 
    echo"<br><SPAN style=\"font-size: 8pt; font-weight: bold\">Incompatable with Legacy Extensions (Requires install.rdf)</SPAN>";
    } else {

$sql = "SELECT `version`,`major`,`minor`,`release`,`SubVer` FROM `t_applications` WHERE `AppName` = '$appname' ORDER BY `major` ASC, `minor` ASC, `release` ASC, `SubVer` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
echo"<SELECT name=\"$appname-minappver\" TITLE=\"Minimum Version* (Required)\">";
echo"<OPTION value\"\"> - </OPTION>\n";
  while ($row = mysql_fetch_array($sql_result)) {
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = "$release.$row[release]";}
    $subver = $row["SubVer"];
    if ($subver !=="final") {$release="$release$subver";}
    echo"<OPTION value=\"$release\">$release</OPTION>\n";
  }
echo"</select>\n";

   echo"&nbsp;-&nbsp;";

$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
echo"<SELECT name=\"$appname-maxappver\" TITLE=\"Maximum Version* (Required)\">";
echo"<OPTION value\"\"> - </OPTION>\n";
  while ($row = mysql_fetch_array($sql_result)) {
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = "$release.$row[release]";}
    $subver = $row["SubVer"];
    if ($subver !=="final") {$release="$release$subver";}
    echo"<OPTION value=\"$release\">$release</OPTION>\n";
  }
echo"</select>\n";
    echo"</TD></TR>\n";
 }   }
}
?>

<TR><TD><SPAN class="global">Homepage</SPAN></TD> <TD COLSPAN=2><INPUT NAME="homepage" TYPE="TEXT" VALUE="<?php echo"$homepage"; ?>" SIZE=60 MAXLENGTH=200></TD></TR>
<TR><TD><SPAN class="global">Description*</SPAN></TD> <TD COLSPAN=2><TEXTAREA NAME="description" ROWS=3 COLS=55><?php echo"$description"; ?></TEXTAREA></TD></TR>
<?php
    echo"<TR><TD><SPAN class=\"file\">Version Notes:</SPAN></TD><TD COLSPAN=2><TEXTAREA NAME=\"notes\" ROWS=4 COLS=55>$notes</TEXTAREA></TD></TR>\n";
?>
<TR><TD COLSPAN="3" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Next &#187;">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"></TD></TR>
</FORM>


</TABLE>

<?php
} else if ($function=="additem3") {
//print_r($_POST);
//exit;

//Verify that there's at least one min/max app value pair...
 $sql = "SELECT `AppName`,`AppID` FROM `t_applications` GROUP BY `AppName` ORDER BY `AppName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $appname = $row["AppName"];
   $appid = $row["AppID"];
   if (!$minappver AND $_POST["$appname-minappver"]) {$minappver="true";}
   if (!$maxappver AND  $_POST["$appname-maxappver"]) {$maxappver="true";}

  }

//Author List -- Autocomplete and Verify, if no valid authors, kill add.. otherwise, autocomplete/prompt
  $authors = $_POST["authors"];
  $authors = explode(", ","$authors");
foreach ($authors as $author) {
if (strlen($author)<2) {continue;} //Kills all values that're too short.. 
$a++;
 $sql = "SELECT `UserID`,`UserEmail` FROM `t_userprofiles` WHERE `UserEmail` LIKE '$author%' ORDER BY `UserMode`, `UserName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $numresults = mysql_num_rows($sql_result);
   while ($row = mysql_fetch_array($sql_result)) {
    $userid = $row["UserID"];
    $useremail = $row["UserEmail"];
  if ($numresults>1) {
   //Too many e-mails match, store individual data for error block.
    $r++;
     $emailerrors[$a]["foundemails"][$r] = $useremail;
  }
 $authorids[] = $userid;
 $authoremails[] = $useremail;
 }
 if ($numresults !="1") {
  //No Valid Entry Found for this E-Mail or too many, kill and store data for error block.
  $emailerrors[$a]["author"] = "$author";
  $updateauthors = "false"; // Just takes one of these to kill the author update.
  }
}
unset($a,$r);



if ($_POST["name"] AND $_POST["type"] AND $_POST["authors"] AND $updateauthors !="false" AND $_POST["version"] AND $_POST["osid"] AND $_POST["filename"] AND $_POST["filesize"] AND $_POST["description"] AND $minappver AND $maxappver) {
//All Needed Info is in the arrays, procceed with inserting...

//Create DIV for Box around the output...
 echo"<h1>Adding Item... Please Wait...</h1>\n";
 echo"<DIV>\n";

//Phase One, Main Data
$name = $_POST["name"];
$homepage = $_POST["homepage"];
$description = $_POST["description"];
$item_id = $_POST["item_id"];
$guid = $_POST["guid"];
$type = $_POST["type"];
if ($_POST["mode"]=="update") { 
$sql = "UPDATE `t_main` SET `Name`='$name', `Homepage`='$homepage', `Description`='$description', `DateUpdated`=NOW(NULL) WHERE `ID`='$item_id' LIMIT 1";
} else {
$sql = "INSERT INTO `t_main` (`GUID`, `Name`, `Type`, `Homepage`,`Description`,`DateAdded`,`DateUpdated`) VALUES ('$guid', '$name', '$type', '$homepage', '$description', NOW(NULL), NOW(NULL));";
}
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
if ($sql_result) {echo"Updating/Adding record for $name...<br>\n";}


//Get ID for inserted row... if we don't know it already
if (!$_POST[item_id] and $_POST["mode"] !=="update") {
 $sql = "SELECT `ID` FROM `t_main` WHERE `GUID`='$_POST[guid]' AND `Name`='$_POST[name]' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
   $id = $row["ID"];
  } else {
   $id = $_POST["item_id"];
  }


//Phase 2 -- Commit Updates to AuthorXref tables.. with the ID and UserID.
if ($updateauthors != "false") {
 //Remove Current Authors
 $sql = "DELETE FROM `t_authorxref` WHERE `ID` = '$id'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

 //Add New Authors based on $authorids
 sort($authorids);
  foreach ($authorids as $authorid) {
   	$sql = "INSERT INTO `t_authorxref` (`ID`, `UserID`) VALUES ('$id', '$authorid');";
    $result = mysql_query($sql) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
  }
   if ($result) { echo"Authors added...<br>\n"; }
} else {
   echo"ERROR: Could not update Authors list, please fix the errors printed below and try again...<br>\n"; 
}

unset($authors); //Clear from Post.. 


// Phase 3, t_categoryxref

if (!$_POST["categories"]) {
//No Categories defined, need to grab one to prevent errors...
 $sql = "SELECT `CategoryID` FROM `t_categories` WHERE `CatType`='$type' AND `CatName`='Miscellaneous' LIMIT 1";
   $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
   $_POST["categories"] = array("$row[CategoryID]");
  }

}

 //Delete Current Category Linkages...
   $sql = "DELETE FROM `t_categoryxref` WHERE `ID` = '$id'";
   $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

 //Add New Categories from $_POST["categories"]
   foreach ($_POST["categories"] as $categoryid) {
   	$sql = "INSERT INTO `t_categoryxref` (`ID`, `CategoryID`) VALUES ('$id', '$categoryid');";
    $result = mysql_query($sql) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
  }
   if ($result) {echo"Categories added...<br>\n"; }


//Phase 4, t_version rows

//Construct Internal App_Version Arrays
$i=0;
$sql = "SELECT `AppName`, `int_version`, `major`, `minor`, `release`, `SubVer`, `shortname` FROM `t_applications` ORDER BY `AppName`, `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $i++;
  $appname = $row["AppName"];
  $int_version = $row["int_version"];
  $subver = $row["SubVer"];
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = "$release.$row[release]";}
  if ($subver !=="final") {$release="$release$subver";}
    $app_internal_array[$release] = $int_version;
    $app_shortname[strtolower($appname)] = $row["shortname"];
  }

 $sql2 = "SELECT `AppName`,`AppID` FROM `t_applications` GROUP BY `AppName` ORDER BY `AppName` ASC";
 $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row2 = mysql_fetch_array($sql_result2)) {
   $appname = $row2["AppName"];
   $appid = $row2["AppID"];
   $minappver = $_POST["$appname-minappver"];
   $maxappver = $_POST["$appname-maxappver"];

if ($minappver and $maxappver) {

if ($app_internal_array["$minappver"]) {$minappver_int = $app_internal_array["$minappver"]; }
if ($app_internal_array["$maxappver"]) {$maxappver_int = $app_internal_array["$maxappver"]; }
if (!$minappver_int) {$minappver_int = $minappver;}
if (!$maxappver_int) {$maxappver_int = $maxappver;}


$version = $_POST["version"];
$osid = $_POST["osid"];
$filesize = $_POST["filesize"];
$uri = ""; //we don't have all the parts to set a uri, leave blank and fix when we do.
$notes = $_POST["notes"];

//If a record for this item's exact version, OS, and app already exists, find it and delete it, before inserting
  $sql3 = "SELECT `vID` from `t_version` TV INNER JOIN `t_applications` TA ON TA.AppID=TV.AppID WHERE `OSID`='$osid' AND `AppName` = '$appname' AND TV.Version='$version' ORDER BY `vID` ASC";
    $sql_result3 = mysql_query($sql3, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      while ($row = mysql_fetch_array($sql_result3)) {
        $sql = "DELETE FROM `t_version` WHERE `vID`='$row[vID]' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
          if ($sql_result) { echo"<strong>Warning!</strong> A version Record already exists for this item's Application/OS/Version combination. Deleting.<br>\n"; }
    }

$sql = "INSERT INTO `t_version` (`ID`, `Version`, `OSID`, `AppID`, `MinAppVer`, `MinAppVer_int`, `MaxAppVer`, `MaxAppVer_int`, `Size`, `URI`, `Notes`, `DateAdded`, `DateUpdated`) VALUES ('$id', '$version', '$osid', '$appid', '$minappver', '$minappver_int', '$maxappver', '$maxappver_int', '$filesize', '$uri', '$notes', NOW(NULL), NOW(NULL));";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 if ($sql_result) {echo"Added $name version $version for $appname<br>\n"; $apps_array[]=$app_shortname[strtolower($appname)];}

$sql = "SELECT `vID` from `t_version` WHERE `id` = '$id' ORDER BY `vID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
   $vid_array[] = $row["vID"];
}
}

$sql = "SELECT `OSName` FROM `t_os` WHERE `OSID`='$osid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
   $osname = $row["OSName"];


//Construct the New Filename
$filename_array = explode(".",$_POST[filename]);
$filename_count = count($filename_array)-1;
$fileext = $filename_array[$filename_count];

$itemname = str_replace(" ","_",$name);
$j=0; $app="";
$app_count = count($apps_array);
foreach ($apps_array as $app_val) {
$j++;
$apps .="$app_val";
if ($j<$app_count) {$apps .="+"; }
}
$newfilename = "$itemname-$version-$apps";
if (strtolower($osname) !=="all") {$newfilename .="-".strtolower($osname).""; }
$newfilename .=".$fileext";


//Move temp XPI to home for approval queue items...
$oldpath = "$repositorypath/temp/$_POST[filename]";
$newpath = "$repositorypath/approval/".strtolower($newfilename);
if (file_exists($oldpath)) { 
  rename("$oldpath","$newpath");
  echo"File $newfilename saved to disk...<br>\n";
}
$uri = str_replace("$repositorypath/approval/","http://$sitehostname/developers/approvalfile.php/",$newpath);
//echo"$newfilename ($oldpath) ($newpath) ($uri)<br>\n";

foreach ($vid_array as $vid) {
  $sql = "UPDATE `t_version` SET `URI`='$uri' WHERE `vID`='$vid'";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
}

//Approval Queue
$_SESSION["trusted"]=="FALSE";
 //Trusted User Code Not Yet Implemented, needs a shared function w/ the approval queue
 // for file moving, creation. (and sql updating?)
  //Check if the item belongs to the user, (special case for where admins are trusted, the trust only applies to their own work.)
  $sql = "SELECT `UserID` from `t_authorxref` WHERE `ID`='$id' AND `UserID` = '$_SESSION[uid]' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if (mysql_num_rows($sql_result)=="1" AND $_SESSION["trusted"]=="TRUE") {
    //User is trusted and the item they're modifying inheirits that trust.
    $action = "Approval+";
    $comments = "Auto-Approval for Trusted User";
    //$typenames = array("E"=>"extensions","T"=>"themes");
    //$typename = $typenames[$type];

    //$uri = strtolower(str_replace("http://$sitehostname/developers/approvalfile.php/","http://ftp.mozilla.org/pub/mozilla.org/$typename/$itemname/",$newpath));
    //foreach ($vid_array as $vid) {
    //   $sql = "UPDATE `t_version` SET `URI`='$uri' WHERE `vID`='$vid'";
    //   $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    //}

    } else {
    $action="Approval?";
    $comments="";
    }


//Firstly, log the comments and action taken..
$userid = $_SESSION["uid"];

if (!$vid_array) { $vid_array = array(); }
foreach ($vid_array as $vid) {
$sql = "INSERT INTO `t_approvallog` (`ID`, `vID`, `UserID`, `action`, `date`, `comments`) VALUES ('$id', '$vid', '$userid', '$action', NOW(NULL), '$comments');";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
}


echo"Process Complete...<br><br>\n";
echo"$name version $version has been added to the Mozilla Update database and is awaiting review by an editor, you will be notified when an editor reviews it.<br>\n";
echo"To review or make changes to your submission, visit the <A HREF=\"itemoverview.php?id=$id\">Item Details page</A>...<br>\n";

echo"<br><br>\n";
echo"<A HREF=\"/developers/\">&#171;&#171; Back to Home</A>";
echo"</div>\n";

}


//Author Error Handling/Display Block for Form Post...
if ($emailerrors) {

echo"
<h1>Adding Item... Error Found while processing authors</h1>\n
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE=\"border: 0px; width: 100%\">
<FORM NAME=\"addstep2b\" METHOD=\"POST\" ACTION=\"?function=additem3\">";

foreach ($_POST as $key => $val) {
if ($key=="authors" or $key=="submit") {continue; }
if ($key=="categories") {
foreach ($_POST["categories"] as $val) {
echo"<INPUT name=\"categories[]\" type=\"hidden\" value=\"$val\">\n";
}
continue;
}
echo"<INPUT name=\"$key\" type=\"hidden\" value=\"$val\">\n";
}


echo"<TR><TD COLSPAN=2 STYLE=\"\">\n";
echo"<DIV style=\"margin-left 2px; border: 1px dotted #CCC;\">";
foreach ($emailerrors as $authorerror) {
$author = $authorerror["author"];
$count = count($authorerror["foundemails"]);

if ($count=="0") {
//Error for No Entry Found
echo"<SPAN STYLE=\"color: #FF0000;\"><strong>Error! Entry '$author': No Matches Found.</strong></SPAN> Please check your entry and try again.<BR>\n";
} else {
//Error for Too Many Entries Found
echo"<SPAN STYLE=\"color: #FF0000;\"><strong>Error! Entry '$author': Too Many Matches.</strong></SPAN> Please make your entry more specific.<BR>\n";
}
if ($count>0 AND $count<6) {
echo"&nbsp;&nbsp;&nbsp;&nbsp;Possible Addresses found: ";
foreach ($authorerror["foundemails"] as $foundemails) {
$a++;
echo"$foundemails";

if ($a != $count) {echo", "; } else {echo"<br>\n";}
}
}

}
echo"</font></DIV></TD></TR>\n";
$authors = $_POST["authors"];
?>

<TR><TD><SPAN class="global">Author(s):*</SPAN></TD><TD><INPUT NAME="authors" TYPE="TEXT" VALUE="<?php echo"$authors"; ?>" SIZE=70><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Next &#187;"></TD></TR>
</FORM></TABLE>
<?php
}


} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>