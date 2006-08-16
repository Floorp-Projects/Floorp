<?php
class RdfTest extends UnitTestCase {
	var $manifestData = '
<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#" 
     xmlns:em="http://www.mozilla.org/2004/em-rdf#">

	<Description about="urn:mozilla:install-manifest">
	
		<em:id>{XXXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}</em:id>
		<em:name>Sample Extension</em:name>
		<em:version>1.0</em:version>
		<em:description>A sample extension with advanced features</em:description>
		<em:creator>Your Name Here</em:creator>
		<!-- optional items -->
		<em:contributor>A person who helped you</em:contributor>
		<em:contributor>Another one</em:contributor>
		<em:homepageURL>http://sampleextension.mozdev.org/</em:homepageURL>
		<em:optionsURL>chrome://sampleext/content/settings.xul</em:optionsURL>
		<em:aboutURL>chrome://sampleext/content/about.xul</em:aboutURL>
		<em:iconURL>chrome://sampleext/skin/mainicon.png</em:iconURL>
		<em:updateURL>http://sampleextension.mozdev.org/update.rdf</em:updateURL>
		<em:type>2</em:type> <!-- type=extension --> 

                <!-- Firefox -->
		<em:targetApplication>
			<Description>
				<em:id>{ec8030f7-c20a-464f-9b0e-13a3a9e97384}</em:id>
				<em:minVersion>0.9</em:minVersion>
				<em:maxVersion>1.0</em:maxVersion>
			</Description>
		</em:targetApplication>

                <!-- This is not needed for Firefox 1.1 and later. Only include this 
                     if you want to make your extension compatible with older versions -->
		<em:file>
			<Description about="urn:mozilla:extension:file:sampleext.jar">
				<em:package>content/</em:package>
				<!-- optional items -->
				<em:skin>skin/classic/</em:skin>
				<em:locale>locale/en-US/</em:locale>
				<em:locale>locale/ru-RU/</em:locale>
			</Description>
		</em:file>
	</Description>

</RDF>';
	
	//Setup the RDF Component
	function setUp() {
		loadComponent('Rdf');
		$this->Rdf =& new RdfComponent();
	}

	//Make sure there are no PHP errors
	function testNoErrors() {
		$this->assertNoErrors();
	}

	//Make sure the XML returns an array of parsed data
	function testIsArray() {
		$results = $this->Rdf->parseInstallManifest($this->manifestData);
		$this->assertIsA($results, 'array');
		$this->returnedArray = $results;
	}

	//Make sure the XML generates an error if invalid
	function testIsString() {
		$results = $this->Rdf->parseInstallManifest($this->manifestData." this is so not valid xml");
		$this->assertIsA($results, 'string');
	}

	//Make sure the parsed addon name is correct
	function testParseName() {
		$this->assertEqual($this->returnedArray['name']['en-US'], 'Sample Extension');
	}

        //Make sure the parsed addon GUID is correct
        function testGUID() {
                $this->assertEqual($this->returnedArray['id'], '{XXXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}');
        }

        //Make sure the parsed addon target applications are correct
        function testParseTargetApps() {
                $this->assertEqual($this->returnedArray['targetApplication']['{ec8030f7-c20a-464f-9b0e-13a3a9e97384}']['minVersion'],
                                    '0.9');
        }

}
?>  
