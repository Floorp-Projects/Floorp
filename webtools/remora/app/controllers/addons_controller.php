<?php

class AddonsController extends AppController
{
    var $name = 'Addons';
    var $scaffold;

    function index()
    {
        $this->layout = 'developers';
        $this->set('addons', $this->Addon->findall());
    }
}

?>
