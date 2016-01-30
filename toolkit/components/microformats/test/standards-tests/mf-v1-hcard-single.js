/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v1/hcard/single
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hcard', function() {
   var htmlFragment = "<div class=\"vcard\">\n        \n        <div class=\"fn n\"><span class=\"given-name sort-string\">John</span> Doe</div>\n        <div>Birthday: <abbr class=\"bday\" title=\"2000-01-01T00:00:00-08:00\">January 1st, 2000</abbr></div>\n        <div>Role: <span class=\"role\">Designer</span></div>\n        <div>Location: <abbr class=\"geo\" title=\"30.267991;-97.739568\">Brighton</abbr></div>\n        <div>Time zone: <abbr class=\"tz\" title=\"-05:00\">Eastern Standard Time</abbr></div>\n        \n        <div>Profile details:\n            <div>Profile id: <span class=\"uid\">http://example.com/profiles/johndoe</span></div>\n            <div>Details are: <span class=\"class\">Public</span></div>\n            <div>Last updated: <abbr class=\"rev\" title=\"2008-01-01T13:45:00\">January 1st, 2008 - 13:45</abbr></div>\n        </div>\n    </div>";
   var expected = {"items":[{"type":["h-card"],"properties":{"name":["John Doe"],"given-name":["John"],"sort-string":["John"],"bday":["2000-01-01 00:00:00-08:00"],"role":["Designer"],"geo":[{"value":"30.267991;-97.739568","type":["h-geo"],"properties":{"name":["30.267991;-97.739568"]}}],"tz":["-05:00"],"uid":["http://example.com/profiles/johndoe"],"class":["Public"],"rev":["2008-01-01 13:45:00"]}}],"rels":{},"rel-urls":{}};

   it('single', function(){
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
