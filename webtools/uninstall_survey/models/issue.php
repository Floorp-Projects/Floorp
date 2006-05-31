<?php
class Issue extends AppModel {
    var $name = 'Issue';

    var $hasAndBelongsToMany = array(
                                'Application' => array('className'  => 'Application'),
                                'Result'      => array('className'  => 'Result',
                                                         'joinTable' => 'issues_results',
                                                         'foreignKey'=> 'result_id',
                                                         'assocationForeignKey'=>'issue_id',
                                                        'uniq' => true
                                                        )
                               );
}
?>
