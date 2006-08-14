<?php

class File extends AppModel
{
    var $name = 'File';
    var $belongsTo = array('Version' =>
                           array('className'  => 'Version',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'version_id'
                           ),
                           'Platform' =>
                           array('className'  => 'Platform',
                                 'conditions' => '',
                                 'order'      => '',
                                 'foreignKey' => 'platform_id'
                           )
                     );
    var $hasMany = array('Download' =>
                         array('className'   => 'Download',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'file_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                         'Approval' =>
                         array('className'   => 'Approval',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'file_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
}
?>