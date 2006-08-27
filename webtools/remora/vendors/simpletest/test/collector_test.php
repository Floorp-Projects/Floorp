<?php
// $Id: collector_test.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $

require_once(dirname(__FILE__) . '/../collector.php');
Mock::generate('GroupTest');

class PathEqualExpectation extends EqualExpectation {
	function PathEqualExpectation($value, $message = '%s') {
    	$this->EqualExpectation(str_replace('\\', '/', $value), $message);
	}
	
    function test($compare) {
        return parent::test(str_replace('\\', '/', $compare));
    }
}

class TestOfCollector extends UnitTestCase {
    
    function testCollectionIsAddedToGroup() {
        $group = &new MockGroupTest();
        $group->expectMinimumCallCount('addTestFile', 2);
        $group->expectArguments(
                'addTestFile',
                array(new PatternExpectation('/collectable\\.(1|2)$/')));
        $collector = &new SimpleCollector();
        $collector->collect($group, dirname(__FILE__) . '/support/collector/');
    }
}
    
class TestOfPatternCollector extends UnitTestCase {
    
    function testAddingEverythingToGroup() {
        $group = &new MockGroupTest();
        $group->expectCallCount('addTestFile', 2);
        $group->expectArguments(
                'addTestFile',
                array(new PatternExpectation('/collectable\\.(1|2)$/')));
        $collector = &new SimplePatternCollector('/.*/');
        $collector->collect($group, dirname(__FILE__) . '/support/collector/');
    }
        
    function testOnlyMatchedFilesAreAddedToGroup() {
        $group = &new MockGroupTest();
        $group->expectOnce('addTestFile', array(new PathEqualExpectation(
        		dirname(__FILE__) . '/support/collector/collectable.1')));
        $collector = &new SimplePatternCollector('/1$/');
        $collector->collect($group, dirname(__FILE__) . '/support/collector/');
    }
}
?>