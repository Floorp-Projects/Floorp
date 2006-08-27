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

   /**
    * Checks if user has permissions for an addon
    * @param int $id
    */
    function checkOwnership($id) {
        /* to be re-visited after we implement sessions and ACL
        if (user is admin) {
            return true;
        }
        if (user is an author) {
            return true;
        }
        */

        return true;
    }
}
