<?php

class User extends AppModel
{
    var $name = 'User';
    var $hasAndBelongsToMany = array('Addon' =>
                                     array('className'  => 'Addon',
                                           'joinTable'  => 'addons_users',
                                           'foreignKey' => 'addon_id',
                                           'associationForeignKey'=> 'user_id',
                                           'conditions' => '',
                                           'order'      => '',
                                           'limit'      => '',
                                           'unique'     => false,
                                           'finderSql'  => '',
                                           'deleteQuery'=> '',
                               )
                               );
    var $belongsTo = array('Cake_Session');
    var $hasMany = array('Approval' =>
                         array('className'   => 'Approval',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'user_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                         'Reviewrating' =>
                         array('className'   => 'Reviewrating',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'user_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                         );
}
?>
