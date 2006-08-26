<?php
/* SVN FILE: $Id: app_controller.php,v 1.1 2006/08/26 03:30:20 wclouser%mozilla.com Exp $ */

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
 * @lastmodified $Date: 2006/08/26 03:30:20 $
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


class AppController extends Controller {

    var $translation;

    function AppController()
    {
        parent::Controller();

        // This connection info is hardcoded to:
            // 1) The 'default' cake connection
            // 2) MySQL
        $cm =& new ConnectionManager();
        $db = $cm->getDataSource('default');
        $db_user = $db->config['login'];
        $db_pass = $db->config['password'];
        $db_host = $db->config['host'];
        $db_name = $db->config['database'];
        $dsn = "mysql://$db_user:$db_pass@$db_host/$db_name";

        $params = array(
            'langs_avail_table'      => 'langs',
            'lang_id_col'            => 'id',
            'lang_name_col'          => 'name',
            'lang_meta_col'          => 'meta',
            'lang_errmsg_col'        => 'error_text',
            'string_id_col'          => 'translated_column',
            'string_page_id_col'     => 'pk_column',
            'string_text_col'        => '%s',  //'%s' will be replaced by the lang code
            'strings_default_table'  => 'translations'
            /*
                If we ever split the language into separate tables, we can get rid of
                'strings_default_table' and replace with:

                    'strings_tables'  => array(
                                            'en_US' => 'en_US_table',
                                            'fr' => 'fr_table',
                                            'de' => 'de_table'
                                         ),
            */

        );
        
        $this->translation =& Translation2::factory('DB', $dsn, $params);

        if (PEAR::isError($this->translation)) {
            // What do we wan't to do here?  If this is broken, we're going to have
            // major problems.

            //die($this->translation->getMessage());
        }

        // LANG is set in bootstrap.php
        $this->translation->setLang(LANG);

        // Get all the translations for our page.  If we ever get a global set, we
        // can get that here as well
        $lang = $this->translation->getPage($this->name);

        // give lang to view
        $this->set("lang", $lang);
    }

}

?>
