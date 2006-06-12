<?php
class File extends AppModel {
   var $name = 'File';

   var $belongsTo = array('Application', 'Platform', 'Locale', 'Template');
}
?>
