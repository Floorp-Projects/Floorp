<?php

class Addon extends AppModel
{
    var $name = 'Addon';
    var $belongsTo = array('Addontype' =>
                        array('className'    => 'Addontype',
                              'conditions'   => '',
                              'order'        => '',
                              'dependent'    =>  false,
                              'foreignKey'   => 'addontype_id'
                        )
                  );
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
}
?>
