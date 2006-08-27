<?php
    /**
     *	base include file for SimpleTest
     *	@package	SimpleTest
     *	@subpackage	UnitTester
     *	@version	$Id: exceptions.php,v 1.1 2006/08/27 02:06:39 shaver%mozilla.org Exp $
     */

    /**#@+
     * Includes SimpleTest files and defined the root constant
     * for dependent libraries.
     */
    require_once(dirname(__FILE__) . '/invoker.php');

    /**
     *    Extension that traps exceptions and turns them into
     *    an error message.
	 *	  @package SimpleTest
	 *	  @subpackage UnitTester
     */
    class SimpleExceptionTrappingInvoker extends SimpleInvokerDecorator {

        /**
         *    Stores the invoker to be wrapped.
         *    @param SimpleInvoker $invoker   Test method runner.
         */
        function SimpleExceptionTrappingInvoker($invoker) {
            $this->SimpleInvokerDecorator($invoker);
        }

        /**
         *    Invokes a test method and dispatches any
         *    untrapped errors.
         *    @param string $method    Test method to call.
         *    @access public
         */
        function invoke($method) {
            try {
                parent::invoke($method);
            } catch (Exception $exception) {
                $test_case = &$this->getTestCase();
                $test_case->exception($exception);
            }
        }
    }
?>