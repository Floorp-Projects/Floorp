<?php
/* SVN FILE: $Id: vendors.php,v 1.4 2006/10/08 03:39:24 reed%reedloden.com Exp $ */
/**
 * Short description for file.
 *
 * This file includes js vendor-files from /vendor/ directory if they need to
 * be accessible to the public.
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
 * @subpackage		cake.app.webroot.js
 * @since			CakePHP v 0.2.9
 * @version			$Revision: 1.4 $
 * @modifiedby		$LastChangedBy: phpnut $
 * @lastmodified	$Date: 2006/10/08 03:39:24 $
 * @license			http://www.opensource.org/licenses/mit-license.php The MIT License
 */
/**
 * Enter description here...
 */
$file = $_GET['file'];
$pos = strpos($file, '..');
if ($pos === false) {
	if(is_file('../../vendors/javascript/'.$file) && (preg_match('/(\/.+)\\.js/', $file)))
	{
		readfile('../../vendors/javascript/'.$file);
	}
} else {
	header('HTTP/1.1 404 Not Found');
}
?>