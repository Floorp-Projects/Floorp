<?php
class ExtensionTest extends WebTestCase {
  var $model;
  var $host = 'http://localhost:80/extensions';
  var $getString;
  var $identifier;
  
  function ExtensionTest() {
  	$this->WebTestCase("Cake Remora Page Tests");
  }
  function setUp() {
    
    $this->identifier = $_GET['id'];
    $this->model =& new Extension();
    $this->getString = $this->host . "/show/" . $this->identifier;
    
  }
  
  function testRemoraPage() {
    
    //just checks if the page works or not
    $this->get($this->host);
    $this->assertWantedPattern('/List Extensions/i', "pattern detected");
  }
  function testTitle() {
    
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    $this->title = $this->data['Extension']['name'] . " :: Mozilla Add-ons :: Add Features to Mozilla Software";
    $this->assertTitle($this->title);
  
  }
  function testAuthor() {
    
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    $this->authorPattern = "#<span id=\"developer\"> by <a href=\"" . $this->data['Extension']['homepage'] . "\" title=\"jump to developer information\">" . $this->data['Extension']['author'] . "</a></span><br />#";
    $this->assertWantedPattern($this->authorPattern, htmlentities($this->authorPattern));
  
  }
  function testIcon() {
    
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    $this->wantedPattern = "#<img src=\"" . $this->data['Extension']['icon'] . "\" id=\"addonIcon\" alt=\"" . $this->data['Extension']['name'] . " icon\" />#";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
  
  }
  function testSize() {
    
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    $this->wantedPattern = "#<img src=\"/images/install.gif\" />Install <br />
          <span>\(" . $this->data['Version'][0]['size'] . "KB\)</span>#";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    $this->wantedPattern = "#<img src=\"/images/download.gif\" />Download<br />
          <span>\(" . $this->data['Version'][0]['size'] . "KB\)</span>#";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    
  }
  function testVersionNotes() {
   
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    // check the main version area
    $this->wantedPattern = "@<h3>Version " . $this->data['Version'][0]['version'] . " <span>&mdash; " . $this->data['Version'][0]['created'] . "</span></h3>
    @";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    $this->wantedPattern = "#" . $this->data['Version'][0]['notes'] . "<br />#";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    // check the version at the top title
    $this->wantedPattern = "#<span id=\"version\">" . $this->data['Version'][0]['version'] . "</span>#";
    $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    
  }
  function testCategories() {
    
    $this->model->id = $this->identifier;
    $this->data = $this->model->read();
    $this->get($this->getString);
    
    foreach ($this->data['Addontype'] as $addontype) {
      $typename = $addontype['name'];
      $this->wantedPattern = "@<li><a href=\"#\">" . $typename . "</a></li>@";  
      $this->assertWantedPattern($this->wantedPattern, htmlentities($this->wantedPattern));
    }
    
  
  }
  
}
?>
