<?php
class Downloadable extends AppModel {
   var $name = 'Downloadable';

   var $belongsTo = array('File','Mirror');
}
?>
