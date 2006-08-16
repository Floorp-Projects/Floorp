<?php
/* SVN FILE: $Id: tests_controller.php,v 1.1 2006/08/16 07:10:51 sancus%off.net Exp $ */
/**
 * Short description for file.
 *
 * Long description for file
 *
 * PHP versions 4 and 5
 *
 * CakePHP Test Suite <https://trac.cakephp.org/wiki/Developement/TestSuite>
 * Copyright (c) 2006, Larry E. Masters Shorewood, IL. 60431
 * Author(s): Larry E. Masters aka PhpNut <phpnut@gmail.com>
 *
 *  Licensed under The Open Group Test Suite License
 *  Redistributions of files must retain the above copyright notice.
 *
 * @filesource
 * @author       Larry E. Masters aka PhpNut <phpnut@gmail.com>
 * @copyright    Copyright (c) 2006, Larry E. Masters Shorewood, IL. 60431
 * @link         http://www.phpnut.com/projects/
 * @package      cake
 * @subpackage   cake.app.controllers
 * @since        CakePHP Test Suite v 1.0.0.0
 * @version      $Revision: 1.1 $
 * @modifiedby   $LastChangedBy: phpnut $
 * @lastmodified $Date: 2006/08/16 07:10:51 $
 * @license      http://www.opensource.org/licenses/opengroup.php The Open Group Test Suite License
 */
/**
 * Short description for class.
 *
 * Long description for class
 *
 * @package    cake
 * @subpackage cake.app.controllers
 */
class TestsController extends AppController{
	var $uses = null;

	function index ()
	{
		$this->layout = null;
		require_once TESTS.'tests.php';
		exit();
	}

	function groups ()
	{
		$this->layout = null;
		$_GET['show'] = 'groups';
		require_once TESTS.'tests.php';
		exit();
	}

	function cases ()
	{
		$this->layout = null;
		$_GET['show'] = 'cases';
		require_once TESTS.'tests.php';
		exit();
	}
}
?>