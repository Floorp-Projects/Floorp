<?php
class VersionCompareTest extends UnitTestCase {
	
	//Setup the VC Component
	function setUp() {
		loadComponent('Versioncompare');
		$this->VersionCompare =& new VersionCompareComponent();
	}

	//Make sure there are no PHP errors
	function testNoErrors() {
		$this->assertNoErrors();
	}

	//Make sure the comparison returns greater
	function testGreater() {
		$results = $this->VersionCompare->CompareVersions('1.0.1.4', '1.0');
		$this->assertEqual($results, 1);
	}

        //Make sure the comparison returns lesser
        function testLesser() {
                $results = $this->VersionCompare->CompareVersions('2.0b1', '2.5.0.*');
                $this->assertEqual($results, -1);
        }

        //Make sure the comparison returns equal
        function testEqual() {
                $results = $this->VersionCompare->CompareVersions('1.0.4', '1.0.4.0');
                $this->assertEqual($results, 0);
        }

}
?>  
