/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/hproduct/simpleproperties
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hproduct', function() {
   var htmlFragment = "<meta charset=\"utf-8\">\n<div class=\"hproduct\">\n    <h2 class=\"fn\">Raspberry Pi</h2>\n    <img class=\"photo\" src=\"http://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/RaspberryPi.jpg/320px-RaspberryPi.jpg\" />\n    <p class=\"description\">The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming.</p>\n    <a class=\"url\" href=\"http://www.raspberrypi.org/\">More info about the Raspberry Pi</a>\n    <p class=\"price\">£29.95</p>\n    <p class=\"review hreview\"><span class=\"rating\">4.5</span> out of 5</p>\n    <p>Categories: \n        <a rel=\"tag\" href=\"http://en.wikipedia.org/wiki/computer\" class=\"category\">Computer</a>, \n        <a rel=\"tag\" href=\"http://en.wikipedia.org/wiki/education\" class=\"category\">Education</a>\n    </p>\n</div>";
   var expected = {"items":[{"type":["h-product"],"properties":{"name":["Raspberry Pi"],"photo":["http://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/RaspberryPi.jpg/320px-RaspberryPi.jpg"],"description":[{"value":"The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming.","html":"The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It’s a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming."}],"url":["http://www.raspberrypi.org/"],"price":["£29.95"],"category":["Computer","Education"],"review":[{"value":"4.5 out of 5","type":["h-review"],"properties":{"rating":["4.5"]}}]}}],"rels":{"tag":["http://en.wikipedia.org/wiki/computer","http://en.wikipedia.org/wiki/education"]},"rel-urls":{"http://en.wikipedia.org/wiki/computer":{"text":"Computer","rels":["tag"]},"http://en.wikipedia.org/wiki/education":{"text":"Education","rels":["tag"]}}};

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
