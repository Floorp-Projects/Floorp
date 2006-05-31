<?php
class Intention extends AppModel {
    var $name = 'Intention';

    /* Rather than adding this association here, only add it when you need it.
     * Otherwise queries become very slow (since the result table is so large). Just
     * call the following before you need to join the tables:
     *      $this->bindModel(array('hasOne' => array('Result')));
     */
    //var $hasOne = array('Result');

    var $hasAndBelongsToMany = array('Application' =>
                                   array('className'  => 'Application')
                               );
}
?>
