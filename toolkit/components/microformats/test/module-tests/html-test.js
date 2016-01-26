/*
Unit test for html
*/

assert = chai.assert;

// Tests the private Modules.html object 
// Modules.html is unit tested as it has an interface access by other modules  

describe('Modules.html', function() {
    
    
   it('parse', function(){
       var html = '<a href="http://glennjones.net">Glenn Jones</a>',
           bloghtml = '<section id="content" class="body"><ol id="posts-list" class="h-feed"><li><article class="h-entry"><header><h2 class="p-namee"><a href="#" rel="bookmark" title="Permalink to this POST TITLE">This be the title</a></h2></header><footer class="post-info"><abbr class="dt-published" title="2005-10-10T14:07:00-07:00">10th October 2005</abbr><address class="h-card p-author">By <a class="u-url p-name" href="#">Enrique Ramírez</a></address></footer><div class="e-content"><p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque venenatis nunc vitae libero iaculis elementum. Nullam et justo <a href="#">non sapien</a> dapibus blandit nec et leo. Ut ut malesuada tellus.</p></div></article></li></ol></section>',
           node = document.createElement('div');
           
       node.innerHTML = html;
       assert.equal(Modules.html.parse( node ), html );
       
       // make sure excludes 'data-include' marked items
       var child = document.createElement('p');
       child.setAttribute('data-include', 'true');
       node.appendChild(child);
       assert.equal( Modules.html.parse( node ), html );
       
       node = document.createElement('div');
       node.innerHTML = bloghtml;
       assert.equal( Modules.html.parse( node ), bloghtml );
       
       node = document.createElement('div');
       assert.equal( Modules.html.parse( node ), '' );
       
       child = document.createElement('br');
       node.appendChild(child);
       assert.equal( Modules.html.parse( node ), '<br />' );
       
       node = document.createComment('test comment');
       assert.equal( Modules.html.parse( node ), '' );
       
   });
  

 

   
   
   
   
});