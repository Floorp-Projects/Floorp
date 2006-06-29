<?php
/* SVN FILE: $Id: bootstrap.php,v 1.2 2006/06/29 21:37:53 wclouser%mozilla.com Exp $ */

/**
 * Short description for file.
 *
 * Long description for file
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
 * @subpackage   cake.app.config
 * @since        CakePHP v 0.10.8.2117
 * @version      $Revision: 1.2 $
 * @modifiedby   $LastChangedBy: phpnut $
 * @lastmodified $Date: 2006/06/29 21:37:53 $
 * @license      http://www.opensource.org/licenses/mit-license.php The MIT License
 */

/**
 *
 * This file is loaded automatically by the app/webroot/index.php file after the core bootstrap.php is loaded
 * This is an application wide file to load any function that is not used within a class define.
 * You can also use this to include or require any files in your application.
 *
 */

/**
 * The settings below can be used to set additional paths to models, views and controllers.
 * This is related to Ticket #470 (https://trac.cakephp.org/ticket/470)
 *
 * $modelPaths = array('full path to models', 'second full path to models', 'etc...');
 * $viewPaths  = array('this path to views', 'second full path to views', 'etc...');
 * $controllerPaths  = array('this path to controllers', 'second full path to controllers', 'etc...');
 *
 */

 /**
  * Default application.  If there isn't something in $_GET we'll fall back on this.
  * 1) Make sure this thing exists in the database
  * 2) This isn't escaped at all when used in SQL queries.  If a single quote appears
  * in an application name in the future and you need to use it here, be sure to
  * escape it.
  */
 define('DEFAULT_APP_NAME','Mozilla Firefox');

 define('DEFAULT_APP_VERSION','1.5');

 /**
  * We are adding applications dynamically based on the URL.  If a version of firefox
  * comes in that we haven't dealt with yet, we want to ask a default set of
  * questions (rather than just a comment box).  This is that default set.  Same
  * rules apply as the default application above.
  */
 define('DEFAULT_FIREFOX_INTENTION_SET_ID','2');
 define('DEFAULT_FIREFOX_ISSUE_SET_ID','1');
 define('DEFAULT_THUNDERBIRD_INTENTION_SET_ID','4');
 define('DEFAULT_THUNDERBIRD_ISSUE_SET_ID','3');

//EOF
?>
