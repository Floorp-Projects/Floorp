/*
Unit test for dates
*/

assert = chai.assert;

// Tests the private Modules.dates object
// Modules.dates is unit tested as it has an interface access by other modules


describe('Modules.dates', function() {


   it('hasAM', function(){
       assert.isTrue( Modules.dates.hasAM( '5am' ) );
       assert.isTrue( Modules.dates.hasAM( '5AM' ) );
       assert.isTrue( Modules.dates.hasAM( '5 am' ) );
       assert.isTrue( Modules.dates.hasAM( '5a.m.' ) );
       assert.isTrue( Modules.dates.hasAM( '5:20 a.m.' ) );
       assert.isFalse( Modules.dates.hasAM( '5pm' ) );
   });


   it('hasPM', function(){
       assert.isTrue( Modules.dates.hasPM( '5pm' ) );
       assert.isTrue( Modules.dates.hasPM( '5PM' ) );
       assert.isTrue( Modules.dates.hasPM( '5 pm' ) );
       assert.isTrue( Modules.dates.hasPM( '5p.m.' ) );
       assert.isTrue( Modules.dates.hasPM( '5:20 p.m.' ) );
       assert.isFalse( Modules.dates.hasPM( '5am' ) );
   });


   it('removeAMPM', function(){
       assert.equal( Modules.dates.removeAMPM( '5pm' ), '5' );
       assert.equal( Modules.dates.removeAMPM( '5 pm' ), '5 ' );
       assert.equal( Modules.dates.removeAMPM( '5p.m.' ), '5' );
       assert.equal( Modules.dates.removeAMPM( '5am' ), '5' );
       assert.equal( Modules.dates.removeAMPM( '5a.m.' ), '5' );
       assert.equal( Modules.dates.removeAMPM( '5' ), '5' );
   });


   it('isDuration', function(){
       assert.isTrue( Modules.dates.isDuration( 'PY17M' ) );
       assert.isTrue( Modules.dates.isDuration( 'PW12' ) );
       assert.isTrue( Modules.dates.isDuration( 'P0.5Y' ) );
       assert.isTrue( Modules.dates.isDuration( 'P3Y6M4DT12H30M5S' ) );
       assert.isFalse( Modules.dates.isDuration( '2015-01-23T13:45' ) );
       assert.isFalse( Modules.dates.isDuration( '2015-01-23 13:45' ) );
       assert.isFalse( Modules.dates.isDuration( '20150123T1345' ) );
   });


   it('isTime', function(){
       assert.isTrue( Modules.dates.isTime( '8:43' ) );
       assert.isTrue( Modules.dates.isTime( '08:43' ) );
       assert.isTrue( Modules.dates.isTime( '15:23:00:0567' ) );
       assert.isTrue( Modules.dates.isTime( '10:34pm' ) );
       assert.isTrue( Modules.dates.isTime( '10:34 p.m.' ) );
       assert.isTrue( Modules.dates.isTime( '+01:00:00' ) );
       assert.isTrue( Modules.dates.isTime( '-02:00' ) );
       assert.isTrue( Modules.dates.isTime( 'z15:00' ) );
       assert.isTrue( Modules.dates.isTime( '0843' ) );
       assert.isFalse( Modules.dates.isTime( 'P3Y6M4DT12H30M5S' ) );
       assert.isFalse( Modules.dates.isTime( '2015-01-23T13:45' ) );
       assert.isFalse( Modules.dates.isTime( '2015-01-23 13:45' ) );
       assert.isFalse( Modules.dates.isTime( '20150123T1345' ) );
       assert.isFalse( Modules.dates.isTime( 'abc' ) );
       assert.isFalse( Modules.dates.isTime( '12345' ) );
   });


   it('parseAmPmTime', function(){
       assert.equal( Modules.dates.parseAmPmTime( '5am' ), '05' );
       assert.equal( Modules.dates.parseAmPmTime( '12pm' ), '12' );
       assert.equal( Modules.dates.parseAmPmTime( '5a.m.' ), '05' );
       assert.equal( Modules.dates.parseAmPmTime( '5pm' ), '17' );
       assert.equal( Modules.dates.parseAmPmTime( '5:34pm' ), '17:34' );
       assert.equal( Modules.dates.parseAmPmTime( '5:04pm' ), '17:04' );
       assert.equal( Modules.dates.parseAmPmTime( '05:34:00' ), '05:34:00' );
       assert.equal( Modules.dates.parseAmPmTime( '05:34:00' ), '05:34:00' );
       assert.equal( Modules.dates.parseAmPmTime( '1:52:04pm' ), '13:52:04' );
   });


   it('dateTimeUnion', function(){
       assert.equal( Modules.dates.dateTimeUnion( '2015-01-23', '05:34:00', 'HTML5' ).toString('HTML5'), '2015-01-23 05:34:00' );
       assert.equal( Modules.dates.dateTimeUnion( '2015-01-23', '05:34', 'HTML5' ).toString('HTML5'), '2015-01-23 05:34' );
       assert.equal( Modules.dates.dateTimeUnion( '2015-01-23', '05', 'HTML5' ).toString('HTML5'), '2015-01-23 05' );
       assert.equal( Modules.dates.dateTimeUnion( '2009-06-26T19:00', '2200', 'HTML5' ).toString('HTML5'), '2009-06-26 22:00' );

       assert.equal( Modules.dates.dateTimeUnion( '2015-01-23', '', 'HTML5' ).toString('HTML5'), '2015-01-23' );
       assert.equal( Modules.dates.dateTimeUnion( '', '', 'HTML5' ).toString('HTML5'), '' );
   });


   it('concatFragments', function(){
       assert.equal( Modules.dates.concatFragments( ['2015-01-23', '05:34:00'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34:00' );
       assert.equal( Modules.dates.concatFragments( ['05:34:00', '2015-01-23'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34:00' );
       assert.equal( Modules.dates.concatFragments( ['2015-01-23T05:34:00'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34:00' );
       assert.equal( Modules.dates.concatFragments( ['2015-01-23', '05:34:00', 'z'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34:00Z' );
       assert.equal( Modules.dates.concatFragments( ['2015-01-23', '05:34', '-01'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34-01' );
       assert.equal( Modules.dates.concatFragments( ['2015-01-23', '05:34', '-01:00'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34-01:00' );
       assert.equal( Modules.dates.concatFragments( ['2015-01-23', '05:34-01:00'], 'HTML5' ).toString('HTML5'), '2015-01-23 05:34-01:00' );

   });





});
