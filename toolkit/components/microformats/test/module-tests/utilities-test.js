/*
Unit test for utilities
*/

assert = chai.assert;

// Tests the private Modules.utils object
// Modules.utils is unit tested as it has an interface access by other modules


describe('Modules.utilities', function() {


   it('isString', function(){
       assert.isTrue( Modules.utils.isString( 'abc' ) );
       assert.isFalse( Modules.utils.isString( 123 ) );
       assert.isFalse( Modules.utils.isString( 1.23 ) );
       assert.isFalse( Modules.utils.isString( {'abc': 'abc'} ) );
       assert.isFalse( Modules.utils.isString( ['abc'] ) );
       assert.isFalse( Modules.utils.isString( true ) );
   });


   it('isArray', function(){
       assert.isTrue( Modules.utils.isArray( ['abc'] ) );
       assert.isFalse( Modules.utils.isArray( 123 ) );
       assert.isFalse( Modules.utils.isArray( 1.23 ) );
       assert.isFalse( Modules.utils.isArray( 'abc' ) );
       assert.isFalse( Modules.utils.isArray( {'abc': 'abc'} ) );
       assert.isFalse( Modules.utils.isArray( true ) );
   });


   it('isNumber', function(){
       assert.isTrue( Modules.utils.isNumber( 123 ) );
       assert.isTrue( Modules.utils.isNumber( 1.23 ) );
       assert.isFalse( Modules.utils.isNumber( 'abc' ) );
       assert.isFalse( Modules.utils.isNumber( {'abc': 'abc'} ) );
       assert.isFalse( Modules.utils.isNumber( ['abc'] ) );
       assert.isFalse( Modules.utils.isNumber( true ) );
   });


   it('startWith', function(){
       assert.isTrue( Modules.utils.startWith( 'p-name', 'p-' ) );
       assert.isFalse( Modules.utils.startWith( 'p-name', 'name' ) );
       assert.isFalse( Modules.utils.startWith( 'p-name', 'u-' ) );
   });


   it('trim', function(){
       assert.equal( Modules.utils.trim( ' Glenn Jones ' ), 'Glenn Jones' );
       assert.equal( Modules.utils.trim( 'Glenn Jones' ), 'Glenn Jones' );
       assert.equal( Modules.utils.trim( undefined ), '' );
   });


   it('replaceCharAt', function(){
       assert.equal( Modules.utils.replaceCharAt( 'Glenn Jones', 5, '-' ), 'Glenn-Jones' );
       assert.equal( Modules.utils.replaceCharAt( 'Glenn Jones', 50, '-' ), 'Glenn Jones' );
   });


   it('isOnlyWhiteSpace', function(){
       assert.isTrue( Modules.utils.isOnlyWhiteSpace( '  ') );
       assert.isTrue( Modules.utils.isOnlyWhiteSpace( '  \n\r') );
       assert.isFalse( Modules.utils.isOnlyWhiteSpace( '  text\n\r') );
   });


   it('collapseWhiteSpace', function(){
       assert.equal( Modules.utils.collapseWhiteSpace( '  '), ' ' );
       assert.equal( Modules.utils.collapseWhiteSpace( '  \n\r'), ' ' );
       assert.equal( Modules.utils.collapseWhiteSpace( '  text\n\r'), ' text ' );
   });


   it('hasProperties', function(){
       assert.isTrue( Modules.utils.hasProperties( {name: 'glennjones'} ) );
       assert.isFalse( Modules.utils.hasProperties( {} ) );
   });


   it('sortObjects', function(){
       var arr = [{'name': 'one'},{'name': 'two'},{'name': 'three'},{'name': 'three'}];

       assert.deepEqual( arr.sort( Modules.utils.sortObjects( 'name', true ) ), [{"name":"two"},{"name":"three"},{'name': 'three'},{"name":"one"}] );
       assert.deepEqual( arr.sort( Modules.utils.sortObjects( 'name', false ) ), [{"name":"one"},{"name":"three"},{'name': 'three'},{"name":"two"}] );
   });



});
