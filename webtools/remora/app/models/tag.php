<?php

class Tag extends AppModel
{
    var $name = 'Tag';
    var $hasAndBelongsToMany = array('Addon' =>
                                     array('className'  => 'Addon',
                                           'joinTable'  => 'addons_tags',
                                           'foreignKey' => 'addon_id',
                                           'associationForeignKey'=> 'tag_id',
                                           'conditions' => '',
                                           'order'      => '',
                                           'limit'      => '',
                                           'unique'     => false,
                                           'finderSql'  => '',
                                           'deleteQuery'=> '',
                                     )
                               );
    var $belongsTo = array('Addontype', 'Application');
}
?>
