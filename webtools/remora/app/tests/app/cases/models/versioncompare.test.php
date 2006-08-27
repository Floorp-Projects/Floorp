<?php
class VersionCompareTest extends UnitTestCase {
	function compareVersions($a, $b) {
		return $this->VersionCompare->CompareVersions($a, $b);
	}

	// $test is in the inclusive range bounded by $lower and $upper
	function assertVersionBetweenClosed($test, $lower, $upper) {
		$this->assertTrue($this->VersionCompare->VersionBetween($test, $lower, $upper));
	}

	// $test is NOT in the inclusive range bounded by $lower and $upper
	function assertVersionNotBetweenClosed($test, $lower, $upper) {
		$this->assertFalse($this->VersionCompare->VersionBetween($test, $lower, $upper));
	}
	
	// The versions are considered equal
	function assertVersionsEqual($a, $b) {
		$this->assertEqual($this->compareVersions($a, $b), 0);
	}

	// The first version is greater than the second
	function assertVersionGreater($greater, $lesser) {
		$this->assertEqual($this->compareVersions($greater, $lesser), 1);
	}
	
	//Setup the VC Component
	function setUp() {
		loadComponent('Versioncompare');
		$this->VersionCompare =& new VersionCompareComponent();
	}

	//Make sure there are no PHP errors
	function testNoErrors() {
		$this->assertNoErrors();
	}

	// Minor update is greater than base version
	function testMinorUpdateGreater() {
		$this->assertVersionGreater('1.0.1.4', '1.0');
	}
	
	// A later update with fewer components is greater than earlier+longer
	function testLaterShorterMinorUpdateGreater() {
		$this->assertVersionGreater('1.0.2', '1.0.1.5');
	}

    // Beta is less than release-stream wildcard
    function testBetaLessThanReleaseStreamWildcard() {
		$this->assertVersionGreater('2.0.0.*', '2.0b1');
	}
	
	// Alpha is less than beta, beta less than final
	function testAlphaBetaRCFinalOrdering() {
		$this->assertVersionGreater('3.0b1', '3.0a2');
		$this->assertVersionGreater('2.0', '2.0b3');
	}

	// A compatible update is between the release version and the compatible-stream wildcard
	function testCompatibleUpdateInReleaseStream() {
		$this->assertVersionBetweenClosed('2.0.0.1', '2.0', '2.0.0.*');
	}
	
	// Incompatible updates aren't
	function testIncompatibleUpdateNotInStream() {
		$this->assertVersionNotBetweenClosed('2.0.1', '2.0', '2.0.0.*');
	}

    // Extra zeroes don't matter
    function testImpliedZeroes() {
		$this->assertVersionsEqual('1.0', '1.0.0.0');
		$this->assertVersionsEqual('1.0.0', '1.0');
		$this->assertVersionsEqual('1.5', '1.5.0.0');
    }

}
?>  
