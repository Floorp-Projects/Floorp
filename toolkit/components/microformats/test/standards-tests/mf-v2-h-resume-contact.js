/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-resume/contact
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-resume', function() {
   var htmlFragment = "<div class=\"h-resume\">\n    <p class=\"p-name\">Tim Berners-Lee</p>\n    <p class=\"p-summary\">Invented the World Wide Web.</p><hr />\n    <div class=\"p-contact h-card\">\n        <p class=\"p-name\">MIT</p>\n        <p>\n            <span class=\"p-street-address\">32 Vassar Street</span>, \n            <span class=\"p-extended-address\">Room 32-G524</span>, \n            <span class=\"p-locality\">Cambridge</span>,  \n            <span class=\"p-region\">MA</span> \n            <span class=\"p-postal-code\">02139</span>, \n            <span class=\"p-country-name\">USA</span>.\n        </p>\n        <p>Tel:<span class=\"p-tel\">+1 (617) 253 5702</span></p>\n        <p>Email:<a class=\"u-email\" href=\"mailto:timbl@w3.org\">timbl@w3.org</a></p>\n    </div>\n</div>";
   var expected = {"items":[{"type":["h-resume"],"properties":{"name":["Tim Berners-Lee"],"summary":["Invented the World Wide Web."],"contact":[{"value":"MIT","type":["h-card"],"properties":{"name":["MIT"],"street-address":["32 Vassar Street"],"extended-address":["Room 32-G524"],"locality":["Cambridge"],"region":["MA"],"postal-code":["02139"],"country-name":["USA"],"tel":["+1 (617) 253 5702"],"email":["mailto:timbl@w3.org"]}}]}}],"rels":{},"rel-urls":{}};

   it('contact', function(){
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
