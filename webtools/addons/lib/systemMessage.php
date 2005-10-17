<?php
/**
 * systemMessage function -- just a snippet
 * @todo incorporate into OO, using $db->query to pull messages (if they exist and are flagged as active)
 * @todo add db tables to SQL schema
 */

#define PAGE_TYPE_FRONT_PAGE        1
#define PAGE_TYPE_DEVELOPER_PAGE    2
#define PAGE_TYPE_LIST_PAGE         4
#define PAGE_TYPE_DETAIL_PAGE       8

// these values can be done as a DB table so they can be controlled and edited by site admins without access to the webserver itself
function getSystemMessageData() {
  $messageData['header'] = 'Want to get involved?';
  $messageData['text'] = 'We are looking for volunteers to help us with UMO. We are in need of PHP developers to help with redesigning the site, and people to review extensions and themes that get submitted to UMO. We especially need Mac and Thunderbird users. If you are interested in being a part of this exciting project, please send information such as your full name, timezone and experience to <a href="mailto:umo-reviewer@mozilla.org?subject=Review%20Application%20for%20[Name Here]">umo-reviewer@mozilla.org</a>. Also, please join us in #umo on irc.mozilla.org to start getting a feeling for what\'s up or for a more informal chat.';
  $messageData['pageType'] = 1;

  return $messageData;
}

function showSystemMessage($pageType) {
  $messageData = getSystemMessageData();
  if ($messageData['pageType'] & $pageType) {
    $smarty->assign("systemMessage", $messageData);
  }
}

// USAGE:
// showSystemMessage(PAGE_TYPE_FRONT_PAGE); should be somewhere in the script, with the 
// appropriate page type

?>
