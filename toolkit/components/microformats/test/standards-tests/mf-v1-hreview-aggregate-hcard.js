/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v1/hreview-aggregate/hcard
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hreview-aggregate', function() {
   var htmlFragment = "<div class=\"hreview-aggregate\">\n    <div class=\"item vcard\">\n        <h3 class=\"fn org\">Mediterranean Wraps</h3>    \n        <p>\n            <span class=\"adr\">\n                <span class=\"street-address\">433 S California Ave</span>, \n                <span class=\"locality\">Palo Alto</span>, \n                <span class=\"region\">CA</span></span> - \n            \n            <span class=\"tel\">(650) 321-8189</span>\n        </p>\n    </div> \n    <p class=\"rating\">\n        <span class=\"average value\">9.2</span> out of \n        <span class=\"best\">10</span> \n        based on <span class=\"count\">17</span> reviews\n    </p>\n</div>";
   var expected = {"items":[{"type":["h-review-aggregate"],"properties":{"item":[{"value":"Mediterranean Wraps","type":["h-item","h-card"],"properties":{"name":["Mediterranean Wraps"],"org":["Mediterranean Wraps"],"adr":[{"value":"433 S California Ave, \n                Palo Alto, \n                CA","type":["h-adr"],"properties":{"street-address":["433 S California Ave"],"locality":["Palo Alto"],"region":["CA"]}}],"tel":["(650) 321-8189"]}}],"rating":["9.2"],"average":["9.2"],"best":["10"],"count":["17"]}}],"rels":{},"rel-urls":{}};

   it('hcard', function(){
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
