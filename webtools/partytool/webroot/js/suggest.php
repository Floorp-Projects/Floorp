<?php
if (empty($_REQUEST['s']))
  die();
  
include('../../vendors/webServices.php');
include('../../config/bootstrap.php');

$suggest = new webServices(array('type' => 'gsuggest'));
$string = $suggest->GSuggest($_REQUEST['s']);

echo '<?xml version="1.0"?>
';?>
<suggestion>
  <string value="<?php echo $string; ?>"/>
</suggestion>