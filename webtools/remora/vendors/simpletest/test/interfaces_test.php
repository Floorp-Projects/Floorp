<?php
    // $Id: interfaces_test.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $
    if (function_exists('spl_classes')) {
        include(dirname(__FILE__) . '/support/spl_examples.php');
    }

    interface DummyInterface {
        function aMethod();
        function anotherMethod($a);
        function &referenceMethod(&$a);
    }

    Mock::generate('DummyInterface');
    Mock::generatePartial('DummyInterface', 'PartialDummyInterface', array());

    class TestOfMockInterfaces extends UnitTestCase {

        function testCanMockAnInterface() {
            $mock = new MockDummyInterface();
            $this->assertIsA($mock, 'SimpleMock');
            $this->assertIsA($mock, 'MockDummyInterface');
            $this->assertTrue(method_exists($mock, 'aMethod'));
            $this->assertTrue(method_exists($mock, 'anotherMethod'));
            $this->assertNull($mock->aMethod());
        }

        function testMockedInterfaceExpectsParameters() {
            $mock = new MockDummyInterface();
            $mock->anotherMethod();
            $this->assertError();
        }

        function testCannotPartiallyMockAnInterface() {
            $this->assertFalse(class_exists('PartialDummyInterface'));
        }
    }

    class TestOfSpl extends UnitTestCase {

        function testCanMockAllSplClasses() {
            if (! function_exists('spl_classes')) {
                return;
            }
            foreach(spl_classes() as $class) {
                $mock_class = "Mock$class";
                Mock::generate($class);
                $this->assertIsA(new $mock_class(), $mock_class);
            }
        }

        function testExtensionOfCommonSplClasses() {
            if (! function_exists('spl_classes')) {
                return;
            }
            Mock::generate('IteratorImplementation');
            $this->assertIsA(
                    new IteratorImplementation(),
                    'IteratorImplementation');
            Mock::generate('IteratorAggregateImplementation');
            $this->assertIsA(
                    new IteratorAggregateImplementation(),
                    'IteratorAggregateImplementation');
       }
    }

    class WithHint {
        function hinted(DummyInterface $object) { }
    }

    class ImplementsDummy implements DummyInterface {
        function aMethod() { }
        function anotherMethod($a) { }
        function &referenceMethod(&$a) { }
        function extraMethod($a = false) { }
    }

    Mock::generate('ImplementsDummy');

    class TestOfImplementations extends UnitTestCase {

        function testMockedInterfaceCanPassThroughTypeHint() {
            $mock = new MockDummyInterface();
            $hinter = new WithHint();
            $hinter->hinted($mock);
        }

        function testImplementedInterfacesAreCarried() {
            $mock = new MockImplementsDummy();
            $hinter = new WithHint();
            $hinter->hinted($mock);
        }
        
        function testNoSpuriousWarningsWhenSkippingDefaultedParameter() {
            $mock = new MockImplementsDummy();
            $mock->extraMethod();
        }
    }
?>