/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-entry/u-property
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-entry', function() {
   var htmlFragment = "<base href=\"http://example.com\">\n<div class=\"h-entry\">\n    <p class=\"p-name\">microformats.org at 7</p>\n\n    \n    <p class=\"u-url\">\n      <span class=\"value-title\" title=\"http://microformats.org/\"> </span>\n      Article permalink\n    </p>\n    <p class=\"u-url\">\n        <span class=\"value\">http://microformats.org/</span> - \n        <span class=\"value\">2012/06/25/microformats-org-at-7</span> \n    </p> \n\n    <p><a class=\"u-url\" href=\"http://microformats.org/2012/06/25/microformats-org-at-7\">Article permalink</a></p>\n\n    <img src=\"images/logo.gif\" alt=\"company logos\" usemap=\"#logomap\" />\n    <map name=\"logomap\">\n        <area class=\"u-url\" shape=\"rect\" coords=\"0,0,82,126\" href=\"http://microformats.org/\" alt=\"microformats.org\" />\n    </map>\n\n    <img class=\"u-photo\" src=\"images/logo.gif\" alt=\"company logos\" />\n\n    <object class=\"u-url\" data=\"http://microformats.org/wiki/microformats2-parsing\"></object>\n\n    <abbr class=\"u-url\" title=\"http://microformats.org/wiki/value-class-pattern\">value-class-pattern</abbr> \n    <data class=\"u-url\" value=\"http://microformats.org/wiki/\"></data>\n    <p class=\"u-url\">http://microformats.org/discuss</p>\n</div>";
   var expected = {"items":[{"type":["h-entry"],"properties":{"name":["microformats.org at 7"],"url":["http://microformats.org/","http://microformats.org/2012/06/25/microformats-org-at-7","http://microformats.org/2012/06/25/microformats-org-at-7","http://microformats.org/","http://microformats.org/wiki/microformats2-parsing","http://microformats.org/wiki/value-class-pattern","http://microformats.org/wiki/","http://microformats.org/discuss"],"photo":["http://example.com/images/logo.gif"]}}],"rels":{},"rel-urls":{}};

   it('u-property', function(){
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
