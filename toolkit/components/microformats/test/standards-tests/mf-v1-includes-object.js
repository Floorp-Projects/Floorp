/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/includes/object
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('includes', function() {
   var htmlFragment = "<div class=\"vevent\">\n    <span class=\"name\">HTML5 & CSS3 latest features in action!</span> - \n    <span class=\"speaker\">David Rousset</span> -\n    <time class=\"dtstart\" datetime=\"2012-10-30T11:45:00-08:00\">Tue 11:45am</time>\n    <object data=\"#buildconf\" class=\"include\" type=\"text/html\" height=\"0\" width=\"0\"></object>\n</div>\n<div class=\"vevent\">\n    <span class=\"name\">Building High-Performing JavaScript for Modern Engines</span> -\n    <span class=\"speaker\">John-David Dalton</span> and \n    <span class=\"speaker\">Amanda Silver</span> -\n    <time class=\"dtstart\" datetime=\"2012-10-31T11:15:00-08:00\">Wed 11:15am</time>\n    <object data=\"#buildconf\" class=\"include\" type=\"text/html\" height=\"0\" width=\"0\"></object>\n</div>\n\n\n<div id=\"buildconf\">\n    <p class=\"summary\">Build Conference</p>\n    <p class=\"location adr\">\n        <span class=\"locality\">Redmond</span>, \n        <span class=\"region\">Washington</span>, \n        <span class=\"country-name\">USA</span>\n    </p>\n</div>";
   var expected = {"items":[{"type":["h-event"],"properties":{"start":["2012-10-30 11:45:00-08:00"],"name":["Build Conference"],"location":[{"value":"Redmond, \n        Washington, \n        USA","type":["h-adr"],"properties":{"locality":["Redmond"],"region":["Washington"],"country-name":["USA"]}}]}},{"type":["h-event"],"properties":{"start":["2012-10-31 11:15:00-08:00"],"name":["Build Conference"],"location":[{"value":"Redmond, \n        Washington, \n        USA","type":["h-adr"],"properties":{"locality":["Redmond"],"region":["Washington"],"country-name":["USA"]}}]}},{"type":["h-adr"],"properties":{"locality":["Redmond"],"region":["Washington"],"country-name":["USA"]}}],"rels":{},"rel-urls":{}};

   it('object', function(){
       var doc, dom, node, options, parser, found;
       dom = new DOMParser();
       doc = dom.parseFromString( htmlFragment, 'text/html' );
       options ={
           'document': doc,
           'node': doc,
           'baseUrl': 'http://example.com',
           'dateFormat': 'html5'
       };
       found = Microformats.get( options );
       assert.deepEqual(found, expected);
   });
});
