/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-review/vcard
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-review', function() {
   var htmlFragment = "<div class=\"h-review\">\n    <span><span class=\"p-rating\">5</span> out of 5 stars</span>\n    <h4 class=\"p-name\">Crepes on Cole is awesome</h4>\n    <span class=\"p-reviewer h-card\">\n        Reviewer: <span class=\"p-name\">Tantek</span> - \n    </span>\n    <time class=\"dt-reviewed\" datetime=\"2005-04-18\">April 18, 2005</time>\n    <div class=\"e-description\">\n        <p class=\"p-item h-card\">\n        <span class=\"p-name p-org\">Crepes on Cole</span> is one of the best little \n        creperies in <span class=\"p-adr h-adr\"><span class=\"p-locality\">San Francisco</span></span>.\n        Excellent food and service. Plenty of tables in a variety of sizes \n        for parties large and small.  Window seating makes for excellent \n        people watching to/from the N-Judah which stops right outside.  \n        I've had many fun social gatherings here, as well as gotten \n        plenty of work done thanks to neighborhood WiFi.\n        </p>\n    </div>\n    <p>Visit date: <span>April 2005</span></p>\n    <p>Food eaten: <a class=\"p-category\" href=\"http://en.wikipedia.org/wiki/crepe\">crepe</a></p>\n    <p>Permanent link for review: <a class=\"u-url\" href=\"http://example.com/crepe\">http://example.com/crepe</a></p>\n    <p><a rel=\"license\" href=\"http://en.wikipedia.org/wiki/Wikipedia:Text_of_Creative_Commons_Attribution-ShareAlike_3.0_Unported_License\">Creative Commons Attribution-ShareAlike License</a></p>\n</div>";
   var expected = {"items":[{"type":["h-review"],"properties":{"rating":["5"],"name":["Crepes on Cole is awesome"],"reviewer":[{"value":"Tantek","type":["h-card"],"properties":{"name":["Tantek"]}}],"reviewed":["2005-04-18"],"description":[{"value":"Crepes on Cole is one of the best little \n        creperies in San Francisco.\n        Excellent food and service. Plenty of tables in a variety of sizes \n        for parties large and small.  Window seating makes for excellent \n        people watching to/from the N-Judah which stops right outside.  \n        I've had many fun social gatherings here, as well as gotten \n        plenty of work done thanks to neighborhood WiFi.","html":"\n        <p class=\"p-item h-card\">\n        <span class=\"p-name p-org\">Crepes on Cole</span> is one of the best little \n        creperies in <span class=\"p-adr h-adr\"><span class=\"p-locality\">San Francisco</span></span>.\n        Excellent food and service. Plenty of tables in a variety of sizes \n        for parties large and small.  Window seating makes for excellent \n        people watching to/from the N-Judah which stops right outside.  \n        I've had many fun social gatherings here, as well as gotten \n        plenty of work done thanks to neighborhood WiFi.\n        </p>\n    "}],"item":[{"value":"Crepes on Cole","type":["h-card"],"properties":{"name":["Crepes on Cole"],"org":["Crepes on Cole"],"adr":[{"value":"San Francisco","type":["h-adr"],"properties":{"locality":["San Francisco"],"name":["San Francisco"]}}]}}],"category":["crepe"],"url":["http://example.com/crepe"]}}],"rels":{"license":["http://en.wikipedia.org/wiki/Wikipedia:Text_of_Creative_Commons_Attribution-ShareAlike_3.0_Unported_License"]},"rel-urls":{"http://en.wikipedia.org/wiki/Wikipedia:Text_of_Creative_Commons_Attribution-ShareAlike_3.0_Unported_License":{"text":"Creative Commons Attribution-ShareAlike License","rels":["license"]}}};

   it('vcard', function(){
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
