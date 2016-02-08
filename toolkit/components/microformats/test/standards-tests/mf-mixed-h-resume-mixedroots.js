/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-mixed/h-resume/mixedroots
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-resume', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<div class=\"h-resume\">\n    <div class=\"p-contact vcard\">\n        <p class=\"fn\">Tim Berners-Lee</p>\n        <p class=\"title\">Director of the World Wide Web Foundation</p>\n    </div>\n    <p class=\"p-summary\">Invented the World Wide Web.</p><hr />\n    <div class=\"p-experience vevent vcard\">\n        <p class=\"title\">Director</p>\n        <p><a class=\"fn org summary url\" href=\"http://www.webfoundation.org/\">World Wide Web Foundation</a></p>\n        <p>\n            <time class=\"dtstart\" datetime=\"2009-01-18\">Jan 2009</time> – Present\n            <time class=\"duration\" datetime=\"P2Y11M\">(2 years 11 month)</time>\n        </p>\n    </div>\n</div>";
   var expected = {"items":[{"type":["h-resume"],"properties":{"contact":[{"value":"Tim Berners-Lee","type":["h-card"],"properties":{"name":["Tim Berners-Lee"],"job-title":["Director of the World Wide Web Foundation"]}}],"summary":["Invented the World Wide Web."],"experience":[{"value":"World Wide Web Foundation","type":["h-event","h-card"],"properties":{"job-title":["Director"],"name":["World Wide Web Foundation"],"org":["World Wide Web Foundation"],"url":["http://www.webfoundation.org/"],"start":["2009-01-18"],"duration":["P2Y11M"]}}],"name":["Tim Berners-Lee\n        Director of the World Wide Web Foundation\n    \n    Invented the World Wide Web.\n    \n        Director\n        World Wide Web Foundation\n        \n            Jan 2009 – Present\n            (2 years 11 month)"]}}],"rels":{},"rel-urls":{}};

   it('mixedroots', function(){
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
