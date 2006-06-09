<?php
class Mirror extends AppModel
{
    var $name = 'Mirror';
    var $primaryKey = 'mirror_id';
    var $hasAndBelongsToMany = array('regions' => 
        array(  'className'                 => 'Region',
                'joinTable'                 => 'mirror_region_map',
                'foreignKey'                => 'mirror_id',
                'associationForeignKey'     => 'region_id',
                'order'                     => 'region_name desc',
                'uniq'                      => true  )
    );
}
?>
