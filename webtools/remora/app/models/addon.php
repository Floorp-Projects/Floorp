<?php

class Addon extends AppModel
{
    var $name = 'Addon';
    var $belongsTo = array('Addontype');
    var $hasMany = array('Version' =>
                         array('className'   => 'Version',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'addon_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                         'Preview' =>
                         array('className'   => 'Preview',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'addon_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         ),
                         'Feature' =>
                         array('classname'   => 'Feature',
                               'conditions'  => '',
                               'order'       => '',
                               'limit'       => '',
                               'foreignKey'  => 'addon_id',
                               'dependent'   => true,
                               'exclusive'   => false,
                               'finderSql'   => ''
                         )
                  );
    var $hasAndBelongsToMany = array('User' =>
                                      array('className'  => 'User',
                                            'joinTable'  => 'addons_users',
                                            'foreignKey' => 'addon_id',
                                            'associationForeignKey'=> 'user_id',
                                            'conditions' => '',
                                            'order'      => '',
                                            'limit'      => '',
                                            'unique'     => false,
                                            'finderSql'  => '',
                                            'deleteQuery'=> '',
                                      ),
                                     'Tag' =>
                                       array('className'  => 'Tag',
                                            'joinTable'  => 'addons_tags',
                                            'foreignKey' => 'addon_id',
                                            'associationForeignKey'=> 'tag_id',
                                            'conditions' => '',
                                            'order'      => '',
                                            'limit'      => '',
                                            'unique'     => false,
                                            'finderSql'  => '',
                                            'deleteQuery'=> '',
                                      )

                               );
    var $validate = array(
        'guid' => VALID_NOT_EMPTY,
        'name' => VALID_NOT_EMPTY,
        'description' => VALID_NOT_EMPTY,
        'addontype_id' => VALID_NUMBER
    );
}
?>
