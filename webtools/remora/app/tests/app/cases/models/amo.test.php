<?php
class AmoTest extends UnitTestCase {
	
	//Setup the Amo Component
	function setUp() {
		loadComponent('Amo');
		$this->Amo =& new AmoComponent();
	}

	//Make sure there are no PHP errors
	function testNoErrors() {
		$this->assertNoErrors();
	}

	//Make sure the addontypes is an array
	function testAddonTypes() {
		$this->assertIsA($this->Amo->addonTypes, 'array');
	}
}
?>  
