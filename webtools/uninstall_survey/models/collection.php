<?php
class Collection extends AppModel {
    var $name = 'Collection';

    var $hasAndBelongsToMany = array(
                                'Application' => array( 'className'  => 'Application',
                                                        'joinTable' => 'applications_collections',
                                                        'foreignKey' => 'collection_id',
                                                        'associationForeignKey' => 'application_id'
                                                        ),
                                'Choice' => array('className'  => 'Choice')
                               );

}
?>
