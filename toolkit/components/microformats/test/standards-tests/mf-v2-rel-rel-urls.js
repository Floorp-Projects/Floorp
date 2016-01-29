/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/rel/rel-urls
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('rel', function() {
   var htmlFragment = "<a rel=\"author\" href=\"http://example.com/a\">author a</a>\n<a rel=\"author\" href=\"http://example.com/b\">author b</a>\n<a rel=\"in-reply-to\" href=\"http://example.com/1\">post 1</a>\n<a rel=\"in-reply-to\" href=\"http://example.com/2\">post 2</a>\n<a rel=\"alternate home\"\n   href=\"http://example.com/fr\"\n   media=\"handheld\"\n   hreflang=\"fr\">French mobile homepage</a>";
   var expected = {"items":[],"rels":{"author":["http://example.com/a","http://example.com/b"],"in-reply-to":["http://example.com/1","http://example.com/2"],"home":["http://example.com/fr"],"alternate":["http://example.com/fr"]},"rel-urls":{"http://example.com/a":{"rels":["author"],"text":"author a"},"http://example.com/b":{"rels":["author"],"text":"author b"},"http://example.com/1":{"rels":["in-reply-to"],"text":"post 1"},"http://example.com/2":{"rels":["in-reply-to"],"text":"post 2"},"http://example.com/fr":{"rels":["alternate","home"],"media":"handheld","hreflang":"fr","text":"French mobile homepage"}}};

   it('rel-urls', function(){
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
