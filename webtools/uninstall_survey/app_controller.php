<?php
/* SVN FILE: $Id: app_controller.php,v 1.1 2006/05/24 19:14:24 uid815 Exp $ */

/**
 * Short description for file.
 *
 * This file is application-wide controller file. You can put all
 * application-wide controller-related methods here.
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
 * Short description for class.
 *
 * Add your application-wide methods in the class below, your controllers
 * will inherit them.
 *
 * @package    cake
 * @subpackage cake.cake
 */

uses('Sanitize');

class AppController extends Controller {

    /**
     * This function is intended to be used with url parameters when passing them to
     * a view.  (This is useful when echoing values out in <input> tags, etc.
     * Note that the keys to the arrays are escaped as well.
     *
     * @param array dirty parameters
     * @return array cleaned values
     */
    function decodeAndSanitize($params)
    {
        $clean = array();

        foreach ($params as $var => $val) {
            $var = $this->Sanitize->html(urldecode($var));
            $val = $this->Sanitize->html(urldecode($val));

            $clean[$var] = $val;
        }

        return $clean;
    }
}

?>
