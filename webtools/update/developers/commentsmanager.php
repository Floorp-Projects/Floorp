<?php
require"core/sessionconfig.php";
require"../core/config.php";

$function = $_GET["function"];
//Kill access to flagged comments for users.
if ($_SESSION["level"] !=="admin" and $_SESSION["level"] !=="editor") {
    if ($function=="flaggedcomments") {
        unset($function);
    }
}
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Comments Manager</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<?php
if (!$function) {
?>

<?php
if ($_POST["submit"]=="Flag Selected" or $_POST["submit"]=="Delete Selected") {
?>
<h1>Updating comments list, please wait...</h1>
<?php

    //Process Post Data, Make Changes to User Table.
    //Begin General Updating
    for ($i=1; $i<=$_POST[maxid]; $i++) {
      if (!$_POST["selected_$i"]) {
        continue;
      } else {
        $selected = escape_string($_POST["selected_$i"]);
      }

        //Admins/Editors can delete from here. Regular Users Can't.
        if ($_SESSION["level"] !=="admin" and $_SESSION["level"] !=="editor") {
            if ($_POST["submit"]=="Delete Selected") {
                $_POST["submit"]="Flag Selected";
            }
        }


     if (checkFormKey()) {
        if ($_POST["submit"]=="Delete Selected") {
            $sql = "DELETE FROM `t_feedback` WHERE `CommentID`='$selected'";
            $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
            if ($sql_result) {
                echo"Comment $selected deleted from database.<br>\n";
            }
        } else if ($_POST["submit"]=="Flag Selected") {
            $sql = "UPDATE `t_feedback` SET `flag`= 'YES' WHERE `CommentID`='$selected'";
            $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
            if ($sql_result) {
                echo"Comment $selected flagged for editor review.<br>\n";
            }
        }
     }

    }

  unset($i);
  echo"Your changes to the comment list have been succesfully completed<BR>\n";

 }
?>
<?php
if ($_GET["numpg"]) {$items_per_page=$_GET["numpg"]; } else {$items_per_page="50";} //Default Num per Page is 50
if (!$_GET["pageid"]) {$pageid="1"; } else { $pageid = $_GET["pageid"]; } //Default PageID is 1
$startpoint = ($pageid-1)*$items_per_page;

$id = escape_string($_GET["id"]);

$sql = "SELECT `Name` FROM `t_main` WHERE `ID`='$id' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $row = mysql_fetch_array($sql_result);
    $name = $row["Name"];

$sql = "SELECT CommentID FROM  `t_feedback` WHERE ID = '$id'";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $num_pages = ceil(mysql_num_rows($sql_result)/$items_per_page);
?>

<h1>Manage Comments for <?php echo"$name :: Page $pageid of $num_pages"; ?></h1>
<?php
//Flagged Comments Queue Link for Admins/Editors
if ($_SESSION["level"] =="admin" or $_SESSION["level"]=="editor") {
    echo"<a href=\"?function=flaggedcomments\">View Flagged Comments Queue</a> | \n";
}

// Begin Code for Dynamic Navbars
if ($pageid <=$num_pages) {
    $previd=$pageid-1;
    if ($previd >"0") {
        echo"<a href=\"?".uriparams()."&id=$id&page=$page&pageid=$previd\">&#171; Previous</A> &bull; ";
    }
}

echo"Page $pageid of $num_pages";
$nextid=$pageid+1;

if ($pageid <$num_pages) {
    echo" &bull; <a href=\"?".uriparams()."&id=$id&page=$page&pageid=$nextid\">Next &#187;</a>";
}
echo"<BR>\n";
?>

<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%">
<TR style="font-weight: bold">
<TH>Name/E-Mail</TH>
<TH>Date</TH>
<TH>Rating</TH>
<TH>Select</TH>
</TR>
<FORM NAME="updateusers" METHOD="POST" ACTION="?id=<?php echo"$id&pageid=$pageid&numpg=$items_per_page"; ?>&action=update">
<?writeFormKey();?>
<?php

 $sql = "SELECT * FROM `t_feedback` WHERE `ID`='$id' ORDER BY `CommentDate`DESC LIMIT $startpoint,$items_per_page";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
        $commentid = $row["CommentID"];
        $name = $row["CommentName"];
        $email = $row["email"];
        $title = $row["CommentTitle"];
        $notes = $row["CommentNote"];
        $helpful_yes = $row["helpful-yes"];
        $helpful_no = $row["helpful-no"];
        $helpful_total = $helpful_yes+$helpful_no;
        $date = date("l, F j Y g:i:sa", strtotime($row["CommentDate"]));
        $rating = $row["CommentVote"];
        if (!$title) {$title = "No Title"; }
        if (!$name) {$name = "Anonymous"; }
        if ($rating==NULL) {$rating="N/A"; }
        if ($row["flag"]=="YES") {$title .= " (flagged)"; }

   
   
   $i++;
    echo"<TR><TD COLSPAN=4><h2>$i.&nbsp;&nbsp;$title</h2></TD></TR>\n";
    echo"<TR>\n";
    echo"<TD COLSPAN=4>$notes";
    if ($helpful_total>0) {echo" ($helpful_yes of $helpful_total found this comment helpful)"; }
    echo"</TD>\n";
    echo"</TR>\n";
    echo"<TR>";
    if ($email) {
        echo"<TD>Posted by <A HREF=\"mailto:$email\">$name</A></TD>\n";
    } else {
        echo"<TD>Posted by $name</TD>\n";
    }
    echo"<TD NOWRAP>$date</TD>\n";
    echo"<TD NOWRAP>Rated $rating of 5</TD>\n";
    echo"<TD ALIGN=CENTER><INPUT NAME=\"selected_$i\" TYPE=\"CHECKBOX\" VALUE=\"$commentid\" TITLE=\"Selected User\"></TD>";
    echo"</TR>\n";

}

echo"<INPUT NAME=\"maxid\" TYPE=\"HIDDEN\" VALUE=\"$i\">\n";

?>
<TR>
<TD COLSPAN=4>
<h3></h3>
Found a duplicate or inappropriate comment? To Flag comments for review by Mozilla Update Staff for review, select the comment and choose "Flag Selected".<BR>
</TD>
</TR>
<TR><TD COLSPAN=4 ALIGN=RIGHT>
<?php
if ($_SESSION["level"] =="admin" or $_SESSION["level"]=="editor") {
//This user is an Admin or Editor, show the delete button.
?>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Delete Selected" ONCLICK="return confirm('Are you sure you want to delete all selected comments?');">
<?php
}
?>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Flag Selected" ONCLICK="return confirm('Are you sure you want to flag all selected comments for editor review?');">
</TD>
<TD>
</TR>
</FORM>
</TABLE>
<h3></h3>
<?php
// Begin Code for Dynamic Navbars
if ($pageid <=$num_pages) {
    $previd=$pageid-1;
    if ($previd >"0") {
        echo"<a href=\"?".uriparams()."&id=$id&page=$page&pageid=$previd\">&#171; Previous</A> &bull; ";
    }
}

echo"Page $pageid of $num_pages";
$nextid=$pageid+1;

if ($pageid <$num_pages) {
    echo" &bull; <a href=\"?".uriparams()."&id=$id&page=$page&pageid=$nextid\">Next &#187;</a>";
}
echo"<BR>\n";

//Skip to Page...
if ($num_pages>1) {
    echo"Jump to Page: ";
    $pagesperpage=9; //Plus 1 by default..
    $i = 01;

    //Dynamic Starting Point
    if ($pageid>11) {
        $nextpage=$pageid-10;
    }

    $i=$nextpage;

    //Dynamic Ending Point
    $maxpagesonpage=$pageid+$pagesperpage;
    //Page #s
    while ($i <= $maxpagesonpage && $i <= $num_pages) {
        if ($i==$pageid) { 
            echo"<SPAN style=\"color: #FF0000\">$i</SPAN>&nbsp;";
        } else {
            echo"<A HREF=\"?".uriparams()."&id=$id&page=$page&pageid=$i\">$i</A>&nbsp;";
        }
        $i++;
    }
}

if ($num_pages>1) {
    echo"<br>\nComments per page: \n";
    $perpagearray = array("25","50","100");
    foreach ($perpagearray as $items_per_page) {
       echo"<A HREF=\"?".uriparams()."&id=$id&page=$page&pageid=1\">$items_per_page</A>&nbsp;";
    }
}
?>

<?php
if ($_POST["submit"]=="Add Comment") {
echo"<a name=\"addcomment\"></a>\n";
echo"<h2>Submitting Comment, please wait...</h2>\n";


  if (checkFormKey()) {

    $id = escape_string($_POST["id"]);
    $name = escape_string($_POST["name"]);
    $title = escape_string($_POST["title"]);
    $comments = escape_string($_POST["notes"]);

    $sql = "INSERT INTO `t_feedback` (`ID`, `CommentName`, `CommentVote`, `CommentTitle`, `CommentNote`, `CommentDate`, `commentip`) VALUES ('$id', '$name', NULL, '$title', '$comments', NOW(NULL), '$_SERVER[REMOTE_ADDR]');";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if ($sql_result) {
      echo"Your comment has been added successfully...<br>\n";
    } else {
      echo"There was a problem adding your comment, please try again.<br>\n";
    }
  }



}
?>

<h2>Add Comment with No Rating</h2>
Need to make a reply comment or answer a question somebody left who didn't provide an e-mail address? Use the form below. No rating is supplied and it will not affect your item's overall rating.

<form name="addcoment" method="post" action="?id=<?php echo"$id"; ?>&action=addcomment#addcomment">
<?writeFormKey();?>
  <input name="id" type="hidden" value="<?php echo"$id"; ?>">
  <input name="name" type="hidden" value="<?php echo"$_SESSION[name]"; ?>">
  <strong>Title:</strong> <input name="title" type="text" size="30" maxlength="150" value=""><br>
  <strong>Comment:</strong><br>
  <textarea name="notes" rows=5 cols=50></textarea><br>
<input name="submit" type="submit" value="Add Comment"></SPAN>
</form>
</div>


<?php
} else if ($function=="flaggedcomments") {
?>

<?php
if ($_POST["submit"]=="Process Queue") {
echo"<h2>Processing Changes to the Flagged Comments List, please wait...</h2>\n";
    
    for ($i=1; $i<=$_POST[maxid]; $i++) {
        $action = $_POST["action_$i"];
        $commentid = escape_string($_POST["selected_$i"]); 
        if ($action=="skip") {continue;}

        if ($action=="delete") {
            $sql = "DELETE FROM `t_feedback` WHERE `CommentID`='$commentid'";
            $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
            if ($sql_result) {
                echo"Comment $commentid deleted from database.<br>\n";
            }

        } else if ($action=="clear") {
            $sql = "UPDATE `t_feedback` SET `flag`= '' WHERE `CommentID`='$commentid'";
            $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
            if ($sql_result) {
                echo"Flag cleared for comment $commentid.<br>\n";
            }

        }

    

    }



}
unset($i);
?>

<h1>Comments Flagged for Editor Review</h1>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%">
<?php
 $sql = "SELECT `CommentID`,`CommentName`,`email`,`CommentTitle`,`CommentNote`,`CommentDate`,`CommentVote`,`commentip`, TM.Name FROM `t_feedback` INNER JOIN `t_main` TM ON t_feedback.ID=TM.ID WHERE `flag`='YES' ORDER BY `CommentDate`DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $num_results = mysql_num_rows($sql_result);
    if ($num_results>"0") {
?>
<TR style="font-weight: bold">
<TH>Name/E-Mail</TH>
<TH>Date</TH>
<TH>Rating</TH>
<TH>Select</TH>
</TR>
<FORM NAME="updateusers" METHOD="POST" ACTION="?function=flaggedcomments&action=update">
<?writeFormKey();?>

<?php
    }
    while ($row = mysql_fetch_array($sql_result)) {
        $itemname = $row["Name"];
        $commentid = $row["CommentID"];
        $name = $row["CommentName"];
        $email = $row["email"];
        $title = $row["CommentTitle"];
        $notes = $row["CommentNote"];
        $date = date("l, F j Y g:i:sa", strtotime($row["CommentDate"]));
        $rating = $row["CommentVote"];
        $commentip = $row["commentip"];
        if (!$title) {$title = "No Title"; }
        if (!$name) {$name = "Anonymous"; }
        if ($rating==NULL) {$rating="N/A"; }

   
   
   $i++;
    echo"<TR><TD COLSPAN=4><h2>$i.&nbsp;&nbsp;$itemname :: $title</h2></TD></TR>\n";
    echo"<TR>\n";
    echo"<TD COLSPAN=4>$notes";
    if ($commentip) {echo"<BR>(Posted from IP: $commentip)"; }
    echo"</TD>\n";
    echo"</TR>\n";
    echo"<TR>";
    if ($email) {
        echo"<TD>Posted by <A HREF=\"mailto:$email\">$name</A></TD>\n";
    } else {
        echo"<TD>Posted by $name</TD>\n";
    }
    echo"<TD NOWRAP>$date</TD>\n";
    echo"<TD NOWRAP>Rated $rating of 5</TD>\n";
    echo"<TD>&nbsp;<INPUT NAME=\"selected_$i\" TYPE=\"hidden\" VALUE=\"$commentid\"></TD>";
    echo"</TR>\n";

    echo"<TR>\n";
    echo"<TD COLSPAN=4><input name=\"action_$i\" type=\"radio\" value=\"delete\"> Delete Comment  <input name=\"action_$i\" type=\"radio\" value=\"clear\"> Clear Flag <input name=\"action_$i\" type=\"radio\" value=\"skip\" checked> No Action</TD>\n";
    echo"</TR>\n";

}
if ($num_results>"0") {
echo"<INPUT NAME=\"maxid\" TYPE=\"HIDDEN\" VALUE=\"$i\">\n";

?>
<TR><TD COLSPAN=4 ALIGN=RIGHT>
<h3></h3>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Process Queue">&nbsp;&nbsp;<INPUT name="reset" type="reset" value="Reset Form">
</TD>
<TD>
</TR>
<?php
} else {
echo"<TR><TD COLSPAN=4 align=center>No Comments are Currently Flagged for Editor Review</TD></TR>\n";
}
?>
</FORM>
</TABLE>

<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>