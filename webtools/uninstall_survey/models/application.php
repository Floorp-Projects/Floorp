<?php
class Application extends AppModel {
    var $name = 'Application';

    var $hasOne = array('Result');

    var $hasAndBelongsToMany = array(
                                'Intention' => array('className'  => 'Intention'),
                                'Issue' => array('className'  => 'Issue')
                               );

    /**
     * This was added because running findAll() on this model does a left join on the
     * results table which takes around 10 seconds to grab all the data.  All I want
     * is a list of the applications...
     *
     * @return array rows representing each application
     */
    function getApplications()
    {
        return $this->query('SELECT * FROM `applications` ORDER BY `id`');
    }
}
?>
