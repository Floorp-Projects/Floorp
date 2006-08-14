<?php

class Version extends AppModel
{
    var $name = 'Version';
    var $belongsTo = array('Addon' =>
                         array('className'  => 'Addon',
                               'conditions' => '',
                               'order'      => '',
                               'foreignKey' => 'addon_id'
                         )
                   );
  var $hasMany = array('Review' =>
                         array('className'   => 'Review',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'version_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                       'File' =>
                         array('className'   => 'File',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'version_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
    var $hasAndBelongsToMany = array('Application' =>
                                      array('className'  => 'Application',
                                            'joinTable'  => 'applications_versions',
                                            'foreignKey' => 'application_id',
                                            'associationForeignKey'=> 'version_id',
                                            'conditions' => '',
                                            'order'      => '',
                                            'limit'      => '',
                                            'unique'     => false,
                                            'finderSql'  => '',
                                            'deleteQuery'=> '',
                                      ),
                                      );
}
?>
