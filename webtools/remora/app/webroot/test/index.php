<?php
/* SVN FILE: $Id: index.php,v 1.2 2006/08/16 04:03:12 sancus%off.net Exp $ */

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
 * @since        CakePHP Test Suite v 1.0.0.0
 * @version      $Revision: 1.2 $
 * @modifiedby   $LastChangedBy: phpnut $
 * @lastmodified $Date: 2006/08/16 04:03:12 $
 * @license      http://www.opensource.org/licenses/opengroup.php The Open Group Test Suite License
 */

error_reporting(E_ALL);
set_time_limit(600);
ini_set('memory_limit','128M');
class CakeDummyTestClass
{

}

if (!defined('DS'))
{
    define('DS', DIRECTORY_SEPARATOR);
}

if (!defined('ROOT'))
{
//define('ROOT', 'FULL PATH TO DIRECTORY WHERE APP DIRECTORY IS LOCATED DO NOT ADD A TRAILING DIRECTORY SEPARATOR';
    define('ROOT', dirname(dirname(dirname(dirname(__FILE__)))).DS);
}

if (!defined('APP_DIR'))
{
//define('APP_DIR', 'DIRECTORY NAME OF APPLICATION';
    define ('APP_DIR', basename(dirname(dirname(dirname(__FILE__)))).DS);
    //define('APP_DIR', 'remora'.DS);
}

/**
 * This only needs to be changed if the cake installed libs are located
 * outside of the distributed directory structure.
 */
if (!defined('CAKE_CORE_INCLUDE_PATH'))
{
//define ('CAKE_CORE_INCLUDE_PATH', FULL PATH TO DIRECTORY WHERE CAKE CORE IS INSTALLED DO NOT ADD A TRAILING DIRECTORY SEPARATOR';
    define('CAKE_CORE_INCLUDE_PATH', ROOT);
}


if (!defined('WEBROOT_DIR'))
{
    define ('WEBROOT_DIR', basename(dirname(dirname(__FILE__))));
}

define('WWW_ROOT', dirname(dirname(__FILE__)));

if (function_exists('ini_set')) {
                ini_set('include_path', ini_get('include_path') . PATH_SEPARATOR . CAKE_CORE_INCLUDE_PATH . PATH_SEPARATOR . ROOT . DS . APP_DIR . DS);
                define('APP_PATH', null);
                define('CORE_PATH', null);
        } else {
                define('APP_PATH', ROOT . DS . APP_DIR . DS);
                define('CORE_PATH', CAKE_CORE_INCLUDE_PATH . DS);
        }

ini_set('include_path',ini_get('include_path').PATH_SEPARATOR.CAKE_CORE_INCLUDE_PATH.PATH_SEPARATOR.ROOT.DS.APP_DIR.DS);


    require_once 'cake'.DS.'basics.php';
    require_once 'cake'.DS.'config'.DS.'paths.php';
    require_once TESTS . 'test_paths.php';
    require_once TESTS . 'lib'.DS.'test_manager.php';
    vendor('simpletest'.DS.'reporter');

    if (!isset(  $_SERVER['SERVER_NAME'] ))
    {
        $_SERVER['SERVER_NAME'] = '';
    }

    if (empty( $_GET['output']))
    {
        TestManager::setOutputFromIni(TESTS . 'config.ini.php');
        $_GET['output'] = TEST_OUTPUT;
    }

/**
 *
 * Used to determine output to display
 */
define('CAKE_TEST_OUTPUT_HTML',1);
define('CAKE_TEST_OUTPUT_XML',2);
define('CAKE_TEST_OUTPUT_TEXT',3);

    if ( isset($_GET['output']) && $_GET['output'] == 'xml' )
    {
        define('CAKE_TEST_OUTPUT', CAKE_TEST_OUTPUT_XML);
    }
    elseif  ( isset($_GET['output']) && $_GET['output'] == 'html' )
    {
        define('CAKE_TEST_OUTPUT', CAKE_TEST_OUTPUT_HTML);
    }
    else
    {
        define('CAKE_TEST_OUTPUT', CAKE_TEST_OUTPUT_TEXT);
    }

    function & CakeTestsGetReporter()
    {
        static $Reporter = NULL;
        if ( !$Reporter )
        {
            switch ( CAKE_TEST_OUTPUT )
            {
                case CAKE_TEST_OUTPUT_XML:
                    vendor('simpletest'.DS.'xml');
                    $Reporter = new XmlReporter();
                break;

                case CAKE_TEST_OUTPUT_HTML:
                    require_once TESTS . 'lib'.DS.'cake_reporter.php';
                    $Reporter = new CakeHtmlReporter();
                break;

                default:
                    $Reporter = new TextReporter();
                break;
            }
        }
        return $Reporter;
    }

    function CakePHPTestRunMore()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
            break;
            case CAKE_TEST_OUTPUT_HTML:
                $link = !class_exists('CakeDummyTestClass')
                    ?   "<p><a href='/tests/'>Run more tests</a></p>\n"
                    :
                        "<p><a href='" . $_SERVER['PHP_SELF'] . "'>Run more tests</a></p>\n";
                echo $link;
            break;

            case CAKE_TEST_OUTPUT_TEXT:
            default:
            break;
        }
    }

    function CakePHPTestCaseList()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
                if (isset($_GET['app']))
                {
                    echo XmlTestManager::getTestCaseList(APP_TEST_CASES);
                }
                else
                {
                    echo XmlTestManager::getTestCaseList(CORE_TEST_CASES);
                }
            break;

            case CAKE_TEST_OUTPUT_HTML:
                if (isset($_GET['app']))
                {
                    echo HtmlTestManager::getTestCaseList(APP_TEST_CASES);
                }
                else
                {
                    echo HtmlTestManager::getTestCaseList(CORE_TEST_CASES);
                }
            break;

            case CAKE_TEST_OUTPUT_TEXT:
            default:
                if (isset($_GET['app']))
                {
                    echo TextTestManager::getTestCaseList(APP_TEST_CASES);
                }
                else
                {
                    echo TextTestManager::getTestCaseList(CORE_TEST_CASES);
                }
            break;
    }
}

    function CakePHPTestGroupTestList()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
                if (isset($_GET['app']))
                {
                    echo XmlTestManager::getGroupTestList(APP_TEST_GROUPS);
                }
                else
                {
                    echo XmlTestManager::getGroupTestList(CORE_TEST_GROUPS);
                }
            break;

            case CAKE_TEST_OUTPUT_HTML:
                if (isset($_GET['app']))
                {
                    echo HtmlTestManager::getGroupTestList(APP_TEST_GROUPS);
                }
                else
                {
                    echo HtmlTestManager::getGroupTestList(CORE_TEST_GROUPS);
                }
            break;

            case CAKE_TEST_OUTPUT_TEXT:
            default:
                if (isset($_GET['app']))
                {
                    echo TextTestManager::getGroupTestList(APP_TEST_GROUPS);
                }
                else
                {
                    echo TextTestManager::getGroupTestList(CORE_TEST_GROUPS);
                }
            break;
        }
    }

    function CakePHPTestHeader()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
                header(' content-Type: text/xml; charset="utf-8"');
            break;
            case CAKE_TEST_OUTPUT_HTML:
        $header = <<<EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
    <html xmlns="http://www.w3.org/1999/xhtml">
        <head>
            <meta http-equiv=' content-Type' content='text/html; charset=iso-8859-1' />
            <title>CakePHP Test Suite v 1.0.0.0</title>
            <link rel="stylesheet" type="text/css" href="/css/cake.default.css" />
        </head>
        <body>

EOF;
                echo $header;
            break;
            case CAKE_TEST_OUTPUT_TEXT:
            default:
                header(' content-type: text/plain');
            break;
        }
    }

    function CakePHPTestSuiteHeader()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
            break;

            case CAKE_TEST_OUTPUT_HTML:
                $groups = !class_exists('CakeDummyTestClass') ? 'groups' : $_SERVER['PHP_SELF'].'?show=groups';
                $cases = !class_exists('CakeDummyTestClass') ? 'cases' : $_SERVER['PHP_SELF'].'?show=cases';
                $suiteHeader = <<<EOD

<div id="wrapper">
    <div id="header">
        <img src="/img/cake.logo.png" alt="" />

    </div>
    <div id="content">
    <h1>CakePHP Test Suite v 1.0.0.0</h1>
	<p><a href='$groups'>Core Test Groups</a>  ||   <a href='$cases'>Core Test Cases</a></p>
	<p><a href='$groups&amp;app=true'>App Test Groups</a>  ||   <a href='$cases&amp;app=true'>App Test Cases</a></p>
EOD;
            echo $suiteHeader;
            break;

            case CAKE_TEST_OUTPUT_TEXT:
            default:
            break;
        }
    }


    function CakePHPTestSuiteFooter()
    {
        switch ( CAKE_TEST_OUTPUT )
        {
            case CAKE_TEST_OUTPUT_XML:
            break;

            case CAKE_TEST_OUTPUT_HTML:
                $footer = <<<EOD
  </div>
<div id="footer">
<p>
<a href="https://trac.cakephp.org/wiki/Developement/TestSuite">CakePHP Test Suite </a> ::
<a href="http://www.phpnut.com/projects/"> &copy; 2006, Larry E. Masters.</a>
</p>
<br />
 <p>
<!--PLEASE USE ONE OF THE POWERED BY CAKEPHP LOGO-->
    <a href="http://www.cakephp.org/" target="_new">
    <img src="/img/cake.power.png" alt="CakePHP :: Rapid Development Framework" height = "15" width = "80" />
    </a>
    </p>
  <p>
    <a href="http://validator.w3.org/check?uri=referer">
    <img src="/img/w3c_xhtml10.png" alt="Valid XHTML 1.0 Transitional" height = "15" width = "80" />
    </a>
    <a href="http://jigsaw.w3.org/css-validator/check/referer">
    <img src="/img/w3c_css.png" alt="Valid CSS!" height = "15" width = "80" />
    </a>
  </p>
</div>
</div>
</body>
</html>
EOD;
                echo $footer;
            break;

            case CAKE_TEST_OUTPUT_TEXT:
            default:
            break;
        }
    }


if (isset($_GET['group']))
{
    if ('all' == $_GET['group'])
    {
        TestManager::runAllTests(CakeTestsGetReporter());
    }
    else
    {
        if (isset($_GET['app']))
        {
        TestManager::runGroupTest(ucfirst($_GET['group']),
                                  APP_TEST_GROUPS,
                                  CakeTestsGetReporter());
        }
        else
        {
        TestManager::runGroupTest(ucfirst($_GET['group']),
                                  CORE_TEST_GROUPS,
                                  CakeTestsGetReporter());
        }
    }

    CakePHPTestRunMore();
    CakePHPTestSuiteFooter();
    exit();
}

if (isset($_GET['case']))
{
    TestManager::runTestCase($_GET['case'], CakeTestsGetReporter());
    CakePHPTestRunMore();
    CakePHPTestSuiteFooter();
    exit();
}

CakePHPTestHeader();
CakePHPTestSuiteHeader();

if (isset($_GET['show']) && $_GET['show'] == 'cases')
{
    CakePHPTestCaseList();
}
else
{
    CakePHPTestGroupTestList();
}
CakePHPTestSuiteFooter();

?>
