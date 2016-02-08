/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-product/aggregate
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-product', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<div class=\"h-product\">\n    <h2 class=\"p-name\">Raspberry Pi</h2>\n    <img class=\"u-photo\" src=\"http://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/RaspberryPi.jpg/320px-RaspberryPi.jpg\" />\n    <p class=\"e-description\">The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming.</p>\n    <a class=\"u-url\" href=\"http://www.raspberrypi.org/\">More info about the Raspberry Pi</a>\n    <p class=\"p-price\">£29.95</p>\n    <p class=\"p-review h-review-aggregate\">\n        <span class=\"p-rating h-rating\">\n            <span class=\"p-average\">9.2</span> out of \n            <span class=\"p-best\">10</span> \n            based on <span class=\"p-count\">178</span> reviews\n        </span>\n    </p>\n    <p>Categories: <span class=\"p-category\">Computer</span>, <span class=\"p-category\">Education</span></p>\n    <p class=\"p-brand h-card\">From: \n        <span class=\"p-name p-org\">The Raspberry Pi Foundation</span> - \n        <span class=\"p-locality\">Cambridge</span> \n        <span class=\"p-country-name\">UK</span>\n    </p>\n</div>";
   var expected = {"items":[{"type":["h-product"],"properties":{"name":["Raspberry Pi"],"photo":["http://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/RaspberryPi.jpg/320px-RaspberryPi.jpg"],"description":[{"value":"The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming.","html":"The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming."}],"url":["http://www.raspberrypi.org/"],"price":["£29.95"],"review":[{"value":"9.2 out of \n            10 \n            based on 178 reviews","type":["h-review-aggregate"],"properties":{"rating":[{"value":"9.2 out of \n            10 \n            based on 178 reviews","type":["h-rating"],"properties":{"average":["9.2"],"best":["10"],"count":["178"],"name":["9.2 out of \n            10 \n            based on 178 reviews"]}}],"name":["9.2 out of \n            10 \n            based on 178 reviews"]}}],"category":["Computer","Education"],"brand":[{"value":"The Raspberry Pi Foundation","type":["h-card"],"properties":{"name":["The Raspberry Pi Foundation"],"org":["The Raspberry Pi Foundation"],"locality":["Cambridge"],"country-name":["UK"]}}]}}],"rels":{},"rel-urls":{}};

   it('aggregate', function(){
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
