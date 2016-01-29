/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-card/impliedurl
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-card', function() {
   var htmlFragment = "<a class=\"h-card\" href=\"jane.html\">Jane Doe</a>\n<area class=\"h-card\" href=\"jane.html\" alt=\"Jane Doe\"/ >\n<div class=\"h-card\" ><a href=\"jane.html\">Jane Doe</a><p></p></div> \n<div class=\"h-card\" ><area href=\"jane.html\">Jane Doe</area><p></p></div>\n<div class=\"h-card\" ><a class=\"h-card\" href=\"jane.html\">Jane Doe</a><p></p></div> ";
   var expected = {"items":[{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"]},"children":[{"value":"Jane Doe","type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}}]}],"rels":{},"rel-urls":{}};

   it('impliedurl', function(){
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
