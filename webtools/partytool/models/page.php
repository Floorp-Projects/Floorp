<?php
class Page extends AppModel {
  var $name = 'Page';
  var $useTable = 'parties';
  
  function getUsers() {
    $rv = $this->query("SELECT COUNT(*) FROM users");
    return $rv[0][0]["COUNT(*)"];
  }
}
?>