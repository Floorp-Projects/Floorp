<?php

//--- Send Request via E-Mail Message ---

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

$subject = "Activate your new Mozilla Update account\n";

	$message = "Welcome to Mozilla Update.\n";
    $message .= "Before you can use your new account you must activate it, this ensures the e-mail address you used is valid and belongs to you.\n";
    $message .= "To activate your account, click the link below or copy and paste the whole thing into your browsers location bar:\n";
    $message .= "http://$_SERVER[HTTP_HOST]/developers/createaccount.php?function=confirmaccount&email=$email&confirmationcode=$confirmationcode\n\n";
    $message .= "Keep this e-mail in a safe-place for your records, below is your account details you used when registering for your account.\n\n";
	$message .= "E-Mail: $email\n";
    $message .= "Password: $password_plain\n\n";
    $message .= "Thanks for joining Mozilla Update\n";
    $message .= "-- Mozilla Update Staff\n";

mail($to_address, $subject, $message, $headers);

?>