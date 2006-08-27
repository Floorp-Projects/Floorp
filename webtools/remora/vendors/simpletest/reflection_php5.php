<?php
    /**
     *	base include file for SimpleTest
     *	@package	SimpleTest
     *	@subpackage	UnitTester
     *	@version	$Id: reflection_php5.php,v 1.1 2006/08/27 02:06:39 shaver%mozilla.org Exp $
     */

    /**
     *    Version specific reflection API.
	 *    @package SimpleTest
	 *    @subpackage UnitTester
     */
    class SimpleReflection {
        var $_interface;

        /**
         *    Stashes the class/interface.
         *    @param string $interface    Class or interface
         *                                to inspect.
         */
        function SimpleReflection($interface) {
            $this->_interface = $interface;
        }

        /**
         *    Checks that a class has been declared. Versions
         *    before PHP5.0.2 need a check that it's not really
         *    an interface.
         *    @return boolean            True if defined.
         *    @access public
         */
        function classExists() {
            if (! class_exists($this->_interface)) {
                return false;
            }
            $reflection = new ReflectionClass($this->_interface);
            return ! $reflection->isInterface();
        }

        /**
         *    Needed to kill the autoload feature in PHP5
         *    for classes created dynamically.
         *    @return boolean        True if defined.
         *    @access public
         */
        function classExistsSansAutoload() {
            return class_exists($this->_interface, false);
        }

        /**
         *    Checks that a class or interface has been
         *    declared.
         *    @return boolean            True if defined.
         *    @access public
         */
        function classOrInterfaceExists() {
            return $this->_classOrInterfaceExistsWithAutoload($this->_interface, true);
        }

        /**
         *    Needed to kill the autoload feature in PHP5
         *    for classes created dynamically.
         *    @return boolean        True if defined.
         *    @access public
         */
        function classOrInterfaceExistsSansAutoload() {
            return $this->_classOrInterfaceExistsWithAutoload($this->_interface, false);
        }

        /**
         *    Needed to select the autoload feature in PHP5
         *    for classes created dynamically.
         *    @param string $interface       Class or interface name.
         *    @param boolean $autoload       True totriggerautoload.
         *    @return boolean                True if interface defined.
         *    @access private
         */
        function _classOrInterfaceExistsWithAutoload($interface, $autoload) {
            if (function_exists('interface_exists')) {
                if (interface_exists($this->_interface, $autoload)) {
                    return true;
                }
            }
            return class_exists($this->_interface, $autoload);
        }

        /**
         *    Gets the list of methods on a class or
         *    interface. Needs to recursively look at all of
         *    the interfaces included.
         *    @returns array              List of method names.
         *    @access public
         */
        function getMethods() {
            return array_unique(get_class_methods($this->_interface));
        }

        /**
         *    Gets the list of interfaces from a class. If the
         *    class name is actually an interface then just that
         *    interface is returned.
         *    @returns array          List of interfaces.
         *    @access public
         */
        function getInterfaces() {
            $reflection = new ReflectionClass($this->_interface);
            if ($reflection->isInterface()) {
            	return array($this->_interface);
            }
            return $this->_onlyParents($reflection->getInterfaces());
        }

        /**
         *    Gets the list of methods for the implemented
         *    interfaces only.
         *    @returns array      List of enforced method signatures.
         *    @access public
         */
        function getInterfaceMethods() {
            $methods = array();
            foreach ($this->getInterfaces() as $interface) {
                $methods = array_merge($methods, get_class_methods($interface));
            }
            return array_unique($methods);
        }
        
        /**
         *    Checks to see if the method signature has to be tightly
         *    specified.
         *    @param string $method        Method name.
         *    @returns boolean             True if enforced.
         *    @access private
         */
        function _isInterfaceMethod($method) {
            return in_array($method, $this->getInterfaceMethods());
        }

        /**
         *    Finds the parent class name.
         *    @returns string      Parent class name.
         *    @access public
         */
        function getParent() {
            $reflection = new ReflectionClass($this->_interface);
            $parent = $reflection->getParentClass();
            if ($parent) {
                return $parent->getName();
            }
            return false;
        }

        /**
         *    Determines if the class is abstract.
         *    @returns boolean      True if abstract.
         *    @access public
         */
        function isAbstract() {
            $reflection = new ReflectionClass($this->_interface);
            return $reflection->isAbstract();
        }

        /**
         *    Wittles a list of interfaces down to only the top
         *    level parents.
         *    @param array $interfaces     Reflection API interfaces
         *                                 to reduce.
         *    @returns array               List of parent interface names.
         *    @access private
         */
        function _onlyParents($interfaces) {
            $parents = array();
            foreach ($interfaces as $interface) {
                foreach($interfaces as $possible_parent) {
                    if ($interface->getName() == $possible_parent->getName()) {
                        continue;
                    }
                    if ($interface->isSubClassOf($possible_parent)) {
                        break;
                    }
                }
                $parents[] = $interface->getName();
            }
            return $parents;
        }

        /**
         *    Gets the source code matching the declaration
         *    of a method.
         *    @param string $name    Method name.
         *    @return string         Method signature up to last
         *                           bracket.
         *    @access public
         */
        function getSignature($name) {
        	if ($name == '__get') {
        		return 'function __get($key)';
        	}
        	if ($name == '__set') {
        		return 'function __set($key, $value)';
        	}
        	if (! is_callable(array($this->_interface, $name))) {
        		return "function $name()";
        	}
        	if ($this->_isInterfaceMethod($name)) {
        	    return $this->_getFullSignature($name);
        	}
        	return "function $name()";
        }
        
        /**
         *    For a signature specified in an interface, full
         *    details must be replicated to be a valid implementation.
         *    @param string $name    Method name.
         *    @return string         Method signature up to last
         *                           bracket.
         *    @access private
         */
        function _getFullSignature($name) {
	        $interface = new ReflectionClass($this->_interface);
	        $method = $interface->getMethod($name);
	        $reference = $method->returnsReference() ? '&' : '';
        	return "function $reference$name(" .
            		implode(', ', $this->_getParameterSignatures($method)) .
            		")";
        }

        /**
         *    Gets the source code for each parameter.
         *    @param ReflectionMethod $method   Method object from
         *										reflection API
         *    @return array                     List of strings, each
         *                                      a snippet of code.
         *    @access private
         */
        function _getParameterSignatures($method) {
        	$signatures = array();
            foreach ($method->getParameters() as $parameter) {
                $type = $parameter->getClass();
            	$signatures[] =
					(! is_null($type) ? $type->getName() . ' ' : '') .
            			($parameter->isPassedByReference() ? '&' : '') .
            			'$' . $this->_suppressSpurious($parameter->getName()) .
            			($this->_isOptional($parameter) ? ' = null' : '');
            }
            return $signatures;
        }

        /**
         *    The SPL library has problems with the
         *    Reflection library. In particular, you can
         *    get extra characters in parameter names :(.
         *    @param string $name    Parameter name.
         *    @return string         Cleaner name.
         *    @access private
         */
        function _suppressSpurious($name) {
            return str_replace(array('[', ']', ' '), '', $name);
        }

        /**
         *    Test of a reflection parameter being optional
         *    that works with early versions of PHP5.
         *    @param reflectionParameter $parameter    Is this optional.
         *    @return boolean                          True if optional.
         *    @access private
         */
        function _isOptional($parameter) {
            if (method_exists($parameter, 'isOptional')) {
                return $parameter->isOptional();
            }
            return false;
        }
    }
?>