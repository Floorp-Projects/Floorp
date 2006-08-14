<?php
class ExtensionsControllerTest extends UnitTestCase {

	
	function setUp() {
		$this->controller =& new Extensions();
		$this->controller->Extension->id=1;
		$this->stuff = $this->Extension->read();
	}
	
	function testName() {
		$this->name = $this->stuff['Extension']['name'];
		$this->assertEqual('$this->name', 'MicroFarmer');
	}
}
?>  