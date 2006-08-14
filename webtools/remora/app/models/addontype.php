<?php

class Addontype extends AppModel
{
    var $name = 'Addontype';
    var $belongsTo = array('Addon' =>
                         array('className'  => 'Addon',
                   'conditions' => '',
                               'order'      => '',
                               'foreignKey' => 'addontype_id'
                         )
                   );

    var $hasMany = array('Tag' =>
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
