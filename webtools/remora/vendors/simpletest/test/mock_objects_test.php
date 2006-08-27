<?php
    // $Id: mock_objects_test.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $
    require_once(dirname(__FILE__) . '/../expectation.php');

    class TestOfAnythingExpectation extends UnitTestCase {

        function testSimpleInteger() {
            $expectation = new AnythingExpectation();
            $this->assertTrue($expectation->test(33));
            $this->assertPattern(
                    '/matches.*33/i',
                    $expectation->testMessage(33));
        }
    }

    class TestOfParametersExpectation extends UnitTestCase {

        function testEmptyMatch() {
            $expectation = new ParametersExpectation(array());
            $this->assertTrue($expectation->test(array()));
            $this->assertFalse($expectation->test(array(33)));
        }

        function testSingleMatch() {
            $expectation = new ParametersExpectation(array(0));
            $this->assertFalse($expectation->test(array(1)));
            $this->assertTrue($expectation->test(array(0)));
        }

        function testAnyMatch() {
            $expectation = new ParametersExpectation(false);
            $this->assertTrue($expectation->test(array()));
            $this->assertTrue($expectation->test(array(1, 2)));
        }

        function testMissingParameter() {
            $expectation = new ParametersExpectation(array(0));
            $this->assertFalse($expectation->test(array()));
        }

        function testNullParameter() {
            $expectation = new ParametersExpectation(array(null));
            $this->assertTrue($expectation->test(array(null)));
            $this->assertFalse($expectation->test(array()));
        }

        function testAnythingExpectations() {
            $expectation = new ParametersExpectation(array(new AnythingExpectation()));
            $this->assertFalse($expectation->test(array()));
            $this->assertIdentical($expectation->test(array(null)), true);
            $this->assertIdentical($expectation->test(array(13)), true);
        }

        function testOtherExpectations() {
            $expectation = new ParametersExpectation(
                    array(new PatternExpectation('/hello/i')));
            $this->assertFalse($expectation->test(array('Goodbye')));
            $this->assertTrue($expectation->test(array('hello')));
            $this->assertTrue($expectation->test(array('Hello')));
        }

        function testIdentityOnly() {
            $expectation = new ParametersExpectation(array("0"));
            $this->assertFalse($expectation->test(array(0)));
            $this->assertTrue($expectation->test(array("0")));
        }

        function testLongList() {
            $expectation = new ParametersExpectation(
                    array("0", 0, new AnythingExpectation(), false));
            $this->assertTrue($expectation->test(array("0", 0, 37, false)));
            $this->assertFalse($expectation->test(array("0", 0, 37, true)));
            $this->assertFalse($expectation->test(array("0", 0, 37)));
        }
    }

    class TestOfCallMap extends UnitTestCase {

        function testEmpty() {
            $map = new CallMap();
            $this->assertFalse($map->isMatch("any", array()));
            $this->assertNull($map->findFirstMatch("any", array()));
        }

        function testExactValue() {
            $map = new CallMap();
            $map->addValue(array(0), "Fred");
            $map->addValue(array(1), "Jim");
            $map->addValue(array("1"), "Tom");
            $this->assertTrue($map->isMatch(array(0)));
            $this->assertEqual($map->findFirstMatch(array(0)), "Fred");
            $this->assertTrue($map->isMatch(array(1)));
            $this->assertEqual($map->findFirstMatch(array(1)), "Jim");
            $this->assertEqual($map->findFirstMatch(array("1")), "Tom");
        }

        function testExactReference() {
            $map = new CallMap();
            $ref = "Fred";
            $map->addReference(array(0), $ref);
            $this->assertEqual($map->findFirstMatch(array(0)), "Fred");
            $ref2 = &$map->findFirstMatch(array(0));
            $this->assertReference($ref2, $ref);
        }

        function testWildcard() {
            $map = new CallMap();
            $map->addValue(array(new AnythingExpectation(), 1, 3), "Fred");
            $this->assertTrue($map->isMatch(array(2, 1, 3)));
            $this->assertEqual($map->findFirstMatch(array(2, 1, 3)), "Fred");
        }

        function testAllWildcard() {
            $map = new CallMap();
            $this->assertFalse($map->isMatch(array(2, 1, 3)));
            $map->addValue("", "Fred");
            $this->assertTrue($map->isMatch(array(2, 1, 3)));
            $this->assertEqual($map->findFirstMatch(array(2, 1, 3)), "Fred");
        }

        function testOrdering() {
            $map = new CallMap();
            $map->addValue(array(1, 2), "1, 2");
            $map->addValue(array(1, 3), "1, 3");
            $map->addValue(array(1), "1");
            $map->addValue(array(1, 4), "1, 4");
            $map->addValue(array(new AnythingExpectation()), "Any");
            $map->addValue(array(2), "2");
            $map->addValue("", "Default");
            $map->addValue(array(), "None");
            $this->assertEqual($map->findFirstMatch(array(1, 2)), "1, 2");
            $this->assertEqual($map->findFirstMatch(array(1, 3)), "1, 3");
            $this->assertEqual($map->findFirstMatch(array(1, 4)), "1, 4");
            $this->assertEqual($map->findFirstMatch(array(1)), "1");
            $this->assertEqual($map->findFirstMatch(array(2)), "Any");
            $this->assertEqual($map->findFirstMatch(array(3)), "Any");
            $this->assertEqual($map->findFirstMatch(array()), "Default");
        }
    }

    class Dummy {
        function Dummy() {
        }

        function aMethod() {
            return true;
        }

        function anotherMethod() {
            return true;
        }

        function __get($key) {
            return $key;
        }
    }

    Stub::generate('Dummy', 'StubDummy');
    Stub::generate('Dummy', 'AnotherStubDummy');
    Stub::generate('Dummy', 'StubDummyWithExtraMethods', array('extraMethod'));

    class SpecialSimpleStub extends SimpleMock {
        function SpecialSimpleStub() {
            $this->SimpleMock();
        }
    }
    SimpleTest::setMockBaseClass('SpecialSimpleStub');
    Stub::generate('Dummy', 'SpecialStubDummy');
    SimpleTest::setMockBaseClass('SimpleMock');

    class TestOfStubGeneration extends UnitTestCase {

        function testCloning() {
            $stub = &new StubDummy();
            $this->assertTrue(method_exists($stub, "aMethod"));
            $this->assertNull($stub->aMethod(null));
        }

        function testCloningWithExtraMethod() {
            $stub = &new StubDummyWithExtraMethods();
            $this->assertTrue(method_exists($stub, "extraMethod"));
        }

        function testCloningWithChosenClassName() {
            $stub = &new AnotherStubDummy();
            $this->assertTrue(method_exists($stub, "aMethod"));
        }

        function testCloningWithDifferentBaseClass() {
            $stub = &new SpecialStubDummy();
            $this->assertIsA($stub, "SpecialSimpleStub");
            $this->assertTrue(method_exists($stub, "aMethod"));
        }
    }

    class TestOfServerStubReturns extends UnitTestCase {

        function testDefaultReturn() {
            $stub = &new StubDummy();
            $stub->setReturnValue("aMethod", "aaa");
            $this->assertIdentical($stub->aMethod(), "aaa");
            $this->assertIdentical($stub->aMethod(), "aaa");
        }

        function testParameteredReturn() {
            $stub = &new StubDummy();
            $stub->setReturnValue("aMethod", "aaa", array(1, 2, 3));
            $this->assertNull($stub->aMethod());
            $this->assertIdentical($stub->aMethod(1, 2, 3), "aaa");
        }

        function testReferenceReturned() {
            $stub = &new StubDummy();
            $object = new Dummy();
            $stub->setReturnReference("aMethod", $object, array(1, 2, 3));
            $this->assertReference($zref =& $stub->aMethod(1, 2, 3), $object);
        }

        function testCallCount() {
            $stub = &new StubDummy();
            $this->assertEqual($stub->getCallCount("aMethod"), 0);
            $stub->aMethod();
            $this->assertEqual($stub->getCallCount("aMethod"), 1);
            $stub->aMethod();
            $this->assertEqual($stub->getCallCount("aMethod"), 2);
        }

        function testMultipleMethods() {
            $stub = &new StubDummy();
            $stub->setReturnValue("aMethod", 100, array(1));
            $stub->setReturnValue("aMethod", 200, array(2));
            $stub->setReturnValue("anotherMethod", 10, array(1));
            $stub->setReturnValue("anotherMethod", 20, array(2));
            $this->assertIdentical($stub->aMethod(1), 100);
            $this->assertIdentical($stub->anotherMethod(1), 10);
            $this->assertIdentical($stub->aMethod(2), 200);
            $this->assertIdentical($stub->anotherMethod(2), 20);
        }

        function testReturnSequence() {
            $stub = &new StubDummy();
            $stub->setReturnValueAt(0, "aMethod", "aaa");
            $stub->setReturnValueAt(1, "aMethod", "bbb");
            $stub->setReturnValueAt(3, "aMethod", "ddd");
            $this->assertIdentical($stub->aMethod(), "aaa");
            $this->assertIdentical($stub->aMethod(), "bbb");
            $this->assertNull($stub->aMethod());
            $this->assertIdentical($stub->aMethod(), "ddd");
        }

        function testReturnReferenceSequence() {
            $stub = &new StubDummy();
            $object = new Dummy();
            $stub->setReturnReferenceAt(1, "aMethod", $object);
            $this->assertNull($stub->aMethod());
            $this->assertReference($zref =& $stub->aMethod(), $object);
            $this->assertNull($stub->aMethod());
        }

        function testComplicatedReturnSequence() {
            $stub = &new StubDummy();
            $object = new Dummy();
            $stub->setReturnValueAt(1, "aMethod", "aaa", array("a"));
            $stub->setReturnValueAt(1, "aMethod", "bbb");
            $stub->setReturnReferenceAt(2, "aMethod", $object, array('*', 2));
            $stub->setReturnValueAt(2, "aMethod", "value", array('*', 3));
            $stub->setReturnValue("aMethod", 3, array(3));
            $this->assertNull($stub->aMethod());
            $this->assertEqual($stub->aMethod("a"), "aaa");
            $this->assertReference($zref =& $stub->aMethod(1, 2), $object);
            $this->assertEqual($stub->aMethod(3), 3);
            $this->assertNull($stub->aMethod());
        }

        function testMultipleMethodSequences() {
            $stub = &new StubDummy();
            $stub->setReturnValueAt(0, "aMethod", "aaa");
            $stub->setReturnValueAt(1, "aMethod", "bbb");
            $stub->setReturnValueAt(0, "anotherMethod", "ccc");
            $stub->setReturnValueAt(1, "anotherMethod", "ddd");
            $this->assertIdentical($stub->aMethod(), "aaa");
            $this->assertIdentical($stub->anotherMethod(), "ccc");
            $this->assertIdentical($stub->aMethod(), "bbb");
            $this->assertIdentical($stub->anotherMethod(), "ddd");
        }

        function testSequenceFallback() {
            $stub = &new StubDummy();
            $stub->setReturnValueAt(0, "aMethod", "aaa", array('a'));
            $stub->setReturnValueAt(1, "aMethod", "bbb", array('a'));
            $stub->setReturnValue("aMethod", "AAA");
            $this->assertIdentical($stub->aMethod('a'), "aaa");
            $this->assertIdentical($stub->aMethod('b'), "AAA");
        }

        function testMethodInterference() {
            $stub = &new StubDummy();
            $stub->setReturnValueAt(0, "anotherMethod", "aaa");
            $stub->setReturnValue("aMethod", "AAA");
            $this->assertIdentical($stub->aMethod(), "AAA");
            $this->assertIdentical($stub->anotherMethod(), "aaa");
        }
    }

    Mock::generate('Dummy');
    Mock::generate('Dummy', 'AnotherMockDummy');
    Mock::generate('Dummy', 'MockDummyWithExtraMethods', array('extraMethod'));

    class SpecialSimpleMock extends SimpleMock { }

    SimpleTest::setMockBaseClass("SpecialSimpleMock");
    Mock::generate("Dummy", "SpecialMockDummy");
    SimpleTest::setMockBaseClass("SimpleMock");

    class TestOfMockGeneration extends UnitTestCase {

        function testCloning() {
            $mock = &new MockDummy();
            $this->assertTrue(method_exists($mock, "aMethod"));
            $this->assertNull($mock->aMethod());
        }

        function testCloningWithExtraMethod() {
            $mock = &new MockDummyWithExtraMethods();
            $this->assertTrue(method_exists($mock, "extraMethod"));
        }

        function testCloningWithChosenClassName() {
            $mock = &new AnotherMockDummy();
            $this->assertTrue(method_exists($mock, "aMethod"));
        }

        function testCloningWithDifferentBaseClass() {
            $mock = &new SpecialMockDummy();
            $this->assertIsA($mock, "SpecialSimpleMock");
            $this->assertTrue(method_exists($mock, "aMethod"));
        }
    }

    class TestOfMockReturns extends UnitTestCase {

        function testParameteredReturn() {
            $mock = &new MockDummy();
            $mock->setReturnValue('aMethod', 'aaa', array(1, 2, 3));
            $this->assertNull($mock->aMethod());
            $this->assertIdentical($mock->aMethod(1, 2, 3), 'aaa');
        }

        function testReferenceReturned() {
            $mock = &new MockDummy();
            $object = new Dummy();
            $mock->setReturnReference("aMethod", $object, array(1, 2, 3));
            $this->assertReference($zref =& $mock->aMethod(1, 2, 3), $object);
        }

        function testWildcardReturn() {
            $mock = &new MockDummy();
            $mock->setWildcard('wild');
            $mock->setReturnValue('aMethod', 'a', array(1, 'wild', 3));
            $this->assertIdentical($mock->aMethod(1, 'something', 3), 'a');
            $this->assertIdentical($mock->aMethod(1, 'anything', 3), 'a');
        }

        function testPatternMatchReturn() {
            $mock = &new MockDummy();
            $mock->setReturnValue(
                    "aMethod",
                    "aaa",
                    array(new PatternExpectation('/hello/i')));
            $this->assertIdentical($mock->aMethod('Hello'), "aaa");
            $this->assertNull($mock->aMethod('Goodbye'));
        }

        function testCallCount() {
            $mock = &new MockDummy();
            $this->assertEqual($mock->getCallCount("aMethod"), 0);
            $mock->aMethod();
            $this->assertEqual($mock->getCallCount("aMethod"), 1);
            $mock->aMethod();
            $this->assertEqual($mock->getCallCount("aMethod"), 2);
        }

        function testReturnReferenceSequence() {
            $mock = &new MockDummy();
            $object = new Dummy();
            $mock->setReturnReferenceAt(1, "aMethod", $object);
            $this->assertNull($mock->aMethod());
            $this->assertReference($zref =& $mock->aMethod(), $object);
            $this->assertNull($mock->aMethod());
            $this->swallowErrors();
        }
    }

    class TestOfSpecialMethods extends UnitTestCase {
        function testReturnFromSpecialMethod() {
            $mock = &new MockDummy();
            $mock->setReturnValue('__get', '1st Return', array('first'));
            $mock->setReturnValue('__get', '2nd Return', array('second'));
            $this->assertEqual($mock->__get('first'), '1st Return');
            $this->assertEqual($mock->__get('second'), '2nd Return');
            if (phpversion() >= 5) {
                $this->assertEqual($mock->first, $mock->__get('first'));
                $this->assertEqual($mock->second, $mock->__get('second'));
            }
        }
    }

    class TestOfMockTally extends UnitTestCase {

        function testZeroCallCount() {
            $mock = &new MockDummy();
            $mock->expectCallCount("aMethod", 0);
        }

        function testExpectedCallCount() {
            $mock = &new MockDummy();
            $mock->expectCallCount("aMethod", 2);
            $mock->aMethod();
            $mock->aMethod();
        }
    }

    class MockDummyWithInjectedTestCase extends MockDummy {
        function &_getCurrentTestCase() {
            $test = &SimpleTest::getCurrent();
            return $test->test;
        }
    }

    Mock::generate("SimpleTestCase");

    class TestOfMockExpectations extends UnitTestCase {
        var $_test;

        function setUp() {
            $this->test = &new MockSimpleTestCase();
        }

        function testSettingExpectationOnNonMethodThrowsError() {
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectMaximumCallCount("aMissingMethod", 2);
            $this->assertError();
        }

        function testMaxCallsDetectsOverrun() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectMaximumCallCount("aMethod", 2);
            $mock->aMethod();
            $mock->aMethod();
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testTallyOnMaxCallsSendsPassOnUnderrun() {
            $this->test->expectOnce("assertTrue", array(true, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectMaximumCallCount("aMethod", 2);
            $mock->aMethod();
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testExpectNeverDetectsOverrun() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectNever("aMethod");
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testTallyOnExpectNeverSendsPassOnUnderrun() {
            $this->test->expectOnce("assertTrue", array(true, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectNever("aMethod");
            $mock->atTestEnd('testSomething');
        }

        function testMinCalls() {
            $this->test->expectOnce("assertTrue", array(true, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectMinimumCallCount("aMethod", 2);
            $mock->aMethod();
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testFailedNever() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectNever("aMethod");
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testUnderOnce() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectOnce("aMethod");
            $mock->atTestEnd('testSomething');
        }

        function testOverOnce() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectOnce("aMethod");
            $mock->aMethod();
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
            $this->swallowErrors();
        }

        function testUnderAtLeastOnce() {
            $this->test->expectOnce("assertTrue", array(false, '*'));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectAtLeastOnce("aMethod");
            $mock->atTestEnd('testSomething');
        }

        function testZeroArguments() {
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArguments("aMethod", array());
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testExpectedArguments() {
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArguments("aMethod", array(1, 2, 3));
            $mock->aMethod(1, 2, 3);
            $mock->atTestEnd('testSomething');
        }

        function testFailedArguments() {
            $this->test->expectOnce("assertTrue", array(false, "*"));
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArguments("aMethod", array("this"));
            $mock->aMethod("that");
            $mock->atTestEnd('testSomething');
        }

        function testWildcardArguments() {
            $mock = &new MockDummyWithInjectedTestCase($this, "wild");
            $mock->expectArguments("aMethod", array("wild", 123, "wild"));
            $mock->aMethod(100, 123, 101);
            $mock->atTestEnd('testSomething');
        }

        function testSpecificSequence() {
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArgumentsAt(1, "aMethod", array(1, 2, 3));
            $mock->expectArgumentsAt(2, "aMethod", array("Hello"));
            $mock->aMethod();
            $mock->aMethod(1, 2, 3);
            $mock->aMethod("Hello");
            $mock->aMethod();
            $mock->atTestEnd('testSomething');
        }

        function testFailedSequence() {
            $this->test->expectArguments("assertTrue", array(false, "*"));
            $this->test->expectCallCount("assertTrue", 2);
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArgumentsAt(0, "aMethod", array(1, 2, 3));
            $mock->expectArgumentsAt(1, "aMethod", array("Hello"));
            $mock->aMethod(1, 2);
            $mock->aMethod("Goodbye");
            $mock->atTestEnd('testSomething');
        }

        function testBadArgParameter() {
            $mock = &new MockDummyWithInjectedTestCase();
            $mock->expectArguments("aMethod", "foo");
            $this->assertErrorPattern('/\$args.*not an array/i');
            $mock->aMethod();
            $mock->tally();
            $mock->atTestEnd('testSomething');
       }
    }

    class TestOfMockComparisons extends UnitTestCase {

        function testEqualComparisonOfMocksDoesNotCrash() {
            $expectation = &new EqualExpectation(new MockDummy());
            $this->assertTrue($expectation->test(new MockDummy(), true));
        }

        function testIdenticalComparisonOfMocksDoesNotCrash() {
            $expectation = &new IdenticalExpectation(new MockDummy());
            $this->assertTrue($expectation->test(new MockDummy()));
        }
    }

    Mock::generatePartial('Dummy', 'TestDummy', array('anotherMethod'));

    class TestOfPartialMocks extends UnitTestCase {

        function testMethodReplacement() {
            $mock = &new TestDummy();
            $this->assertEqual($mock->aMethod(99), 99);
            $this->assertNull($mock->anotherMethod());
        }

        function testSettingReturns() {
            $mock = &new TestDummy();
            $mock->setReturnValue('anotherMethod', 33, array(3));
            $mock->setReturnValue('anotherMethod', 22);
            $mock->setReturnValueAt(2, 'anotherMethod', 44, array(3));
            $this->assertEqual($mock->anotherMethod(), 22);
            $this->assertEqual($mock->anotherMethod(3), 33);
            $this->assertEqual($mock->anotherMethod(3), 44);
        }

        function testReferences() {
            $mock = &new TestDummy();
            $object = new Dummy();
            $mock->setReturnReferenceAt(0, 'anotherMethod', $object, array(3));
            $this->assertReference($zref =& $mock->anotherMethod(3), $object);
        }

        function testExpectations() {
            $mock = &new TestDummy();
            $mock->expectCallCount('anotherMethod', 2);
            $mock->expectArguments('anotherMethod', array(77));
            $mock->expectArgumentsAt(1, 'anotherMethod', array(66));
            $mock->anotherMethod(77);
            $mock->anotherMethod(66);
        }

        function testSettingExpectationOnMissingMethodThrowsError() {
            $mock = &new TestDummy();
            $mock->expectCallCount('aMissingMethod', 2);
            $this->assertError();
        }
    }

    class ConstructorSuperClass {
        function ConstructorSuperClass() { }
    }

    class ConstructorSubClass extends ConstructorSuperClass {

    }

    class TestOfPHP4StyleSuperClassConstruct extends UnitTestCase {
        /**
         * This addresses issue #1231401.  Without the fix in place, this will
		 * generate a fatal PHP error.
		 */
        function testBasicConstruct() {
            Mock::generate('ConstructorSubClass');
            $mock = &new MockConstructorSubClass();
            $this->assertIsA($mock, 'SimpleMock');
            $this->assertTrue(method_exists($mock, 'ConstructorSuperClass'));
        }
    }
?>