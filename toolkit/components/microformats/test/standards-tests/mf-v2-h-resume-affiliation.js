/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-resume/affiliation
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-resume', function() {
   var htmlFragment = "<div class=\"h-resume\">\n    <p>\n        <span class=\"p-name\">Tim Berners-Lee</span>, \n        <span class=\"p-summary\">invented the World Wide Web</span>. \n    </p> \n    Belongs to following groups:\n    <p>   \n        <a class=\"p-affiliation h-card\" href=\"http://www.w3.org/\">\n            <img class=\"p-name u-photo\" alt=\"W3C\" src=\"http://www.w3.org/Icons/WWW/w3c_home_nb.png\" />\n        </a>\n    </p>   \n</div>";
   var expected = {"items":[{"type":["h-resume"],"properties":{"name":["Tim Berners-Lee"],"summary":["invented the World Wide Web"],"affiliation":[{"type":["h-card"],"properties":{"name":["W3C"],"photo":["http://www.w3.org/Icons/WWW/w3c_home_nb.png"],"url":["http://www.w3.org/"]}}]}}],"rels":{},"rel-urls":{}};

   it('affiliation', function(){
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
