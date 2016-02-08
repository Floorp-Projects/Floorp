/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-resume/work
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-resume', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<div class=\"h-resume\">\n    <p class=\"p-name\">Tim Berners-Lee</p>\n    <div class=\"p-contact h-card\">\n        <p class=\"p-title\">Director of the World Wide Web Foundation</p>\n    </div>\n    <p class=\"p-summary\">Invented the World Wide Web.</p><hr />\n    <div class=\"p-experience h-event h-card\">\n        <p class=\"p-title\">Director</p>\n        <p><a class=\"p-name p-org u-url\" href=\"http://www.webfoundation.org/\">World Wide Web Foundation</a></p>\n        <p>\n            <time class=\"dt-start\" datetime=\"2009-01-18\">Jan 2009</time> – Present\n            <time class=\"dt-duration\" datetime=\"P2Y11M\">(2 years 11 month)</time>\n        </p>\n    </div>\n</div>";
   var expected = {"items":[{"type":["h-resume"],"properties":{"name":["Tim Berners-Lee"],"contact":[{"value":"Director of the World Wide Web Foundation","type":["h-card"],"properties":{"title":["Director of the World Wide Web Foundation"],"name":["Director of the World Wide Web Foundation"]}}],"summary":["Invented the World Wide Web."],"experience":[{"value":"World Wide Web Foundation","type":["h-event","h-card"],"properties":{"title":["Director"],"name":["World Wide Web Foundation"],"org":["World Wide Web Foundation"],"url":["http://www.webfoundation.org/"],"start":["2009-01-18"],"duration":["P2Y11M"]}}]}}],"rels":{},"rel-urls":{}};

   it('work', function(){
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
