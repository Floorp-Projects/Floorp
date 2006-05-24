<?php
/* SVN FILE: $Id: app_model.php,v 1.1 2006/05/24 19:14:24 uid815 Exp $ */

/**
 * Application model for Cake.
 *
 * This file is application-wide model file. You can put all
 * application-wide model-related methods here.
 *
 * PHP versions 4 and 5
 *
 * CakePHP :  Rapid Development Framework <http://www.cakephp.org/>
 * Copyright (c) 2006, Cake Software Foundation, Inc.
 *                     1785 E. Sahara Avenue, Suite 490-204
 *                     Las Vegas, Nevada 89104
 *
 * Licensed under The MIT License
 * Redistributions of files must retain the above copyright notice.
 *
 * @filesource
 * @copyright    Copyright (c) 2006, Cake Software Foundation, Inc.
 * @link         http://www.cakefoundation.org/projects/info/cakephp CakePHP Project
 * @package      cake
 * @subpackage   cake.cake
 * @since        CakePHP v 0.2.9
 * @version      $Revision: 1.1 $
 * @modifiedby   $LastChangedBy: phpnut $
 * @lastmodified $Date: 2006/05/24 19:14:24 $
 * @license      http://www.opensource.org/licenses/mit-license.php The MIT License
 */

/**
 * Application model for Cake.
 *
 * Add your application-wide methods in the class below, your models
 * will inherit them.
 *
 * @package    cake
 * @subpackage cake.cake
 */

uses('sanitize');

class AppModel extends Model {

    /**
     * Will clean arrays for input into SQL.
     * Note that the array keys are getting cleaned here as well.  If you're using strings
     * (with escapable characters in them) as keys to your array, be extra careful.
     * 
     * @access public
     * @param array to be cleaned
     * @return array with sql escaped
     */
    function cleanArrayForSql($array)
    {
        $sanitize = new Sanitize();
        $clean = array();

        foreach ($array as $var => $val) 
        {
            $var = $sanitize->sql($var);
            $val = $sanitize->sql($val);
            $clean[$var] = $val;
        }

        return $clean;
    }

}

?>
