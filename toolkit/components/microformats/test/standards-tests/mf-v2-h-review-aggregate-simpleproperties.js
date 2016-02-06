/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-review-aggregate/simpleproperties
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-review-aggregate', function() {
   var htmlFragment = "<div class=\"h-review-aggregate\">\n    <div class=\"p-item h-card\">\n        <h3 class=\"p-name\">Mediterranean Wraps</h3>\n        <p>\n            <span class=\"p-street-address\">433 S California Ave</span>, \n            <span class=\"p-locality\">Palo Alto</span>, \n            <span class=\"p-region\">CA</span> - \n            <span class=\"p-tel\">(650) 321-8189</span>\n        </p>\n    </div> \n    <span class=\"p-summary\">Customers flock to this small restaurant for their \n    tasty falafel and shawerma wraps and welcoming staff.</span>\n    <span class=\"p-rating\">\n        <span class=\"p-average value\">9.2</span> out of \n        <span class=\"p-best\">10</span> \n        based on <span class=\"p-count\">17</span> reviews\n    </span>\n</div>";
   var expected = {"items":[{"type":["h-review-aggregate"],"properties":{"item":[{"value":"Mediterranean Wraps","type":["h-card"],"properties":{"name":["Mediterranean Wraps"],"street-address":["433 S California Ave"],"locality":["Palo Alto"],"region":["CA"],"tel":["(650) 321-8189"]}}],"summary":["Customers flock to this small restaurant for their \n    tasty falafel and shawerma wraps and welcoming staff."],"rating":["9.2"],"average":["9.2"],"best":["10"],"count":["17"],"name":["Mediterranean Wraps\n        \n            433 S California Ave, \n            Palo Alto, \n            CA - \n            (650) 321-8189\n        \n     \n    Customers flock to this small restaurant for their \n    tasty falafel and shawerma wraps and welcoming staff.\n    \n        9.2 out of \n        10 \n        based on 17 reviews"]}}],"rels":{},"rel-urls":{}};

   it('simpleproperties', function(){
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
