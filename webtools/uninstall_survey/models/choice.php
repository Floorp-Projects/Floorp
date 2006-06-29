<?php
class Choice extends AppModel {
    var $name = 'Choice';

    var $hasAndBelongsToMany = array(
                                'Result'      => array('className'  => 'Result', 'uniq' => true ),
                                'Collection'      => array('className'  => 'Collection', 'uniq' => true )
                               );
}
?>
