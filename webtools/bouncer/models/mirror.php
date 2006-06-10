<?php
class Mirror extends AppModel
{
    var $name = 'Mirror';
    var $hasAndBelongsToMany = array('regions' => 
        array(  'className'                 => 'Region',
                'joinTable'                 => 'mirror_regions',
                'foreignKey'                => 'mirror_id',
                'associationForeignKey'     => 'region_id',
                'order'                     => 'regions.name desc',
                'uniq'                      => true  )
    );
}
?>
