<?php

class Reviewrating extends AppModel
{
    var $name = 'Reviewrating';
    var $belongsTo = array('User' =>
                           array('className'  => 'User',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'user_id'
                           ),
                           'Review' =>
                           array('className'  => 'Review',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'review_id'
                           )
                     );
}
?>