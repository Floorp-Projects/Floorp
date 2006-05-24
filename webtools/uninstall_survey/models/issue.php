<?php
class Issue extends AppModel {
    var $name = 'Issue';

    var $hasAndBelongsToMany = array(
                                'Application' => array('className'  => 'Application'),
                                'Result'      => array('className'  => 'Result')
                               );
}
?>
