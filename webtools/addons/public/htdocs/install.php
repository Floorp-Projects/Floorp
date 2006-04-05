<?php
/**
 * Install script to log downloads.
 * @package amo
 * @subpackage docs
 */
require_once('includes.php');

$uri = mysql_real_escape_string(str_replace(" ","+",$_GET["uri"]));
$db->query("
    SELECT
        vid,
        m.id,
        uri
    FROM version v 
    INNER JOIN main m ON m.id=v.id
    WHERE 
        uri='$uri' 
    LIMIT
        1
", SQL_INIT, SQL_ASSOC);

if (empty($db->record)) {
    echo 'nope';
    exit;
} else {
    $row=$db->record;
    $uri=$row['uri']; 
    $id = $row['id'];
    $vid = $row['vid'];
}

//Are we behind a proxy and given the IP via an alternate enviroment variable? If so, use it.
if (!empty($_SERVER["HTTP_X_FORWARDED_FOR"])) {
    $remote_addr = mysql_real_escape_string($_SERVER["HTTP_X_FORWARDED_FOR"]);
} else {
    $remote_addr = mysql_real_escape_string($_SERVER["REMOTE_ADDR"]);
}

// Clean the user agent string.
$http_user_agent = mysql_real_escape_string($_SERVER['HTTP_USER_AGENT']);

// Rate limit set to 10 minutes.
$sql = "
    SELECT
        did
    FROM
        downloads
    WHERE
        id='$id' AND
        vid='$vid' AND
        user_ip='$remote_addr' AND
        user_agent='$http_user_agent' AND
        date>DATE_SUB(NOW(), INTERVAL 10 MINUTE)
    LIMIT 
        1
";
$db->query($sql,SQL_INIT,SQL_ASSOC);

if (empty($db->record)) {
    $db->query("
        INSERT INTO 
            downloads (ID, date, vID, user_ip, user_agent) 
        VALUES
            ('${id}',NOW(),'{$vid}', '{$remote_addr}', '{$http_user_agent}')
    ",SQL_NONE);
}

header('Location: '.$uri);
exit;
?>
