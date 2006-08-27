<?php
    // $Id: reflection_php5_test.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $

    abstract class AnyOldClass {
        function aMethod() { }
    }

    interface AnyOldInterface {
        function aMethod();
    }

    interface AnyOldArgumentInterface {
        function aMethod(AnyOldInterface $argument);
    }

    interface AnyDescendentInterface extends AnyOldInterface {
    }

    class AnyOldImplementation implements AnyOldInterface {
    	function aMethod() { }
    	function extraMethod() { }
    }

    abstract class AnyAbstractImplementation implements AnyOldInterface {
    }

    class AnyOldSubclass extends AnyOldImplementation { }

	class AnyOldArgumentClass {
		function aMethod($argument) { }
	}

	class AnyOldArgumentImplementation implements AnyOldArgumentInterface {
		function aMethod(AnyOldInterface $argument) { }
	}

	class AnyOldTypeHintedClass implements AnyOldArgumentInterface {
		function aMethod(AnyOldInterface $argument) { }
	}

    class TestOfReflection extends UnitTestCase {

        function testClassExistence() {
            $reflection = new SimpleReflection('AnyOldClass');
            $this->assertTrue($reflection->classOrInterfaceExists());
            $this->assertTrue($reflection->classOrInterfaceExistsSansAutoload());
        }

        function testClassNonExistence() {
            $reflection = new SimpleReflection('UnknownThing');
            $this->assertFalse($reflection->classOrInterfaceExists());
            $this->assertFalse($reflection->classOrInterfaceExistsSansAutoload());
        }

        function testDetectionOfAbstractClass() {
            $reflection = new SimpleReflection('AnyOldClass');
            $this->assertTrue($reflection->isAbstract());
        }

        function testFindingParentClass() {
            $reflection = new SimpleReflection('AnyOldSubclass');
            $this->assertEqual($reflection->getParent(), 'AnyOldImplementation');
        }

        function testInterfaceExistence() {
            $reflection = new SimpleReflection('AnyOldInterface');
            $this->assertTrue(
            		$reflection->classOrInterfaceExists());
            $this->assertTrue(
            		$reflection->classOrInterfaceExistsSansAutoload());
        }

        function testMethodsListFromClass() {
            $reflection = new SimpleReflection('AnyOldClass');
            $this->assertIdentical($reflection->getMethods(), array('aMethod'));
        }

        function testMethodsListFromInterface() {
            $reflection = new SimpleReflection('AnyOldInterface');
            $this->assertIdentical($reflection->getMethods(), array('aMethod'));
            $this->assertIdentical($reflection->getInterfaceMethods(), array('aMethod'));
        }

        function testMethodsComeFromDescendentInterfacesASWell() {
            $reflection = new SimpleReflection('AnyDescendentInterface');
            $this->assertIdentical($reflection->getMethods(), array('aMethod'));
        }
        
        function testCanSeparateInterfaceMethodsFromOthers() {
            $reflection = new SimpleReflection('AnyOldImplementation');
            $this->assertIdentical($reflection->getMethods(), array('aMethod', 'extraMethod'));
            $this->assertIdentical($reflection->getInterfaceMethods(), array('aMethod'));
        }

        function testMethodsComeFromDescendentInterfacesInAbstractClass() {
            $reflection = new SimpleReflection('AnyAbstractImplementation');
            $this->assertIdentical($reflection->getMethods(), array('aMethod'));
        }

        function testInterfaceHasOnlyItselfToImplement() {
            $reflection = new SimpleReflection('AnyOldInterface');
        	$this->assertEqual(
        			$reflection->getInterfaces(),
        			array('AnyOldInterface'));
        }

        function testInterfacesListedForClass() {
            $reflection = new SimpleReflection('AnyOldImplementation');
        	$this->assertEqual(
        			$reflection->getInterfaces(),
        			array('AnyOldInterface'));
        }

        function testInterfacesListedForSubclass() {
            $reflection = new SimpleReflection('AnyOldSubclass');
        	$this->assertEqual(
        			$reflection->getInterfaces(),
        			array('AnyOldInterface'));
        }

		function testNoParameterCreationWhenNoInterface() {
			$reflection = new SimpleReflection('AnyOldArgumentClass');
			$function = $reflection->getSignature('aMethod');
			if (version_compare(phpversion(), '5.0.2', '<=')) {
			    $this->assertEqual('function amethod()', strtolower($function));
    	    } else {
			    $this->assertEqual('function aMethod()', $function);
    	    }
		}

		function testParameterCreationWithoutTypeHinting() {
			$reflection = new SimpleReflection('AnyOldArgumentImplementation');
			$function = $reflection->getSignature('aMethod');
			if (version_compare(phpversion(), '5.0.2', '<=')) {
			    $this->assertEqual('function amethod(AnyOldInterface $argument)', $function);
    	    } else {
			    $this->assertEqual('function aMethod(AnyOldInterface $argument)', $function);
    	    }
		}

		function testParameterCreationForTypeHinting() {
			$reflection = new SimpleReflection('AnyOldTypeHintedClass');
			$function = $reflection->getSignature('aMethod');
			if (version_compare(phpversion(), '5.0.2', '<=')) {
			    $this->assertEqual('function amethod(AnyOldInterface $argument)', $function);
    	    } else {
			    $this->assertEqual('function aMethod(AnyOldInterface $argument)', $function);
    	    }
		}
    }
?>