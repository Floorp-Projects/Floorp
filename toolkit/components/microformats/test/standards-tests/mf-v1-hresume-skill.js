/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v1/hresume/skill
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hresume', function() {
   var htmlFragment = "<div class=\"hresume\"> \n    <p>\n        <span class=\"contact vcard\"><span class=\"fn\">Tim Berners-Lee</span></span>, \n        <span class=\"summary\">invented the World Wide Web</span>.\n    </p>\n    Skills:     \n    <ul>\n        <li><a class=\"skill\" rel=\"tag\" href=\"http://example.com/skills/informationsystems\">information systems</a></li>\n        <li><a class=\"skill\" rel=\"tag\" href=\"http://example.com/skills/advocacy\">advocacy</a></li>\n        <li><a class=\"skill\" rel=\"tag\" href=\"http://example.com/skills/leadership\">leadership</a></li>\n    </ul>\n</div>";
   var expected = {"items":[{"type":["h-resume"],"properties":{"contact":[{"value":"Tim Berners-Lee","type":["h-card"],"properties":{"name":["Tim Berners-Lee"]}}],"summary":["invented the World Wide Web"],"skill":["information systems","advocacy","leadership"]}}],"rels":{"tag":["http://example.com/skills/informationsystems","http://example.com/skills/advocacy","http://example.com/skills/leadership"]},"rel-urls":{"http://example.com/skills/informationsystems":{"text":"information systems","rels":["tag"]},"http://example.com/skills/advocacy":{"text":"advocacy","rels":["tag"]},"http://example.com/skills/leadership":{"text":"leadership","rels":["tag"]}}};

   it('skill', function(){
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
