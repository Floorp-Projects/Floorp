<?php

class Review extends AppModel
{
    var $name = 'Review';
    var $belongsTo = array('Version' =>
                         array('className'  => 'Version',
                               'conditions' => '',
                               'order'      => '',
                               'foreignKey' => 'version_id'
                         )
                   );
    var $hasMany = array('ReviewRating' =>
                         array('className'   => 'ReviewRating',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'review_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
}
?>
