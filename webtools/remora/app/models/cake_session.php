<?php

class Cake_Session extends AppModel
{
    var $name = 'Cake_Session';
    var $useTable = 'cake_sessions';
    var $belongsTo = array('User' =>
                           array('className'  => 'User',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'cake_session_id'
                           )
                     );
}
?>