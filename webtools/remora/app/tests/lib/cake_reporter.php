<?php
/* SVN FILE: $Id: cake_reporter.php,v 1.1 2006/08/14 23:54:57 sancus%off.net Exp $ */
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
 * Portions modifiied from WACT Test Suite
 * Author(s): Harry Fuecks
 *            Jon Ramsey
 *            Jason E. Sweat
 *            Franco Ponticelli
 *            Lorenzo Alberton
 *
 *  Licensed under The Open Group Test Suite License
 *  Redistributions of files must retain the above copyright notice.
 *
 * @filesource
 * @author       Larry E. Masters aka PhpNut <phpnut@gmail.com>
 * @copyright    Copyright (c) 2006, Larry E. Masters Shorewood, IL. 60431
 * @link         http://www.phpnut.com/projects/
 * @package      tests
 * @subpackage   tests.libs
 * @since        CakePHP Test Suite v 1.0.0.0
 * @version      $Revision: 1.1 $
 * @modifiedby   $LastChangedBy: phpnut $
 * @lastmodified $Date: 2006/08/14 23:54:57 $
 * @license      http://www.opensource.org/licenses/opengroup.php The Open Group Test Suite License
 */
/**
 * Short description for class.
 *
 * @package    tests
 * @subpackage tests.libs
 * @since      CakePHP Test Suite v 1.0.0.0
 */
class CakeHtmlReporter extends HtmlReporter {

/**
 *    Does nothing yet. The first output will
 *    be sent on the first test start. For use
 *    by a web browser.
 *    @access public
 */
	function CakeHtmlReporter($character_set = 'ISO-8859-1') {
		parent::HtmlReporter($character_set);
	}
/**
 *    Paints the top of the web page setting the
 *    title to the name of the starting test.
 *    @param string $test_name      Name class of test.
 *    @access public
 */
	function paintHeader($test_name) {
		$this->sendNoCacheHeaders();
		print "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n";
		print "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
		print "<head>\n";
		print "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" . $this->_character_set . "\" />\n";
		print "<title>CakePHP Test Suite v 1.0.0.0 :: $test_name</title>\n";
		print "<link rel=\"stylesheet\" type=\"text/css\" href=\"/css/cake.default.css\" />\n";
		print "<style type=\"text/css\">\n";
		print $this->_getCss() . "\n";
		print "</style>\n";
		print "</head>\n<body>\n";
		print "<div id=\"wrapper\">\n";
		print "<div id=\"header\">\n";
		print "<img src=\"/img/cake.logo.png\" alt=\"\" /></div>\n";
		print "<div id=\"content\">\n";
		print "<h1>CakePHP Test Suite v 1.0.0.0</h1>\n";
		print "<h2>$test_name</h2>\n";
		flush();
	}
/**
 * Paints the end of the test with a summary of
 * the passes and failures.
 *  @param string $test_name Name class of test.
 * @access public
 *
 */
	function paintFooter($test_name) {
    	$colour = ($this->getFailCount() + $this->getExceptionCount() > 0 ? "red" : "green");
    	print "<div style=\"";
    	print "padding: 8px; margin-top: 1em; background-color: $colour; color: white;";
    	print "\">";
    	print $this->getTestCaseProgress() . "/" . $this->getTestCaseCount();
    	print " test cases complete:\n";
    	print "<strong>" . $this->getPassCount() . "</strong> passes, ";
    	print "<strong>" . $this->getFailCount() . "</strong> fails and ";
    	print "<strong>" . $this->getExceptionCount() . "</strong> exceptions.";
    	print "</div>\n";
	}
	
	function paintPass($message) {
        	parent::paintPass($message);
        	print "<span class=\"pass\">Pass</span>: ";
        	$breadcrumb = $this->getTestList();
        	array_shift($breadcrumb);
        	print implode("-&gt;", $breadcrumb);
        	print "-&gt;$message<br />\n";
    	}
}

?>
