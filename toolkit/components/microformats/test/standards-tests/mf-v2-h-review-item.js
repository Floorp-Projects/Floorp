/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-review/item
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-review', function() {
   var htmlFragment = "<base href=\"http://example.com\" >\n<div class=\"h-review\">\n    <p class=\"p-item h-item\">\n        <img class=\"u-photo\" src=\"images/photo.gif\" />\n        <a class=\"p-name u-url\" href=\"http://example.com/crepeoncole\">Crepes on Cole</a>\n    </p>\n    <p><span class=\"p-rating\">5</span> out of 5 stars</p>\n</div>";
   var expected = {"items":[{"type":["h-review"],"properties":{"item":[{"value":"Crepes on Cole","type":["h-item"],"properties":{"photo":["http://example.com/images/photo.gif"],"name":["Crepes on Cole"],"url":["http://example.com/crepeoncole"]}}],"rating":["5"],"name":["Crepes on Cole\n    \n    5 out of 5 stars"]}}],"rels":{},"rel-urls":{}};

   it('item', function(){
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
