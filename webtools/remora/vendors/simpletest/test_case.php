<?php
    /**
     *	Base include file for SimpleTest
     *	@package	SimpleTest
     *	@subpackage	UnitTester
     *	@version	$Id: test_case.php,v 1.1 2006/08/27 02:06:39 shaver%mozilla.org Exp $
     */

    /**#@+
     * Includes SimpleTest files and defined the root constant
     * for dependent libraries.
     */
    require_once(dirname(__FILE__) . '/invoker.php');
    require_once(dirname(__FILE__) . '/errors.php');
    require_once(dirname(__FILE__) . '/compatibility.php');
    require_once(dirname(__FILE__) . '/scorer.php');
    require_once(dirname(__FILE__) . '/expectation.php');
    require_once(dirname(__FILE__) . '/dumper.php');
    require_once(dirname(__FILE__) . '/simpletest.php');
    if (version_compare(phpversion(), '5') >= 0) {
        require_once(dirname(__FILE__) . '/exceptions.php');
        require_once(dirname(__FILE__) . '/reflection_php5.php');
    } else {
        require_once(dirname(__FILE__) . '/reflection_php4.php');
    }
    if (! defined('SIMPLE_TEST')) {
        /**
         * @ignore
         */
        define('SIMPLE_TEST', dirname(__FILE__) . '/');
    }
    /**#@-*/

    /**
     *    Basic test case. This is the smallest unit of a test
     *    suite. It searches for
     *    all methods that start with the the string "test" and
     *    runs them. Working test cases extend this class.
	 *    @package		SimpleTest
	 *    @subpackage	UnitTester
     */
    class SimpleTestCase {
        var $_label = false;
        var $_reporter;
        var $_observers;

        /**
         *    Sets up the test with no display.
         *    @param string $label    If no test name is given then
         *                            the class name is used.
         *    @access public
         */
        function SimpleTestCase($label = false) {
            if ($label) {
                $this->_label = $label;
            }
        }

        /**
         *    Accessor for the test name for subclasses.
         *    @return string           Name of the test.
         *    @access public
         */
        function getLabel() {
            return $this->_label ? $this->_label : get_class($this);
        }

        /**
         *    Used to invoke the single tests.
         *    @return SimpleInvoker        Individual test runner.
         *    @access public
         */
        function &createInvoker() {
            $invoker = &new SimpleErrorTrappingInvoker(new SimpleInvoker($this));
            if (version_compare(phpversion(), '5') >= 0) {
                $invoker = &new SimpleExceptionTrappingInvoker($invoker);
            }
            return $invoker;
        }

        /**
         *    Uses reflection to run every method within itself
         *    starting with the string "test" unless a method
         *    is specified.
         *    @param SimpleReporter $reporter    Current test reporter.
         *    @access public
         */
        function run(&$reporter) {
            SimpleTest::setCurrent($this);
            $this->_reporter = &$reporter;
            $this->_reporter->paintCaseStart($this->getLabel());
            foreach ($this->getTests() as $method) {
                if ($this->_reporter->shouldInvoke($this->getLabel(), $method)) {
                    $invoker = &$this->_reporter->createInvoker($this->createInvoker());
                    $invoker->before($method);
                    $invoker->invoke($method);
                    $invoker->after($method);
                }
            }
            $this->_reporter->paintCaseEnd($this->getLabel());
            unset($this->_reporter);
            return $reporter->getStatus();
        }

        /**
         *    Gets a list of test names. Normally that will
         *    be all internal methods that start with the
         *    name "test". This method should be overridden
         *    if you want a different rule.
         *    @return array        List of test names.
         *    @access public
         */
        function getTests() {
            $methods = array();
            foreach (get_class_methods(get_class($this)) as $method) {
                if ($this->_isTest($method)) {
                    $methods[] = $method;
                }
            }
            return $methods;
        }

        /**
         *    Tests to see if the method is a test that should
         *    be run. Currently any method that starts with 'test'
         *    is a candidate unless it is the constructor.
         *    @param string $method        Method name to try.
         *    @return boolean              True if test method.
         *    @access protected
         */
        function _isTest($method) {
            if (strtolower(substr($method, 0, 4)) == 'test') {
                return ! SimpleTestCompatibility::isA($this, strtolower($method));
            }
            return false;
        }

        /**
         *    Announces the start of the test.
         *    @param string $method    Test method just started.
         *    @access public
         */
        function before($method) {
            $this->_reporter->paintMethodStart($method);
            $this->_observers = array();
        }

        /**
         *    Sets up unit test wide variables at the start
         *    of each test method. To be overridden in
         *    actual user test cases.
         *    @access public
         */
        function setUp() {
        }

        /**
         *    Clears the data set in the setUp() method call.
         *    To be overridden by the user in actual user test cases.
         *    @access public
         */
        function tearDown() {
        }

        /**
         *    Announces the end of the test. Includes private clean up.
         *    @param string $method    Test method just finished.
         *    @access public
         */
        function after($method) {
            for ($i = 0; $i < count($this->_observers); $i++) {
                $this->_observers[$i]->atTestEnd($method);
            }
            $this->_reporter->paintMethodEnd($method);
        }

        /**
         *    Sets up an observer for the test end.
         *    @param object $observer    Must have atTestEnd()
         *                               method.
         *    @access public
         */
        function tell(&$observer) {
            $this->_observers[] = &$observer;
        }

        /**
         *    Sends a pass event with a message.
         *    @param string $message        Message to send.
         *    @access public
         */
        function pass($message = "Pass") {
            if (! isset($this->_reporter)) {
                trigger_error('Can only make assertions within test methods');
            }
            $this->_reporter->paintPass(
                    $message . $this->getAssertionLine());
            return true;
        }

        /**
         *    Sends a fail event with a message.
         *    @param string $message        Message to send.
         *    @access public
         */
        function fail($message = "Fail") {
            if (! isset($this->_reporter)) {
                trigger_error('Can only make assertions within test methods');
            }
            $this->_reporter->paintFail(
                    $message . $this->getAssertionLine());
            return false;
        }

        /**
         *    Formats a PHP error and dispatches it to the
         *    reporter.
         *    @param integer $severity  PHP error code.
         *    @param string $message    Text of error.
         *    @param string $file       File error occoured in.
         *    @param integer $line      Line number of error.
         *    @access public
         */
        function error($severity, $message, $file, $line) {
            if (! isset($this->_reporter)) {
                trigger_error('Can only make assertions within test methods');
            }
            $this->_reporter->paintError(
                    "Unexpected PHP error [$message] severity [$severity] in [$file] line [$line]");
        }

        /**
         *    Formats an exception and dispatches it to the
         *    reporter.
         *    @param Exception $exception    Object thrown.
         *    @access public
         */
        function exception($exception) {
            $this->_reporter->paintError(
                    'Unexpected exception of type [' . get_class($exception) .
                    '] with message ['. $exception->getMessage() .
                    '] in ['. $exception->getFile() .
                    '] line [' . $exception->getLine() . ']');
        }

        /**
         *    Sends a user defined event to the test reporter.
         *    This is for small scale extension where
         *    both the test case and either the reporter or
         *    display are subclassed.
         *    @param string $type       Type of event.
         *    @param mixed $payload     Object or message to deliver.
         *    @access public
         */
        function signal($type, &$payload) {
            if (! isset($this->_reporter)) {
                trigger_error('Can only make assertions within test methods');
            }
            $this->_reporter->paintSignal($type, $payload);
        }

        /**
         *    Cancels any outstanding errors.
         *    @access public
         */
        function swallowErrors() {
            $queue = &SimpleErrorQueue::instance();
            $queue->clear();
        }

        /**
         *    Runs an expectation directly, for extending the
         *    tests with new expectation classes.
         *    @param SimpleExpectation $expectation  Expectation subclass.
         *    @param mixed $compare               Value to compare.
         *    @param string $message                 Message to display.
         *    @return boolean                        True on pass
         *    @access public
         */
        function assert(&$expectation, $compare, $message = '%s') {
            return $this->assertTrue(
                    $expectation->test($compare),
                    sprintf($message, $expectation->overlayMessage($compare)));
        }

        /**
         *	  @deprecated
         */
        function assertExpectation(&$expectation, $compare, $message = '%s') {
        	return $this->assert($expectation, $compare, $message);
        }

        /**
         *    Called from within the test methods to register
         *    passes and failures.
         *    @param boolean $result    Pass on true.
         *    @param string $message    Message to display describing
         *                              the test state.
         *    @return boolean           True on pass
         *    @access public
         */
        function assertTrue($result, $message = false) {
            if (! $message) {
                $message = 'True assertion got ' . ($result ? 'True' : 'False');
            }
            if ($result) {
                return $this->pass($message);
            } else {
                return $this->fail($message);
            }
        }

        /**
         *    Will be true on false and vice versa. False
         *    is the PHP definition of false, so that null,
         *    empty strings, zero and an empty array all count
         *    as false.
         *    @param boolean $result    Pass on false.
         *    @param string $message    Message to display.
         *    @return boolean           True on pass
         *    @access public
         */
        function assertFalse($result, $message = false) {
            if (! $message) {
                $message = 'False assertion got ' . ($result ? 'True' : 'False');
            }
            return $this->assertTrue(! $result, $message);
        }

        /**
         *    Uses a stack trace to find the line of an assertion.
         *    @param string $format    String formatting.
         *    @param array $stack      Stack frames top most first. Only
         *                             needed if not using the PHP
         *                             backtrace function.
         *    @return string           Line number of first assert*
         *                             method embedded in format string.
         *    @access public
         */
        function getAssertionLine($stack = false) {
            if ($stack === false) {
                $stack = SimpleTestCompatibility::getStackTrace();
            }
            return SimpleDumper::getFormattedAssertionLine($stack);
        }

        /**
         *    Sends a formatted dump of a variable to the
         *    test suite for those emergency debugging
         *    situations.
         *    @param mixed $variable    Variable to display.
         *    @param string $message    Message to display.
         *    @return mixed             The original variable.
         *    @access public
         */
        function dump($variable, $message = false) {
            $formatted = SimpleDumper::dump($variable);
            if ($message) {
                $formatted = $message . "\n" . $formatted;
            }
            $this->_reporter->paintFormattedMessage($formatted);
            return $variable;
        }

        /**
         *    Dispatches a text message straight to the
         *    test suite. Useful for status bar displays.
         *    @param string $message        Message to show.
         *    @access public
         */
        function sendMessage($message) {
            $this->_reporter->PaintMessage($message);
        }

        /**
         *    Accessor for the number of subtests.
         *    @return integer           Number of test cases.
         *    @access public
         *    @static
         */
        function getSize() {
            return 1;
        }
    }

    /**
     *    This is a composite test class for combining
     *    test cases and other RunnableTest classes into
     *    a group test.
	 *    @package		SimpleTest
	 *    @subpackage	UnitTester
     */
    class GroupTest {
        var $_label;
        var $_test_cases;
        var $_old_track_errors;
        var $_xdebug_is_enabled;

        /**
         *    Sets the name of the test suite.
         *    @param string $label    Name sent at the start and end
         *                            of the test.
         *    @access public
         */
        function GroupTest($label = false) {
            $this->_label = $label ? $label : get_class($this);
            $this->_test_cases = array();
            $this->_old_track_errors = ini_get('track_errors');
            $this->_xdebug_is_enabled = function_exists('xdebug_is_enabled') ?
                    xdebug_is_enabled() : false;
        }

        /**
         *    Accessor for the test name for subclasses.
         *    @return string           Name of the test.
         *    @access public
         */
        function getLabel() {
            return $this->_label;
        }

        /**
         *    Adds a test into the suite. Can be either a group
         *    test or some other unit test.
         *    @param SimpleTestCase $test_case  Suite or individual test
         *                                      case implementing the
         *                                      runnable test interface.
         *    @access public
         */
        function addTestCase(&$test_case) {
            $this->_test_cases[] = &$test_case;
        }

        /**
         *    Adds a test into the suite by class name. The class will
         *    be instantiated as needed.
         *    @param SimpleTestCase $test_case  Suite or individual test
         *                                      case implementing the
         *                                      runnable test interface.
         *    @access public
         */
        function addTestClass($class) {
            if ($this->_getBaseTestCase($class) == 'grouptest') {
                $this->_test_cases[] = &new $class();
            } else {
                $this->_test_cases[] = $class;
            }
        }

        /**
         *    Builds a group test from a library of test cases.
         *    The new group is composed into this one.
         *    @param string $test_file        File name of library with
         *                                    test case classes.
         *    @access public
         */
        function addTestFile($test_file) {
            $existing_classes = get_declared_classes();
            if ($error = $this->_requireWithError($test_file)) {
                $this->addTestCase(new BadGroupTest($test_file, $error));
                return;
            }
            $classes = $this->_selectRunnableTests($existing_classes, get_declared_classes());
            if (count($classes) == 0) {
                $this->addTestCase(new BadGroupTest($test_file, "No runnable test cases in [$test_file]"));
                return;
            }
            $group = &$this->_createGroupFromClasses($test_file, $classes);
            $this->addTestCase($group);
        }

        /**
         *    Requires a source file recording any syntax errors.
         *    @param string $file        File name to require in.
         *    @return string/boolean     An error message on failure or false
         *                               if no errors.
         *    @access private
         */
        function _requireWithError($file) {
            $this->_enableErrorReporting();
            include($file);
            $error = isset($php_errormsg) ? $php_errormsg : false;
            $this->_disableErrorReporting();
            $self_inflicted_errors = array(
                    'Assigning the return value of new by reference is deprecated',
                    'var: Deprecated. Please use the public/private/protected modifiers');
            if (in_array($error, $self_inflicted_errors)) {
                return false;
            }
            return $error;
        }

        /**
         *    Sets up detection of parse errors. Note that XDebug
         *    interferes with this and has to be disabled. This is
         *    to make sure the correct error code is returned
         *    from unattended scripts.
         *    @access private
         */
        function _enableErrorReporting() {
            if ($this->_xdebug_is_enabled) {
                xdebug_disable();
            }
            ini_set('track_errors', true);
        }

        /**
         *    Resets detection of parse errors to their old values.
         *    This is to make sure the correct error code is returned
         *    from unattended scripts.
         *    @access private
         */
        function _disableErrorReporting() {
            ini_set('track_errors', $this->_old_track_errors);
            if ($this->_xdebug_is_enabled) {
                xdebug_enable();
            }
        }

        /**
         *    Calculates the incoming test cases from a before
         *    and after list of loaded classes. Skips abstract
         *    classes.
         *    @param array $existing_classes   Classes before require().
         *    @param array $new_classes        Classes after require().
         *    @return array                    New classes which are test
         *                                     cases that shouldn't be ignored.
         *    @access private
         */
        function _selectRunnableTests($existing_classes, $new_classes) {
            $classes = array();
            foreach ($new_classes as $class) {
                if (in_array($class, $existing_classes)) {
                    continue;
                }
                if ($this->_getBaseTestCase($class)) {
                    $reflection = new SimpleReflection($class);
                    if ($reflection->isAbstract()) {
                        SimpleTest::ignore($class);
                    }
                    $classes[] = $class;
                }
            }
            return $classes;
        }

        /**
         *    Builds a group test from a class list.
         *    @param string $title       Title of new group.
         *    @param array $classes      Test classes.
         *    @return GroupTest          Group loaded with the new
         *                               test cases.
         *    @access private
         */
        function &_createGroupFromClasses($title, $classes) {
            SimpleTest::ignoreParentsIfIgnored($classes);
            $group = &new GroupTest($title);
            foreach ($classes as $class) {
                if (! SimpleTest::isIgnored($class)) {
                    $group->addTestClass($class);
                }
            }
            return $group;
        }

        /**
         *    Test to see if a class is derived from the
         *    SimpleTestCase class.
         *    @param string $class     Class name.
         *    @access private
         */
        function _getBaseTestCase($class) {
            while ($class = get_parent_class($class)) {
                $class = strtolower($class);
                if ($class == "simpletestcase" || $class == "grouptest") {
                    return $class;
                }
            }
            return false;
        }

        /**
         *    Delegates to a visiting collector to add test
         *    files.
         *    @param string $path                  Path to scan from.
         *    @param SimpleCollector $collector    Directory scanner.
         *    @access public
         */
        function collect($path, &$collector) {
            $collector->collect($this, $path);
        }

        /**
         *    Invokes run() on all of the held test cases, instantiating
         *    them if necessary.
         *    @param SimpleReporter $reporter    Current test reporter.
         *    @access public
         */
        function run(&$reporter) {
            $reporter->paintGroupStart($this->getLabel(), $this->getSize());
            for ($i = 0, $count = count($this->_test_cases); $i < $count; $i++) {
                if (is_string($this->_test_cases[$i])) {
                    $class = $this->_test_cases[$i];
                    $test = &new $class();
                    $test->run($reporter);
                } else {
                    $this->_test_cases[$i]->run($reporter);
                }
            }
            $reporter->paintGroupEnd($this->getLabel());
            return $reporter->getStatus();
        }

        /**
         *    Number of contained test cases.
         *    @return integer     Total count of cases in the group.
         *    @access public
         */
        function getSize() {
            $count = 0;
            foreach ($this->_test_cases as $case) {
                if (is_string($case)) {
                    $count++;
                } else {
                    $count += $case->getSize();
                }
            }
            return $count;
        }
    }

    /**
     *    This is a failing group test for when a test suite hasn't
     *    loaded properly.
	 *    @package		SimpleTest
	 *    @subpackage	UnitTester
     */
    class BadGroupTest {
        var $_label;
        var $_error;

        /**
         *    Sets the name of the test suite and error message.
         *    @param string $label    Name sent at the start and end
         *                            of the test.
         *    @access public
         */
        function BadGroupTest($label, $error) {
            $this->_label = $label;
            $this->_error = $error;
        }

        /**
         *    Accessor for the test name for subclasses.
         *    @return string           Name of the test.
         *    @access public
         */
        function getLabel() {
            return $this->_label;
        }

        /**
         *    Sends a single error to the reporter.
         *    @param SimpleReporter $reporter    Current test reporter.
         *    @access public
         */
        function run(&$reporter) {
            $reporter->paintGroupStart($this->getLabel(), $this->getSize());
            $reporter->paintFail('Bad GroupTest [' . $this->getLabel() .
                    '] with error [' . $this->_error . ']');
            $reporter->paintGroupEnd($this->getLabel());
            return $reporter->getStatus();
        }

        /**
         *    Number of contained test cases. Always zero.
         *    @return integer     Total count of cases in the group.
         *    @access public
         */
        function getSize() {
            return 0;
        }
    }
?>
