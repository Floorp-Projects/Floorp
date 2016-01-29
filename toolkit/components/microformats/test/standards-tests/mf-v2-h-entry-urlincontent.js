/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-entry/urlincontent
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-entry', function() {
   var htmlFragment = "<div class=\"h-entry\">\n    <h1><a class=\"p-name\">Expanding URLs within HTML content</a></h1>\n    <div class=\"e-content\">\n        <ul>\n            <li><a href=\"http://www.w3.org/\">Should not change: http://www.w3.org/</a></li>\n            <li><a href=\"http://example.com/\">Should not change: http://example.com/</a></li>\n            <li><a href=\"test.html\">File relative: test.html = http://example.com/test.html</a></li>\n            <li><a href=\"/test/test.html\">Directory relative: /test/test.html = http://example.com/test/test.html</a></li>\n            <li><a href=\"/test.html\">Relative to root: /test.html = http://example.com/test.html</a></li>\n        </ul>\n        <img src=\"images/photo.gif\" />\n    </div>  \n</div>";
   var expected = {"items":[{"type":["h-entry"],"properties":{"name":["Expanding URLs within HTML content"],"content":[{"value":"Should not change: http://www.w3.org/\n            Should not change: http://example.com/\n            File relative: test.html = http://example.com/test.html\n            Directory relative: /test/test.html = http://example.com/test/test.html\n            Relative to root: /test.html = http://example.com/test.html","html":"\n        <ul>\n            <li><a href=\"http://www.w3.org/\">Should not change: http://www.w3.org/</a></li>\n            <li><a href=\"http://example.com/\">Should not change: http://example.com/</a></li>\n            <li><a href=\"http://example.com/test.html\">File relative: test.html = http://example.com/test.html</a></li>\n            <li><a href=\"http://example.com/test/test.html\">Directory relative: /test/test.html = http://example.com/test/test.html</a></li>\n            <li><a href=\"http://example.com/test.html\">Relative to root: /test.html = http://example.com/test.html</a></li>\n        </ul>\n        <img src=\"http://example.com/images/photo.gif\" />\n    "}]}}],"rels":{},"rel-urls":{}};

   it('urlincontent', function(){
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
