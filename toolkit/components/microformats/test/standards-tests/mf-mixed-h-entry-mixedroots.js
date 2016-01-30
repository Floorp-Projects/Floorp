/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-mixed/h-entry/mixedroots
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-entry', function() {
   var htmlFragment = "<!-- simplified version of http://aaronparecki.com/notes/2013/10/18/2/realtimeconf-mapattack -->\n<base href=\"http://aaronparecki.com/\" />\n\n<div class=\"h-entry\">\n  <div class=\"h-card vcard author p-author\">\n    <img class=\"photo logo u-photo u-logo\" src=\"https://aaronparecki.com/images/aaronpk.png\" alt=\"Aaron Parecki\"/>\n    <a href=\"https://aaronparecki.com/\" rel=\"author\" class=\"u-url u-uid url\">aaronparecki.com</a>\n    <a class=\"p-name fn value\" href=\"https://aaronparecki.com/\">Aaron Parecki</a>\n    <a href=\"https://plus.google.com/117847912875913905493\" rel=\"author\" class=\"google-profile\">Aaron Parecki</a>\n  </div>\n  <div class=\"entry-content e-content p-name\">Did you play\n    <a href=\"http://twitter.com/playmapattack\">@playmapattack</a>at\n    <a href=\"/tag/realtimeconf\">#<span class=\"p-category\">realtimeconf</span></a>? Here is some more info about how we built it!\n    <a href=\"http://pdx.esri.com/blog/2013/10/17/introducting-mapattack/\"><span class=\"protocol\">http://</span>pdx.esri.com/blog/2013/10/17/introducting-mapattack/</a>\n  </div>\n</div>";
   var expected = {"items":[{"type":["h-entry"],"properties":{"author":[{"value":"aaronparecki.com\n    Aaron Parecki\n    Aaron Parecki","type":["h-card"],"properties":{"photo":["https://aaronparecki.com/images/aaronpk.png"],"logo":["https://aaronparecki.com/images/aaronpk.png"],"url":["https://aaronparecki.com/"],"uid":["https://aaronparecki.com/"],"name":["Aaron Parecki"]}}],"content":[{"value":"Did you play\n    @playmapattackat\n    #realtimeconf? Here is some more info about how we built it!\n    http://pdx.esri.com/blog/2013/10/17/introducting-mapattack/","html":"Did you play\n    <a href=\"http://twitter.com/playmapattack\">@playmapattack</a>at\n    <a href=\"http://aaronparecki.com/tag/realtimeconf\">#<span class=\"p-category\">realtimeconf</span></a>? Here is some more info about how we built it!\n    <a href=\"http://pdx.esri.com/blog/2013/10/17/introducting-mapattack/\"><span class=\"protocol\">http://</span>pdx.esri.com/blog/2013/10/17/introducting-mapattack/</a>\n  "}],"name":["Did you play\n    @playmapattackat\n    #realtimeconf? Here is some more info about how we built it!\n    http://pdx.esri.com/blog/2013/10/17/introducting-mapattack/"],"category":["realtimeconf"]}}],"rels":{"author":["https://aaronparecki.com/","https://plus.google.com/117847912875913905493"]},"rel-urls":{"https://aaronparecki.com/":{"text":"aaronparecki.com","rels":["author"]},"https://plus.google.com/117847912875913905493":{"text":"Aaron Parecki","rels":["author"]}}};

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
