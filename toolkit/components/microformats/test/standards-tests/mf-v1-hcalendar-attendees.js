/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v1/hcalendar/attendees
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hcalendar', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<div class=\"vevent\">\n    <span class=\"summary\">CPJ Online Press Freedom Summit</span>\n    (<time class=\"dtstart\" datetime=\"2012-10-10\">10 Nov 2012</time>) in\n    <span class=\"location\">San Francisco</span>.\n    Attendees:\n    <ul>\n        <li class=\"attendee vcard\"><span class=\"fn\">Brian Warner</span></li>\n        <li class=\"attendee vcard\"><span class=\"fn\">Kyle Machulis</span></li>\n        <li class=\"attendee vcard\"><span class=\"fn\">Tantek Çelik</span></li>\n        <li class=\"attendee vcard\"><span class=\"fn\">Sid Sutter</span></li>\n    </ul>\n</div>\n";
   var expected = {"items":[{"type":["h-event"],"properties":{"name":["CPJ Online Press Freedom Summit"],"start":["2012-10-10"],"location":["San Francisco"],"attendee":[{"value":"Brian Warner","type":["h-card"],"properties":{"name":["Brian Warner"]}},{"value":"Kyle Machulis","type":["h-card"],"properties":{"name":["Kyle Machulis"]}},{"value":"Tantek Çelik","type":["h-card"],"properties":{"name":["Tantek Çelik"]}},{"value":"Sid Sutter","type":["h-card"],"properties":{"name":["Sid Sutter"]}}]}}],"rels":{},"rel-urls":{}};

   it('attendees', function(){
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
