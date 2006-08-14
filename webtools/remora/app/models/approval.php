<?php

class Approval extends AppModel
{
    var $name = 'Approval';
    var $belongsTo = array('User' =>
                           array('className'  => 'User',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'user_id'
                           ),
                           'File' =>
                           array('className'  => 'File',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'file_id'
                           )
                     );
}
?>