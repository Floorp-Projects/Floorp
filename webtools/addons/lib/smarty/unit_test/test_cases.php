<?php

require_once './config.php';
require_once SMARTY_DIR . 'Smarty.class.php';
require_once 'PHPUnit.php';

class Obj {
    var $val = 'val';
    var $arr = array('one' => 'one', 'two' => 2);
    var $ten = 10;

    function meth($a="a", $b="b") {
        return "$a:$b";
    }

    function six() {
        return 6;
    }
}

    
class SmartyTest extends PHPUnit_TestCase {   
    // contains the object handle of the string class
    var $abc;
    // contains the last triggered error's errorlevel
    var $errorlevel;

    // constructor of the test suite
    function SmartyTest($name) {
       $this->PHPUnit_TestCase($name);
    }

    // called before the test functions will be executed    
    // this function is defined in PHPUnit_TestCase and overwritten 
    // here
    function setUp() {
        // create a new instance of String with the
        // string 'abc'
        $this->smarty = new Smarty;
    }
    // called after the test functions are executed    
    // this function is defined in PHPUnit_TestCase and overwritten 
    // here    
    function tearDown() {
        // delete your instance
        unset($this->smarty);
    }
    
    // dummy errorhandler for functions that are supposed to call trigger_error()
    function error_handler($errorlevel) {
        if ($errorlevel) $this->errorlevel = $errorlevel;
    }

    /* DIRECTORY TESTS */
    
    // test that template_dir exists
    function test_template_dir_exists() {
        $this->assertTrue(file_exists($this->smarty->template_dir));                       
    }
    // test that template_dir is a directory
    function test_template_dir_is_dir() {
        $this->assertTrue(is_dir($this->smarty->template_dir));                       
    }
    // test that template_dir is readable
    function test_template_dir_is_readable() {
        $this->assertTrue(is_readable($this->smarty->template_dir));                       
    }
    // test that config_dir exists
    function test_config_dir_exists() {
        $this->assertTrue(file_exists($this->smarty->config_dir));                       
    }
    // test that config_dir is a directory
    function test_config_dir_is_dir() {
        $this->assertTrue(is_dir($this->smarty->config_dir));                       
    }
    // test that config_dir is readable
    function test_config_dir_is_readable() {
        $this->assertTrue(is_readable($this->smarty->config_dir));                       
    }
    // test that compile_dir exists
    function test_compile_dir_exists() {
        $this->assertTrue(file_exists($this->smarty->compile_dir));                       
    }
    // test that compile_dir is a directory
    function test_compile_dir_is_dir() {
        $this->assertTrue(is_dir($this->smarty->compile_dir));                       
    }
    // test that compile_dir is readable
    function test_compile_dir_is_readable() {
        $this->assertTrue(is_readable($this->smarty->compile_dir));                       
    }
    // test that compile_dir is writable
    function test_compile_dir_is_writable() {
        $this->assertTrue(is_writable($this->smarty->compile_dir));                       
    }
    // test that cache_dir exists
    function test_cache_dir_exists() {
        $this->assertTrue(file_exists($this->smarty->cache_dir));                       
    }
    // test that cache_dir is a directory
    function test_cache_dir_is_dir() {
        $this->assertTrue(is_dir($this->smarty->cache_dir));                       
    }
    // test that cache_dir is readable
    function test_cache_dir_is_readable() {
        $this->assertTrue(is_readable($this->smarty->cache_dir));                       
    }
    // test that cache_dir is writable
    function test_cache_dir_is_writable() {
        $this->assertTrue(is_writable($this->smarty->cache_dir));                       
    }

    /* METHOD EXISTS TESTS */
    function test_assign_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'assign'));
    }
    function test_assign_by_ref_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'assign_by_ref'));
    }
    function test_append_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'append'));
    }
    function test_append_by_ref_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'append_by_ref'));
    }
    function test_clear_assign_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_assign'));
    }
    function test_register_function_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_function'));
    }
    function test_unregister_function_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_function'));
    }
    function test_register_object_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_object'));
    }
    function test_unregister_object_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_object'));
    }
    function test_register_block_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_block'));
    }
    function test_unregister_block_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_block'));
    }
    function test_register_compiler_function_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_compiler_function'));
    }
    function test_unregister_compiler_function_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_compiler_function'));
    }
    function test_register_modifier_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_modifier'));
    }
    function test_unregister_modifier_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_modifier'));
    }
    function test_register_resource_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_resource'));
    }
    function test_unregister_resource_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_resource'));
    }
    function test_register_prefilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_prefilter'));
    }
    function test_unregister_prefilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_prefilter'));
    }
    function test_register_postfilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_postfilter'));
    }
    function test_unregister_postfilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_postfilter'));
    }
    function test_register_outputfilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'register_outputfilter'));
    }
    function test_unregister_outputfilter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'unregister_outputfilter'));
    }
    function test_load_filter_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'load_filter'));
    }
    function test_clear_cache_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_cache'));
    }
    function test_clear_all_cache_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_all_cache'));
    }
    function test_is_cached_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'is_cached'));
    }
    function test_clear_all_assign_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_all_assign'));
    }
    function test_clear_compiled_tpl_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_compiled_tpl'));
    }
    function test_template_exists_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'template_exists'));
    }
    function test_get_template_vars_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'get_template_vars'));
    }
    function test_get_config_vars_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'get_config_vars'));
    }
    function test_trigger_error_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'trigger_error'));
    }
    function test_display_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'display'));
    }
    function test_fetch_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'fetch'));
    }
    function test_config_load_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'config_load'));
    }
    function test_get_registered_object_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'get_registered_object'));
    }
    function test_clear_config_method_exists() {
        $this->assertTrue(method_exists($this->smarty, 'clear_config'));
    }
    function test_get_plugin_filepath() {
        $this->assertTrue(method_exists($this->smarty, '_get_plugin_filepath'));
    }

    
    function test_clear_compiled_tpl() {
        $this->assertTrue($this->smarty->clear_compiled_tpl());
    }
    
    /* DISPLAY TESTS */
    
    // test that display() executes properly
    function test_call_to_display() {
        ob_start();
        $this->smarty->display('index.tpl');
        $output = ob_get_contents();
        ob_end_clean();
        $this->assertEquals($output, 'TEST STRING');
    }

    /* FETCH TESTS */

    // test that fetch() executes properly
    function test_call_to_fetch() {
        $this->assertEquals($this->smarty->fetch('index.tpl'), 'TEST STRING');
    }
    
    /* ASSIGN TESTS */

    // test assigning a simple template variable
    function test_assign_var() {
        $this->smarty->assign('foo', 'bar');
        $this->assertEquals($this->smarty->fetch('assign_var.tpl'), 'bar');
    }

    /* PARSING TESTS */
    
    // test assigning and calling an object
    function test_parse_obj_meth() {
        $obj  = new Obj();
        $this->smarty->assign('obj', $obj);
        $this->smarty->assign('foo', 'foo');
        $this->assertEquals('foo:2.5
2.5:foo
2.5:b
val:foo
foo:val
foo:foo
one:2
foo:foo:b', $this->smarty->fetch('parse_obj_meth.tpl'));
    }
    
    // test assigning and calling an object
    function test_parse_math() {
        $obj  = new Obj();
        $this->smarty->assign('obj', $obj);
        $this->smarty->assign('flt', 2.5);
        $this->smarty->assign('items', array(1, 2));
        $this->assertEquals('3
3.5
7
11
4
4.5
8
12
12.5
25
16
20
8.5
7', $this->smarty->fetch('parse_math.tpl'));
    }
    
    /* CONFIG FILE TESTS */

    // test assigning a double quoted global variable
    function test_config_load_globals_double_quotes() {
        // load the global var
        $this->smarty->config_load('globals_double_quotes.conf');
        // test that it is assigned
        $this->assertEquals($this->smarty->_config[0]['vars']['foo'], 'bar');
    }

    // test assigning a single quoted global variable
    function test_config_load_globals_single_quotes() {
        // load the global var
        $this->smarty->config_load('globals_single_quotes.conf');
        // test that it is assigned
        $this->assertEquals($this->smarty->_config[0]['vars']['foo'], 'bar');
    }

    // test loading and running modifier.escape.php
    function test_escape_modifier_get_plugins_filepath() {
        $filepath = $this->smarty->_get_plugin_filepath('modifier', 'escape');
        $this->assertTrue($filepath);
    }

    function test_escape_modifier_include_file() {
        $filepath = $this->smarty->_get_plugin_filepath('modifier', 'escape');
        $this->assertTrue(include($filepath));
    }

    function test_escape_modifier_function_exists() {
        $this->assertTrue(function_exists('smarty_modifier_escape'));
    }

    function test_escape_modifier_escape_default() {
        $string = smarty_modifier_escape("<html><body></body></html>");
        $this->assertEquals('&lt;html&gt;&lt;body&gt;&lt;/body&gt;&lt;/html&gt;',
                            $string);
    }

    function test_escape_modifier_escape_html() {
        $string = smarty_modifier_escape("<html><body></body></html>", 'html');
        $this->assertEquals('&lt;html&gt;&lt;body&gt;&lt;/body&gt;&lt;/html&gt;',
                            $string);
    }

    function test_escape_modifier_escape_htmlall() {
        $string = smarty_modifier_escape("<html><body></body></html>", 'htmlall');
        $this->assertEquals('&lt;html&gt;&lt;body&gt;&lt;/body&gt;&lt;/html&gt;',
                            $string);
    }

    function test_escape_modifier_escape_url() {
        $string = smarty_modifier_escape("http://test.com?foo=bar", 'url');
        $this->assertEquals('http%3A%2F%2Ftest.com%3Ffoo%3Dbar', $string);
    }

    function test_escape_modifier_escape_quotes() {
        $string = smarty_modifier_escape("'\\'\\''", 'quotes');
        $this->assertEquals("\\'\\'\\'\\'", $string);
    }

    function test_escape_modifier_escape_hex() {
        $string = smarty_modifier_escape("abcd", 'hex');
        $this->assertEquals('%61%62%63%64', $string);
    }

    function test_escape_modifier_escape_hexentity() {
        $string = smarty_modifier_escape("ABCD", 'hexentity');
        $this->assertEquals('&#x41;&#x42;&#x43;&#x44;', $string);
    }

    function test_escape_modifier_escape_javascript() {
        $string = smarty_modifier_escape("\r\n\\", 'javascript');
        $this->assertEquals('\\r\\n\\\\', $string);
    }


    function test_core_is_secure_file_exists() {
        $file = SMARTY_CORE_DIR . 'core.is_secure.php';
        $this->assertTrue(file_exists($file));
    }

    function test_core_is_secure_file_include() {
        $file = SMARTY_CORE_DIR . 'core.is_secure.php';
        $this->assertTrue(include($file));
    }

    function test_core_is_secure_function_exists() {
        $this->assertTrue(function_exists('smarty_core_is_secure'));
    }

    function test_core_is_secure_function_is_secure_true() {
        $security = $this->smarty->security;
        $this->smarty->security = true;

        /* check if index.tpl is secure (should be true) */
        $params = array('resource_type' => 'file',
                        'resource_base_path' => dirname(__FILE__) . '/templates',
                        'resource_name' => dirname(__FILE__) . '/templates/index.tpl');
        $this->assertTrue(smarty_core_is_secure($params, $this->smarty));
        $this->smarty->security = $security;
    }

    function test_core_is_secure_function_is_secure_false() {
        $security = $this->smarty->security;
        $this->smarty->security = true;
        /* check if test_cases.php is secure (should be false) */
        $params = array('resource_type' => 'file',
                        'resource_base_path' => dirname(__FILE__) . '/templates',
                        'resource_name' => __FILE__);
        $this->assertFalse(smarty_core_is_secure($params, $this->smarty));
        $this->smarty->security = $security;

    }

    // test constants and security
    function test_core_is_secure_function_smarty_var_const() {
        define('TEST_CONSTANT', 'test constant');
        $this->assertEquals('test constant', $this->smarty->fetch('constant.tpl',
                                                             null, 'var_const'));
    }

    function test_core_is_secure_function_smarty_var_const_allowed() {
        $security = $this->smarty->security;
        $security_settings = $this->smarty->security_settings;
        $this->smarty->security_settings['ALLOW_CONSTANTS'] = true;
        $this->smarty->security = true;
        $this->assertEquals('test constant', $this->smarty->fetch('constant.tpl',
                                                     null, 'var_const_allowed'));
        $this->smarty->security_settings = $security_settings;
        $this->smarty->security = $security;   
    }

    function test_core_is_secure_function_smarty_var_const_not_allowed() {
        $security = $this->smarty->security;
        $this->smarty->security = true;
        /* catch errors: */
        $this->errorlevel = null;
        set_error_handler(array(&$this, 'error_handler'));
        $this->smarty->fetch('constant.tpl', null, 'var_const_not_allowed');
        restore_error_handler();

        $this->assertEquals( $this->errorlevel, E_USER_WARNING);
        $this->smarty->security = $security;
    }

}

?>
