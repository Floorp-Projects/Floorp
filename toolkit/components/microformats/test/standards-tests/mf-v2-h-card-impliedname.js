/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-card/impliedname
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-card', function() {
   var htmlFragment = "\n<img class=\"h-card\" src=\"jane.html\" alt=\"Jane Doe\"/>\n<area class=\"h-card\" href=\"jane.html\" alt=\"Jane Doe\"></area>\n<abbr class=\"h-card\" title=\"Jane Doe\">JD</abbr>\n\n<div class=\"h-card\"><img src=\"jane.html\" alt=\"Jane Doe\"/></div>\n<div class=\"h-card\"><area href=\"jane.html\" alt=\"Jane Doe\"></area></div>\n<div class=\"h-card\"><abbr title=\"Jane Doe\">JD</abbr></div>\n\n<div class=\"h-card\"><span><img src=\"jane.html\" alt=\"Jane Doe\"/></span></div>\n<div class=\"h-card\"><span><area href=\"jane.html\" alt=\"Jane Doe\"></area></span></div>\n<div class=\"h-card\"><span><abbr title=\"Jane Doe\">JD</abbr></span></div>\n\n<div class=\"h-card\"><img class=\"h-card\" src=\"john.html\" alt=\"John Doe\"/>Name</div>\n<div class=\"h-card\"><span class=\"h-card\"><img src=\"john.html\" alt=\"John Doe\"/>Name</span></div>\n";
   var expected = {"items":[{"type":["h-card"],"properties":{"name":["Jane Doe"],"photo":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"photo":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"photo":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"],"url":["http://example.com/jane.html"]}},{"type":["h-card"],"properties":{"name":["Jane Doe"]}},{"type":["h-card"],"properties":{"name":["Name"]},"children":[{"type":["h-card"],"properties":{"name":["John Doe"],"photo":["http://example.com/john.html"]}}]},{"type":["h-card"],"properties":{"name":["Name"]},"children":[{"value":"Name","type":["h-card"],"properties":{"name":["John Doe"],"photo":["http://example.com/john.html"]}}]}],"rels":{},"rel-urls":{}};

   it('impliedname', function(){
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
