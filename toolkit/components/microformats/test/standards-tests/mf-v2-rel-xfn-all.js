/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/rel/xfn-all
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('rel', function() {
   var htmlFragment = "<ul>\n    <li><a rel=\"friend\" href=\"http://example.com/profile/jane\">jane</a></li>\n    <li><a rel=\"acquaintance\" href=\"http://example.com/profile/jeo\">jeo</a></li>\n    <li><a rel=\"contact\" href=\"http://example.com/profile/lily\">lily</a></li>\n    <li><a rel=\"met\" href=\"http://example.com/profile/oliver\">oliver</a></li>\n    <li><a rel=\"co-worker\" href=\"http://example.com/profile/emily\">emily</a></li>\n    <li><a rel=\"colleague\" href=\"http://example.com/profile/jack\">jack</a></li>\n    <li><a rel=\"neighbor\" href=\"http://example.com/profile/isabella\">isabella</a></li>\n    <li><a rel=\"child\" href=\"http://example.com/profile/harry\">harry</a></li>\n    <li><a rel=\"parent\" href=\"http://example.com/profile/sophia\">sophia</a></li>\n    <li><a rel=\"sibling\" href=\"http://example.com/profile/charlie\">charlie</a></li>\n    <li><a rel=\"spouse\" href=\"http://example.com/profile/olivia\">olivia</a></li>\n    <li><a rel=\"kin\" href=\"http://example.com/profile/james\">james</a></li>\n    <li><a rel=\"muse\" href=\"http://example.com/profile/ava\">ava</a></li>\n    <li><a rel=\"crush\" href=\"http://example.com/profile/joshua\">joshua</a></li>\n    <li><a rel=\"date\" href=\"http://example.com/profile/chloe\">chloe</a></li>\n    <li><a rel=\"sweetheart\" href=\"http://example.com/profile/alfie\">alfie</a></li>\n    <li><a rel=\"me\" href=\"http://example.com/profile/isla\">isla</a></li>\n</ul>";
   var expected = {"items":[],"rels":{"friend":["http://example.com/profile/jane"],"acquaintance":["http://example.com/profile/jeo"],"contact":["http://example.com/profile/lily"],"met":["http://example.com/profile/oliver"],"co-worker":["http://example.com/profile/emily"],"colleague":["http://example.com/profile/jack"],"neighbor":["http://example.com/profile/isabella"],"child":["http://example.com/profile/harry"],"parent":["http://example.com/profile/sophia"],"sibling":["http://example.com/profile/charlie"],"spouse":["http://example.com/profile/olivia"],"kin":["http://example.com/profile/james"],"muse":["http://example.com/profile/ava"],"crush":["http://example.com/profile/joshua"],"date":["http://example.com/profile/chloe"],"sweetheart":["http://example.com/profile/alfie"],"me":["http://example.com/profile/isla"]},"rel-urls":{"http://example.com/profile/jane":{"text":"jane","rels":["friend"]},"http://example.com/profile/jeo":{"text":"jeo","rels":["acquaintance"]},"http://example.com/profile/lily":{"text":"lily","rels":["contact"]},"http://example.com/profile/oliver":{"text":"oliver","rels":["met"]},"http://example.com/profile/emily":{"text":"emily","rels":["co-worker"]},"http://example.com/profile/jack":{"text":"jack","rels":["colleague"]},"http://example.com/profile/isabella":{"text":"isabella","rels":["neighbor"]},"http://example.com/profile/harry":{"text":"harry","rels":["child"]},"http://example.com/profile/sophia":{"text":"sophia","rels":["parent"]},"http://example.com/profile/charlie":{"text":"charlie","rels":["sibling"]},"http://example.com/profile/olivia":{"text":"olivia","rels":["spouse"]},"http://example.com/profile/james":{"text":"james","rels":["kin"]},"http://example.com/profile/ava":{"text":"ava","rels":["muse"]},"http://example.com/profile/joshua":{"text":"joshua","rels":["crush"]},"http://example.com/profile/chloe":{"text":"chloe","rels":["date"]},"http://example.com/profile/alfie":{"text":"alfie","rels":["sweetheart"]},"http://example.com/profile/isla":{"text":"isla","rels":["me"]}}};

   it('xfn-all', function(){
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
