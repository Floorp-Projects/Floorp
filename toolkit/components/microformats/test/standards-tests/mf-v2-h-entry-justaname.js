/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-entry/justaname
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-entry', function() {
   var htmlFragment = "<p class=\"h-entry\">microformats.org at 7</p>";
   var expected = {"items":[{"type":["h-entry"],"properties":{"name":["microformats.org at 7"]}}],"rels":{},"rel-urls":{}};

   it('justaname', function(){
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
