<?php

class AddonsController extends AppController
{
    var $name = 'Addons';
    var $components = array('Rdf', 'Versioncompare');

   /**
    *
    */
    function index() {
        $this->layout = 'developers';
        $this->set('addons', $this->Addon->findall());
    }

   /**
    * Add new or new version of an add-on
    * @param int $id
    */
    function add($id = 0) {
        if (isset($this->data['Addon']['add_step1'])) {
            $this->render('add_step2');
        }
        elseif (isset($this->data['Addon']['add_step2'])) {
            $this->render('add_step3');
        }
        elseif (isset($this->data['Addon']['add_step3'])) {
            $this->render('add_step4');
        }
        else {
            $this->render('add_step1');
        }
    }

   /**
    * Manage an add-on
    * @param int $id
    */
    function manage($id) {

    }

   /**
    * Edit add-on
    * @param int $id
    */
    function edit($id) {

    }

   /**
    * Edit a version
    * @param int $id
    */
    function editVersion($id) {

    }

}

?>
