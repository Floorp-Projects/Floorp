/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v1/hfeed/simple
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('hfeed', function() {
   var htmlFragment = "<section class=\"hfeed\">\n\t<h1 class=\"name\">Microformats blog</h1>\n\t<span class=\"author vcard\"><a class=\"fn url\" href=\"http://tantek.com/\">Tantek</a></span>\n\t<a class=\"url\" href=\"http://microformats.org/blog\">permlink</a>\n\t<img class=\"photo\" src=\"photo.jpeg\"/>\n\t<p>\n\t\tTags: <a rel=\"tag\" href=\"tags/microformats\">microformats</a>, \n\t\t<a rel=\"tag\" href=\"tags/html\">html</a>\n\t</p>\n\t\n\t<div class=\"hentry\">\n\t    <h1><a class=\"entry-title\" rel=\"bookmark\" href=\"http://microformats.org/2012/06/25/microformats-org-at-7\">microformats.org at 7</a></h1>\n\t    <div class=\"entry-content\">\n\t        <p class=\"entry-summary\">Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.</p>\n\t\n\t        <p>The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service </p>\n\t    </div>  \n\t    <p>Updated \n\t        <time class=\"updated\" datetime=\"2012-06-25T17:08:26\">June 25th, 2012</time>\n\t    </p>\n\t</div>\n\t\n</section>";
   var expected = {"items":[{"type":["h-feed"],"properties":{"author":[{"value":"Tantek","type":["h-card"],"properties":{"name":["Tantek"],"url":["http://tantek.com/"]}}],"url":["http://microformats.org/blog"],"photo":["http://example.com/photo.jpeg"],"category":["microformats","html"]},"children":[{"value":"microformats.org at 7\n\t    \n\t        Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.\n\t\n\t        The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            principles, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service \n\t      \n\t    Updated \n\t        June 25th, 2012","type":["h-entry"],"properties":{"name":["microformats.org at 7"],"url":["http://microformats.org/2012/06/25/microformats-org-at-7"],"content":[{"value":"Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.\n\t\n\t        The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            principles, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service","html":"\n\t        <p class=\"entry-summary\">Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.</p>\n\t\n\t        <p>The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service </p>\n\t    "}],"summary":["Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities."],"updated":["2012-06-25 17:08:26"]}}]}],"rels":{"tag":["http://example.com/tags/microformats","http://example.com/tags/html"],"bookmark":["http://microformats.org/2012/06/25/microformats-org-at-7"]},"rel-urls":{"http://example.com/tags/microformats":{"text":"microformats","rels":["tag"]},"http://example.com/tags/html":{"text":"html","rels":["tag"]},"http://microformats.org/2012/06/25/microformats-org-at-7":{"text":"microformats.org at 7","rels":["bookmark"]}}};

   it('simple', function(){
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
