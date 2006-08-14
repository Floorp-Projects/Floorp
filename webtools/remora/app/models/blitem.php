<?php

class Blitem extends AppModel
{
    var $name = 'Blitem';
    var $hasMany = array('Blapp' =>
                         array('className'   => 'Blapp',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'blitem_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                         );
}
?>
