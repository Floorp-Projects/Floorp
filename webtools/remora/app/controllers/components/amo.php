<?php
class AmoComponent extends Object {
    var $addonTypes = array();

    /**
     * Called automatically
     * @param object $controller
     */
    function startup(&$controller) {
        $this->getAddonTypes();    
    }

    /**
     * Populates addonTypes array from DB
     */
    function getAddonTypes() {
        $addontype =& new Addontype();

        $addonTypes = $addontype->findAll('', '', '', '', '', -1);
        foreach ($addonTypes as $k => $v) {
            $this->addonTypes[$addonTypes[$k]['Addontype']['id']] = $addonTypes[$k]['Addontype']['name'];
        }
    }
}
