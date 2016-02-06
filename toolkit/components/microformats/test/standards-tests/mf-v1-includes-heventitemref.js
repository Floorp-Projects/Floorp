/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/includes/heventitemref
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('includes', function() {
   var htmlFragment = "<div class=\"vevent\" itemref=\"io-session07\">\n    <span class=\"name\">Monetizing Android Apps</span> - spaekers: \n    <span class=\"speaker\">Chrix Finne</span>, \n    <span class=\"speaker\">Kenneth Lui</span> - \n    <span itemref=\"io-location\" class=\"location adr\">\n        <span class=\"extended-address\">Room 10</span>\n    </span>  \n</div>\n<div class=\"vevent\" itemref=\"io-session07\">\n    <span class=\"name\">New Low-Level Media APIs in Android</span> - spaekers: \n    <span class=\"speaker\">Dave Burke</span> -\n    <span itemref=\"io-location\" class=\"location adr\">\n        <span class=\"extended-address\">Room 11</span>\n    </span>  \n</div>\n\n<p id=\"io-session07\">\n    Session 01 is between: \n    <time class=\"dtstart\" datetime=\"2012-06-27T15:45:00-0800\">3:45PM</time> to \n    <time class=\"dtend\" datetime=\"2012-06-27T16:45:00-0800\">4:45PM</time> \n</p>   \n<p id=\"io-location\">\n    <span class=\"extended-address\">Moscone Center</span>,   \n    <span class=\"locality\">San Francisco</span>  \n</p>";
   var expected = {"items":[{"type":["h-event"],"properties":{"location":[{"value":"Room 10\n    \n    Moscone Center,   \n    San Francisco","type":["h-adr"],"properties":{"extended-address":["Room 10","Moscone Center"],"locality":["San Francisco"]}}],"start":["2012-06-27 15:45:00-08:00"],"end":["2012-06-27 16:45:00-08:00"]}},{"type":["h-event"],"properties":{"location":[{"value":"Room 11\n    \n    Moscone Center,   \n    San Francisco","type":["h-adr"],"properties":{"extended-address":["Room 11","Moscone Center"],"locality":["San Francisco"]}}],"start":["2012-06-27 15:45:00-08:00"],"end":["2012-06-27 16:45:00-08:00"]}}],"rels":{},"rel-urls":{}};

   it('heventitemref', function(){
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
