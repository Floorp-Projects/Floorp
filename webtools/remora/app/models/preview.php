<?php

class Preview extends AppModel
{
    var $name = 'Preview';
    var $belongsTo = array('Preview' =>
                         array('className'  => 'Preview',
                               'conditions' => '',
                               'order'      => '',
                               'foreignKey' => 'addon_id'
                         )
                   );

}
?>
