<?php
class Intention extends AppModel {
    var $name = 'Intention';

    var $hasOne = array('Result');

    var $hasAndBelongsToMany = array('Application' =>
                                   array('className'  => 'Application')
                               );

}
?>
