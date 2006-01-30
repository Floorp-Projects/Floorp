<?php
/**
 * User class.  Contains user methods and metadata.
 * @package amo
 * @subpackage docs
 */
class User extends AMO_Object
{
    // User metadata.
    var $UserID;
    var $UserEmail;
    var $UserWebsite;
    var $UserMode;
    var $UserTrusted;
    var $UserEmailHide;
    var $UserLastLogin;
    var $ConfirmationCode;

    // Addons metadata.
    var $AddOns;

    /**
     * Constructor.
     *
     * @param int $UserID UserID
     */
    function User($UserID=null)
    {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;

        // If $ID is set, attempt to retrieve data.
        if (!empty($UserID)) {
            $this->setVar('UserID',$UserID);
            $this->getUser();
            $this->getAddons();
        }
    }

    /**
     * Get user information from singular record.
     */
    function getUser()
    {
        $this->db->query("
            SELECT
                UserName,
                UserEmail,
                UserWebsite,
                UserMode,
                UserTrusted,
                UserEmailHide,
                UserLastLogin,
                ConfirmationCode
            FROM
                userprofiles
            WHERE
                UserID = {$this->UserID}
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }
    }

    /**
     * Get addons for this author.
     */
    function getAddons()
    {
        // Gather addons metadata, user info.
        $this->db->query("
            SELECT 
                main.ID,
                main.Name,
                main.Description
            FROM 
                main 
            INNER JOIN authorxref ON authorxref.ID = main.ID
            INNER JOIN userprofiles ON userprofiles.UserID = authorxref.UserID
            WHERE 
                userprofiles.UserID = '{$this->UserID}'
        ", SQL_ALL, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVar('AddOns',$this->db->record);
        }
    }

    /**
     * A static method for adding a new user.
     */
    function addUser($info) 
    {
        global $db;//this is a static function
        $_email    = mysql_real_escape_string($info['email']);
        $_name     = mysql_real_escape_string($info['name']);
        $_website  = mysql_real_escape_string($info['website']);
        $_password = mysql_real_escape_string($info['password']);
        $_confirmation_code = md5(mt_rand());//just some string

        $_sql = "INSERT INTO `userprofiles` 
                    (   `UserName`,
                        `UserEmail`,
                        `UserWebsite`,
                        `UserPass`,
                        `UserMode`,
                        `ConfirmationCode`
                    ) VALUES (
                        '{$_name}',
                        '{$_email}',
                        '{$_website}',
                        MD5('{$_password}'),
                        'D',
                        '{$_confirmation_code}'
                    )";
        if ($db->query($_sql)) {
            // Our db class has some severe shortcomings :(
            $_sql = 'SELECT LAST_INSERT_ID()';
            if ($db->query($_sql, SQL_INIT)) {
                return $db->record;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    /**
     * A convenience function for new accounts
     * @param string plaintext password to send to user
     * @access public
     */
    function sendConfirmation($pass)
    {
        $subject  = "Activate your new Mozilla Update account\n";

        $message  = "Welcome to Mozilla Update.\n";
        $message .= "Before you can use your new account you must activate it, this ensures the e-mail address you used is valid and belongs to you.\n";
        $message .= "To activate your account, click the link below or copy and paste the whole thing into your browsers location bar:\n";
        $message .= HTTP_HOST.WEB_PATH.'/verifyaccount.php?email='.urlencode($this->UserEmail).'&confirmationcode='.$this->ConfirmationCode."\n\n";
        $message .= "Keep this e-mail in a safe-place for your records, below is your account details you used when registering for your account.\n\n";
        $message .= "E-Mail: {$this->UserEmail}\n";
        $message .= "Password: {$pass}\n\n";
        $message .= "Thanks for joining Mozilla Update\n";
        $message .= "-- Mozilla Update Staff\n";

        $this->sendMail($subject, $message);
    }

    /**
     * Will send an email to the current user
     * @param string subject
     * @param string message
     * @access public
     */
    function sendMail($subject, $message) 
    {
        $_to = $this->UserEmail;

        $_from_name    = "Mozilla Update";
        $_from_address = "update-daemon@mozilla.org";

        $_headers  = "MIME-Version: 1.0\r\n";
        $_headers .= "Content-type: text/plain; charset=iso-8859-1\r\n";
        $_headers .= "From: ".$_from_name." <".$_from_address.">\r\n";
        //$_headers .= "Reply-To: ".$from_name." <".$from_address.">\r\n";
        $_headers .= "X-Priority: 3\r\n";
        $_headers .= "X-MSMail-Priority: Normal\r\n";
        $_headers .= "X-Mailer: UMO Mail System 1.0";

        mail($_to, $subject, $message, $_headers);
    }

    /** 
     * A static function to get a user by email address.  Really, I wrote it so I could see
     * if an email address was already in use.
     * @param string address to check
     * @return mixed A user object, or false on failure
     * @access public
     */
    function getUserByEmail($address) 
    {
        global $db;//static function
        $_email = mysql_real_escape_string($address);

        $_sql = "SELECT 
                    `UserID`
                 FROM
                    `userprofiles`
                 WHERE
                    `UserEmail`='{$_email}'
                 LIMIT 1";
        $db->query($_sql, SQL_INIT, SQL_ASSOC);

        $ret = $db->record;

        if (is_array($ret) && array_key_exists('UserID', $ret) && is_numeric($ret['UserID'])) {
            return new User($ret['UserID']);
        } else {
            return false;
        }
    }

    /**
     * Will flip a user from 'D' to 'U' (confirm the account)
     * @access public
     * @param string the confirmation code emailed to the user
     * @return bool true on success, false on failure
     */
    function confirm($code) 
    {
        $_code = mysql_real_escape_string($code);

        $_sql = "UPDATE
                    `userprofiles`
                 SET
                    `UserMode`='U',
                    `ConfirmationCode` = ''
                 WHERE
                    `UserEmail`='{$this->UserEmail}'
                 AND
                    `ConfirmationCode` = '{$_code}'
                 LIMIT 1";
        $this->db->query($_sql);

        // Workaround for sql library :-/
        // This will refresh the usermode
        $this->getUser();

        if ($this->UserMode == 'U') {
            return true;
        } else {
            return false;
        }
    }

    /**
     * Will generate and put a confirmation code into a user's row
     * @access public
     */
    function generateConfirmationCode()
    {
        $_code = md5(mt_rand());//just some string

        $_sql = "UPDATE
                    `userprofiles`
                 SET
                    `ConfirmationCode` = '{$_code}'
                 WHERE
                    `UserID`='{$this->UserID}'
                 LIMIT 1";

        $this->db->query($_sql);

        // refresh the confirmation code
        $this->getUser();

        return true;
    }

    /**
     * Will send a password recovery email to the currently logged in user
     * @access public
     */
    function sendPasswordRecoveryEmail()
    {
        $_email = urlencode($this->UserEmail);
        $_code  = urlencode($this->ConfirmationCode);

        $subject = "Mozilla Addons Password Reset\n";

        $message = "Mozilla Addons Password Reset\n";
        $message .= "\n";
        $message .= "A request was recieved to reset the password for this\n";
        $message .= "account on http://addons.mozilla.org/.  To change the password\n";
        $message .= "please click on the following link, or paste it into your browser:\n";
        $message .= HTTP_HOST.WEB_PATH."/resetpassword.php?email={$_email}&code={$_code}\n";
        $message .= "\n";
        $message .= "If you did not request this email there is no need for further action.\n";
        $message .= "Thanks,\n";
        $message .= "-- Mozilla Update Staff\n";

        $this->sendMail($subject,$message);

        return true;
    }

    /**
     * Checks whether a given code is valid to reset a password.
     * @access public
     * @param string email address
     * @param string code reset code
     * @return boolean true or false (duh, I guess)
     */
    function checkResetPasswordCode($email, $code) 
    {
        $_email = mysql_real_escape_string($email);
        $_code  = mysql_real_escape_string($code);

        $_sql = "SELECT
                    `UserID`
                 FROM
                    `userprofiles`
                 WHERE
                    `UserEmail`='{$_email}'
                 AND
                    `ConfirmationCode` = '{$_code}'
                 LIMIT 1";

        $this->db->query($_sql, SQL_INIT, SQL_ASSOC);

        $ret = $this->db->record;

        if (is_array($ret) && array_key_exists('UserID', $ret) && is_numeric($ret['UserID'])) {
            return true;
        } else {
            return false;
        }
    }

    /** 
     * Will simply reset the password for a user
     * @param string password to reset to
     * @access public
     */
    function setPassword($password)
    {
        $_password = mysql_real_escape_string($password);
        $_sql = "UPDATE
                    `userprofiles`
                 SET
                    `UserPass` = MD5('{$_password}')
                 WHERE
                    `UserID`='{$this->UserID}'
                 LIMIT 1";

        $this->db->query($_sql);
    }
}
?>
