<?php
//From the change password function of usermanager
// This message is called to inform the user of an administrative reset of their password.

//--- Send via E-Mail Message ---

$from_name = "Mozilla Update";
$from_address = "update-daemon@update.mozilla.org";


//Send To Address
$to_address = "$email";

$headers .= "MIME-Version: 1.0\r\n";
$headers .= "Content-type: text/plain; charset=iso-8859-1\r\n";
$headers .= "From: ".$from_name." <".$from_address.">\r\n";
//$headers .= "Reply-To: ".$from_name." <".$from_address.">\r\n";
$headers .= "X-Priority: 3\r\n";
$headers .= "X-MSMail-Priority: Normal\r\n";
$headers .= "X-Mailer: Mozilla Update Mail System 1.0";

$subject = "Your New Mozilla Update Password\n";

	$message = "Your New Mozilla Update Password\n";
    $message .= "Below is your new Mozilla Update password which has been either changed by you using the Change Password tool, or regenerated as requested by you using the Mozilla Update Forgotten Password tool or by an Mozilla Update Staff member per your request.\n";
    $message .= "To login to your account, click the link below or copy and paste the whole thing into your browsers location bar:\n";
    $message .= "http://$_SERVER[HTTP_HOST]/developers/\n\n";
    $message .= "Keep this e-mail in a safe-place for your records, below is your account details you used when registering for your account.\n\n";
	$message .= "E-Mail: $email\n";
    $message .= "Password: $password_plain\n\n";
    $message .= "Thanks,\n";
    $message .= "-- Mozilla Update Staff\n";

mail($to_address, $subject, $message, $headers);

?>