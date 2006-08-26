<?php
/* SVN FILE: $Id: bootstrap.php,v 1.4 2006/08/26 03:29:13 wclouser%mozilla.com Exp $ */
/**
 * Short description for file.
 *
 * Long description for file
 *
 * PHP versions 4 and 5
 *
 * CakePHP :  Rapid Development Framework <http://www.cakephp.org/>
 * Copyright (c)	2006, Cake Software Foundation, Inc.
 *								1785 E. Sahara Avenue, Suite 490-204
 *								Las Vegas, Nevada 89104
 *
 * Licensed under The MIT License
 * Redistributions of files must retain the above copyright notice.
 *
 * @filesource
 * @copyright		Copyright (c) 2006, Cake Software Foundation, Inc.
 * @link				http://www.cakefoundation.org/projects/info/cakephp CakePHP Project
 * @package			cake
 * @subpackage		cake.app.config
 * @since			CakePHP v 0.10.8.2117
 * @version			$Revision: 1.4 $
 * @modifiedby		$LastChangedBy: phpnut $
 * @lastmodified	$Date: 2006/08/26 03:29:13 $
 * @license			http://www.opensource.org/licenses/mit-license.php The MIT License
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
 * $viewPaths = array('this path to views', 'second full path to views', 'etc...');
 * $controllerPaths = array('this path to controllers', 'second full path to controllers', 'etc...');
 *
 */

//Path defines - no trailing slashes
define('REPO_PATH', WWW_ROOT.'/files'); // XPI/JAR repository path
define('FTP_URL', 'http://ftp.mozilla.org/pub/mozilla.org'); // FTP


// Required for translating data from the database
require_once 'Translation2.php';

// Required for translating the templates (using gettext)
require_once 'language.php';

// Set's up all the gettext functions for our language (passed in $_GET).  
$language_config = new LANGUAGE_CONFIG(true);

// For other functions/classes
define('LANG', $language_config->getCurrentLanguage());

unset($language_config);

//EOF
?>
