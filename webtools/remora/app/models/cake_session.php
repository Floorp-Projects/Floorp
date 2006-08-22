<?php

class Cake_Session extends AppModel
{
    var $name = 'Cake_Session';
    var $useTable = 'cake_sessions';
    var $hasOne = array('User' =>
                           array('className'  => 'User',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'cake_session_id'
                           )
                     );
}
?>
