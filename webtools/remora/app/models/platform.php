<?php

class Platform extends AppModel
{
    var $name = 'Platform';
    var $hasMany = array('File' =>
                         array('className'   => 'File',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'platform_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
}
?>