<?php

class RemoraWebTest extends WebTestCase {
  
  

  
  
  function testExtensionPreviewImage() {
    //checks to see if preview images work
    $this->get('http://localhost:80/extensions/show/1/1');
    $this->assertWantedPattern('<img src=\"/sample/screenshot2.gif\" />', 'screenshot2.gif linked');
    $this->clickLink('next->');
    $this->assertWantedPattern('<img src=\"/sample/screenshot3.gif\" />', 'Next: screenshot3.gif linked');
    $this->clickLink('<-prev');
    $this->assertWantedPattern('<img src=\"/sample/screenshot2.gif\" />', 'Prev: screenshot2.gif linked');  
  }
  
  function testExtensionPageTitle() {
    
    //$this->Extension->id = 2;
    //$stuff = $this->Extension->read();
    //$d_title = $stuff['Extension']['name'] . "";
    
    $this->get('http://localhost:80/extensions/show/2');
    $this->assertTitle('MicroResume :: Mozilla Add-ons :: Add Features to Mozilla Software');
  }
}
?>
