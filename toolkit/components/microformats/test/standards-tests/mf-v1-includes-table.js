/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/includes/table
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('includes', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<table>\n    <tr>\n        <th id=\"org\"><a class=\"url org\" href=\"http://dev.opera.com/\">Opera</a></th>\n    </tr>\n    <tr>\n        <td class=\"vcard\" headers=\"org\"><span class=\"fn\">Chris Mills</span></td>\n    </tr>\n    <tr>\n        <td class=\"vcard\" headers=\"org\"><span class=\"fn\">Erik Möller</span></td>\n    </tr>\n  </table>";
   var expected = {"items":[{"type":["h-card"],"properties":{"name":["Chris Mills"],"url":["http://dev.opera.com/"],"org":["Opera"]}},{"type":["h-card"],"properties":{"name":["Erik Möller"],"url":["http://dev.opera.com/"],"org":["Opera"]}}],"rels":{},"rel-urls":{}};

   it('table', function(){
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
