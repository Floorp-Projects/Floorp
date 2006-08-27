<?php
    /**
     *	Global state for SimpleTest and kicker script in future versions.
     *	@package	SimpleTest
     *	@subpackage	UnitTester
     *	@version	$Id: simpletest.php,v 1.1 2006/08/27 02:06:39 shaver%mozilla.org Exp $
     */

    /**#@+
     * include SimpleTest files
     */
    if (version_compare(phpversion(), '5') >= 0) {
        require_once(dirname(__FILE__) . '/reflection_php5.php');
    } else {
        require_once(dirname(__FILE__) . '/reflection_php4.php');
    }
    /**#@-*/

    /**
     *    Static global directives and options. I hate this
     *    class. It's a mixture of reference hacks, configuration
     *    and previous design screw-ups that I have to maintain
     *    to keep backward compatibility.
     *	  @package	SimpleTest
     */
    class SimpleTest {

        /**
         *    Reads the SimpleTest version from the release file.
         *    @return string        Version string.
         *    @static
         *    @access public
         */
        function getVersion() {
            $content = file(dirname(__FILE__) . '/VERSION');
            return trim($content[0]);
        }

        /**
         *    Sets the name of a test case to ignore, usually
         *    because the class is an abstract case that should
         *    not be run. Once PHP4 is dropped this will disappear
         *    as a public method and "abstract" will rule.
         *    @param string $class        Add a class to ignore.
         *    @static
         *    @access public
         */
        function ignore($class) {
            $registry = &SimpleTest::_getRegistry();
            $registry['IgnoreList'][strtolower($class)] = true;
        }

        /**
         *    Scans the now complete ignore list, and adds
         *    all parent classes to the list. If a class
         *    is not a runnable test case, then it's parents
         *    wouldn't be either. This is syntactic sugar
         *    to cut down on ommissions of ignore()'s or
         *    missing abstract declarations. This cannot
         *    be done whilst loading classes wiithout forcing
         *    a particular order on the class declarations and
         *    the ignore() calls. It's nice to havethe ignore()
         *    calls at the top of teh file.
         *    @param array $classes     Class names of interest.
         *    @static
         *    @access public
         */
        function ignoreParentsIfIgnored($classes) {
            $registry = &SimpleTest::_getRegistry();
            foreach ($classes as $class) {
                if (SimpleTest::isIgnored($class)) {
                    $reflection = new SimpleReflection($class);
                    if ($parent = $reflection->getParent()) {
                        SimpleTest::ignore($parent);
                    }
                }
            }
        }

        /**
         *    Test to see if a test case is in the ignore
         *    list. Quite obviously the ignore list should
         *    be a separate object and will be one day.
         *    This method is internal to SimpleTest. Don't
         *    use it.
         *    @param string $class        Class name to test.
         *    @return boolean             True if should not be run.
         *    @access public
         *    @static
         */
        function isIgnored($class) {
            $registry = &SimpleTest::_getRegistry();
            return isset($registry['IgnoreList'][strtolower($class)]);
        }

        /**
         *    @deprecated
         */
        function setMockBaseClass($mock_base) {
            $registry = &SimpleTest::_getRegistry();
            $registry['MockBaseClass'] = $mock_base;
        }

        /**
         *    @deprecated
         */
        function getMockBaseClass() {
            $registry = &SimpleTest::_getRegistry();
            return $registry['MockBaseClass'];
        }

        /**
         *    Sets proxy to use on all requests for when
         *    testing from behind a firewall. Set host
         *    to false to disable. This will take effect
         *    if there are no other proxy settings.
         *    @param string $proxy     Proxy host as URL.
         *    @param string $username  Proxy username for authentication.
         *    @param string $password  Proxy password for authentication.
         *    @access public
         */
        function useProxy($proxy, $username = false, $password = false) {
            $registry = &SimpleTest::_getRegistry();
            $registry['DefaultProxy'] = $proxy;
            $registry['DefaultProxyUsername'] = $username;
            $registry['DefaultProxyPassword'] = $password;
        }

        /**
         *    Accessor for default proxy host.
         *    @return string       Proxy URL.
         *    @access public
         */
        function getDefaultProxy() {
            $registry = &SimpleTest::_getRegistry();
            return $registry['DefaultProxy'];
        }

        /**
         *    Accessor for default proxy username.
         *    @return string    Proxy username for authentication.
         *    @access public
         */
        function getDefaultProxyUsername() {
            $registry = &SimpleTest::_getRegistry();
            return $registry['DefaultProxyUsername'];
        }

        /**
         *    Accessor for default proxy password.
         *    @return string    Proxy password for authentication.
         *    @access public
         */
        function getDefaultProxyPassword() {
            $registry = &SimpleTest::_getRegistry();
            return $registry['DefaultProxyPassword'];
        }

        /**
         *    Sets the current test case instance. This
         *    global instance can be used by the mock objects
         *    to send message to the test cases.
         *    @param SimpleTestCase $test        Test case to register.
         *    @access public
         *    @static
         */
        function setCurrent(&$test) {
            $registry = &SimpleTest::_getRegistry();
            $registry['CurrentTestCase'] = &$test;
        }

        /**
         *    Accessor for current test instance.
         *    @return SimpleTEstCase        Currently running test.
         *    @access public
         *    @static
         */
        function &getCurrent() {
            $registry = &SimpleTest::_getRegistry();
            return $registry['CurrentTestCase'];
        }

        /**
         *    Accessor for global registry of options.
         *    @return hash           All stored values.
         *    @access private
         *    @static
         */
        function &_getRegistry() {
            static $registry = false;
            if (! $registry) {
                $registry = SimpleTest::_getDefaults();
            }
            return $registry;
        }

        /**
         *    Constant default values.
         *    @return hash       All registry defaults.
         *    @access private
         *    @static
         */
        function _getDefaults() {
            return array(
                    'StubBaseClass' => 'SimpleStub',
                    'MockBaseClass' => 'SimpleMock',
                    'IgnoreList' => array(),
                    'DefaultProxy' => false,
                    'DefaultProxyUsername' => false,
                    'DefaultProxyPassword' => false);
        }
    }

    /**
     *    @deprecated
     */
    class SimpleTestOptions extends SimpleTest {

        /**
         *    @deprecated
         */
        function getVersion() {
            return Simpletest::getVersion();
        }

        /**
         *    @deprecated
         */
        function ignore($class) {
            return Simpletest::ignore($class);
        }

        /**
         *    @deprecated
         */
        function isIgnored($class) {
            return Simpletest::isIgnored($class);
        }

        /**
         *    @deprecated
         */
        function setMockBaseClass($mock_base) {
            return Simpletest::setMockBaseClass($mock_base);
        }

        /**
         *    @deprecated
         */
        function getMockBaseClass() {
            return Simpletest::getMockBaseClass();
        }

        /**
         *    @deprecated
         */
        function useProxy($proxy, $username = false, $password = false) {
            return Simpletest::useProxy($proxy, $username, $password);
        }

        /**
         *    @deprecated
         */
        function getDefaultProxy() {
            return Simpletest::getDefaultProxy();
        }

        /**
         *    @deprecated
         */
        function getDefaultProxyUsername() {
            return Simpletest::getDefaultProxyUsername();
        }

        /**
         *    @deprecated
         */
        function getDefaultProxyPassword() {
            return Simpletest::getDefaultProxyPassword();
        }
    }
?>