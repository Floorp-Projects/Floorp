<?php

class Addontype extends AppModel
{
    var $name = 'Addontype';
    
    var $hasMany = array('Addon' =>
                         array('className'   => 'Addon',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'addontype_id',
                               'dependent'   => false,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                         'Tag' =>
                         array('className'   => 'Tag',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'addontype_id',
                               'dependent'   => false,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
}
?>
