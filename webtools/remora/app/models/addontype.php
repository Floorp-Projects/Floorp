<?php

class Addontype extends AppModel
{
    var $name = 'Addontype';
    
    var $hasOne = array('Addon' =>
                        array('className'    => 'Addon',
                              'conditions'   => '',
                              'order'        => '',
                              'dependent'    =>  false,
                              'foreignKey'   => 'addontype_id'
                        ),
                        'Tag' => 
                        array('className'    => 'Tag',
                              'conditions'   => '',
                              'order'        => '',
                              'dependent'    =>  false,
                              'foreignKey'   => 'addontype_id'
                        )
                  );
}
?>
