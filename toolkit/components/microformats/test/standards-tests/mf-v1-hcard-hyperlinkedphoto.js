/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/hcard/hyperlinkedphoto
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hcard', function() {
   var htmlFragment = "<a class=\"vcard\" href=\"http://rohit.khare.org/\">\n        <img alt=\"Rohit Khare\" src=\"images/photo.gif\" />\n</a>";
   var expected = {"items":[{"type":["h-card"],"properties":{}}],"rels":{},"rel-urls":{}};

   it('hyperlinkedphoto', function(){
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
