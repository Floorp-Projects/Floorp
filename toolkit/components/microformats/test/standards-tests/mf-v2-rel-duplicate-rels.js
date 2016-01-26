/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/rel/duplicate-rels
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('rel', function() {
   var htmlFragment = "<a href=\"http://ma.tt/2015/05/beethoven-mozart-bach/\" \n    title=\"Permalink to Beethoven, Mozart, Bach\" rel=\"bookmark\">\n<time class=\"entry-date\" datetime=\"2015-05-31T22:42:00+00:00\">May 31, 2015</time></a></span>\n<a href=\"http://ma.tt/category/asides/\" rel=\"category tag\">Asides</a>\n<span class=\"author vcard\">\n<a class=\"url fn n\" href=\"http://ma.tt/author/saxmatt/\" \n    title=\"View all posts by Matt\" rel=\"author\">Matt</a></span>\n<span class=\"date\"><a href=\"http://ma.tt/2015/06/jefferson-on-idleness/\" title=\"Permalink to Jefferson on Idleness\" rel=\"bookmark\"><time class=\"entry-date\" datetime=\"2015-06-02T21:26:00+00:00\">June 2, 2015</time></a></span>\n<span class=\"categories-links\"><a href=\"http://ma.tt/category/asides/\" rel=\"category tag\">Asides</a></span>\n<span class=\"author vcard\"><a class=\"url fn n\" href=\"http://ma.tt/author/saxmatt/\" title=\"View all posts by Matt\" rel=\"author\">Matt</a></span>\n";
   var expected = {"rels":{"bookmark":["http://ma.tt/2015/05/beethoven-mozart-bach/","http://ma.tt/2015/06/jefferson-on-idleness/"],"category":["http://ma.tt/category/asides/"],"tag":["http://ma.tt/category/asides/"],"author":["http://ma.tt/author/saxmatt/"]},"items":[{"type":["h-card"],"properties":{"url":["http://ma.tt/author/saxmatt/"],"name":["Matt"]}},{"type":["h-card"],"properties":{"url":["http://ma.tt/author/saxmatt/"],"name":["Matt"]}}],"rel-urls":{"http://ma.tt/category/asides/":{"rels":["category","tag"],"text":"Asides"},"http://ma.tt/author/saxmatt/":{"rels":["author"],"text":"Matt","title":"View all posts by Matt"},"http://ma.tt/2015/05/beethoven-mozart-bach/":{"rels":["bookmark"],"text":"May 31, 2015","title":"Permalink to Beethoven, Mozart, Bach"},"http://ma.tt/2015/06/jefferson-on-idleness/":{"rels":["bookmark"],"text":"June 2, 2015","title":"Permalink to Jefferson on Idleness"}}};

   it('duplicate-rels', function(){
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
