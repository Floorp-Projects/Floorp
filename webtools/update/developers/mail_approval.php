<?php
// This message is called to inform the user of action being taken on a queued item listing.

//--- Send via E-Mail Message ---

$from_name = "Mozilla Update";
$from_address = "update-daemon@mozilla.org";


//Send To Address
//Get E-Mail Addresses of the Authors of this item.

$sql = "SELECT `email` FROM `authorxref` TAX INNER JOIN `userprofiles` TU ON TAX.UserID=TU.UserID WHERE TAX.ID='$id'";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {


    $to_address = $row["email"];

    $headers .= "MIME-Version: 1.0\r\n";
    $headers .= "Content-type: text/plain; charset=iso-8859-1\r\n";
    $headers .= "From: ".$from_name." <".$from_address.">\r\n";
    $headers .= "X-Priority: 3\r\n";
    $headers .= "X-MSMail-Priority: Normal\r\n";
    $headers .= "X-Mailer: Mozilla Update Mail System 1.0";

    $subject = "[$name $version] $action_email \n";

	$message = "$name $version - $action_email\n";
    $message .= "Your item, $name $version, has been reviewed by a Mozilla Update editor who took the following action:\n";
    $message .= "$action_email\n\n";
    $message .= "Your item was tested by the editor using $testbuild on $testos.\n";
    $message .= "Editor's Comments:\n $comments\n";
    $message .= "----\n";
    $message .= "Mozilla Update: https://$sitehostname/\n";

    mail($to_address, $subject, $message, $headers);

}
?>