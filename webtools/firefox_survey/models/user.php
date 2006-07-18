<?php

class User extends AppModel {
    var $name = 'User';

    var $validate = array(
    // Names are optional
        //'firstname' =>VALID_NOT_EMPTY,
        //'lastname' =>VALID_NOT_EMPTY,
        'email' =>VALID_EMAIL
        );
}

?>
