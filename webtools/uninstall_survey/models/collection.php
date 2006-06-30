<?php
class Collection extends AppModel {
    var $name = 'Collection';

    var $hasAndBelongsToMany = array(
                                'Application' => array('className'  => 'Application'),
                                'Choice' => array('className'  => 'Choice')
                               );

}
?>
