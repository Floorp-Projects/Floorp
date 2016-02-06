/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-geo/simpleproperties
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-geo', function() {
   var htmlFragment = "<p class=\"h-geo\">We are meeting at \n    <span class=\"p-name\">The Bricklayer's Arms</span>\n    (Geo: <span class=\"p-latitude\">51.513458</span>:\n    <span class=\"p-longitude\">-0.14812</span>)\n</p>";
   var expected = {"items":[{"type":["h-geo"],"properties":{"name":["The Bricklayer's Arms"],"latitude":["51.513458"],"longitude":["-0.14812"]}}],"rels":{},"rel-urls":{}};

   it('simpleproperties', function(){
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
