<?php 
/*
Script Name: Full Featured PHP Browser/OS detection
Author: Harald Hope, Website: http://tech.ratmachines.com/
Script Source URI: http://tech.ratmachines.com/downloads/browser_detection.php
Version 4.2.7
Copyright (C) 04 March 2004

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

Lesser GPL license text:
http://www.gnu.org/licenses/lgpl.txt

Coding conventions:
http://cvs.sourceforge.net/viewcvs.py/phpbb/phpBB2/docs/codingstandards.htm?rev=1.3
*/

/****************************************** 
this is currently set to accept 9 parameters, although you can add as many as you want:
1. safe - returns true/false, you can determine what makes the browser be safe lower down, 
  currently it's set for ns4 and pre version 1 mozillas not being safe, plus all older browsers
2. ie_version - tests to see what general IE it is, ie5x-6, ie4, or macIE, returns these values.
3. moz_version - returns array of moz version, version number (includes full version, + etc), rv number (for math
  comparison), rv number (for full rv, including alpha and beta versions), and release date
4. dom - returns true/false if it is a basic dom browser, ie >= 5, opera >= 5, all new mozillas, safaris, konquerors
5. os - returns which os is being used
6. os_number - returns windows versions, 95, 98, me, nt 4, nt 5 [windows 2000], nt 5.1 [windows xp],
  otherwise returns false
7. browser - returns the browser name, in shorthand: ie, ie4, ie5x, op, moz, konq, saf, ns4
8. number - returns the browser version number, if available, otherwise returns '' [not available]
9. full - returns this array: $browser_name, $version_number, $ie_version, $dom_browser, $safe_browser, $os, $os_number
*******************************************/

// main script, uses two other functions, which_os() and browser_version() as needed
function browser_detection( $which_test ) {
  /*
  uncomment the global variable declaration if you want the variables to be available on a global level
  throughout your php page, make sure that php is configured to support the use of globals first!
  Use of globals should be avoided however, and they are not necessary with this script
  */

  /*global $dom_browser, $safe_browser, $browser_user_agent, $os, $browser_name, $ie_version, 
  $version_number, $os_number, $b_repeat, $moz_version, $moz_version_number, $moz_rv, $moz_rv_full, $moz_release;*/

  static $dom_browser, $safe_browser, $browser_user_agent, $os, $browser_name, $ie_version, 
  $version_number, $os_number, $b_repeat, $moz_version, $moz_version_number, $moz_rv, $moz_rv_full, $moz_release;

  /* 
  this makes the test only run once no matter how many times you call it
  since all the variables are filled on the first run through, it's only a matter of returning the
  the right ones 
  */
  if ( !$b_repeat )
  {
    //initialize all variables with default values to prevent error
    $dom_browser = false;
    $safe_browser = false;
    $os = '';
    $os_number = '';
    $a_os_data = '';
    $browser_name = '';
    $version_number = '';
    $ie_version = '';
    $moz_version = '';
    $moz_version_number = '';
    $moz_rv = '';
    $moz_rv_full = '';
    $moz_release = '';

    //make navigator user agent string lower case to make sure all versions get caught
    $browser_user_agent = strtolower( $_SERVER['HTTP_USER_AGENT'] );
    
    $a_os_data = which_os( $browser_user_agent );// get os/number array
    $os = $a_os_data[0];// os name, abbreviated
    $os_number = $a_os_data[1];// os number if windows

    /*
    pack the browser type array, in this order
    the order is important, because opera must be tested first, and ie4 tested for before ie general
    same for konqueror, then safari, then gecko, since safari navigator user agent id's with 'gecko' in string.
    note that $dom_browser is set for all  modern dom browsers, this gives you a default to use.

    array[0] = id string for useragent, array[1] is if dom capable, array[2] is working name for browser

    Note: all browser strings are in lower case to match the strtolower output, this avoids possible detection
    errors
    */
    $a_browser_types[] = array( 'opera', true, 'op' );
    $a_browser_types[] = array( 'msie', true, 'ie' );
    $a_browser_types[] = array( 'konqueror', true, 'konq' );
    $a_browser_types[] = array( 'safari', true, 'saf' );
    // covers Netscape 6-7, K-Meleon, Most linux versions
    // the 3 is for the rv: number, the release date is hardcoded
    $a_browser_types[] = array( 'gecko', true, 'moz' );
    // netscape 4 test: this has to be last or else ie or opera might register true
    $a_browser_types[] = array( 'mozilla/4', false, 'ns4' );
    $a_browser_types[] = array( 'lynx', false, 'lynx' );
    $a_browser_types[] = array( 'webtv', true, 'webtv' );
    // search engine spider bots:
    $a_browser_types[] = array( 'googlebot', false, 'google' );// google
    $a_browser_types[] = array( 'fast-webcrawler', false, 'fast' );// Fast AllTheWeb
    $a_browser_types[] = array( 'msnbot', false, 'msn' );// msn search 
    $a_browser_types[] = array( 'scooter', false, 'scooter' );// altavista
    //$a_browser_types[] = array( '', false ); // browser array template

    /* 
    moz types array
    note the order, netscape6 must come before netscape, which  is how netscape 7 id's itself.
    rv comes last in case it is plain old mozilla 
    */
    $moz_types = array( 'firebird', 'phoenix', 'firefox', 'galeon', 'k-meleon', 'camino', 'netscape6', 
    'netscape', 'rv' );

    /*
    run through the browser_types array, break if you hit a match, if no match, assume old browser
    or non dom browser, assigns false value to $b_success.
    */
    for ($i = 0; $i < count($a_browser_types); $i++)
    {
      //unpacks browser array, assigns to variables
      $s_browser = $a_browser_types[$i][0];// text string to id browser from array
      $b_dom = $a_browser_types[$i][1];// hardcoded dom support from array
      $browser_name = $a_browser_types[$i][2];// working name for browser
      $b_success = false;

      if (stristr($browser_user_agent, $s_browser)) 
      {
        // it defaults to true, will become false below if needed
        // this keeps it easier to keep track of what is safe, only explicit false assignment will make it false.
        $safe_browser = true;
        switch ( $browser_name )
        {
          case 'ns4':
            $safe_browser = false;
            break;
          case 'moz':
            /*
            note: The 'rv' test is not absolute since the rv number is very different on 
            different versions, for example Galean doesn't use the same rv version as Mozilla, 
            neither do later Netscapes, like 7.x. For more on this, read the full mozilla numbering 
            conventions here:
            http://www.mozilla.org/releases/cvstags.html
            */

            // this will return alpha and beta version numbers, if present
            $moz_rv_full = browser_version( $browser_user_agent, 'rv' );
            // this slices them back off for math comparisons
            $moz_rv = substr( $moz_rv_full, 0, 3 );

            // this is to pull out specific mozilla versions, firebird, netscape etc..
            for ( $i = 0; $i < count( $moz_types ); $i++ )
            {
              if ( stristr( $browser_user_agent, $moz_types[$i] ) ) 
              {
                $moz_version = $moz_types[$i];
                $moz_version_number = browser_version( $browser_user_agent, $moz_version );
                break;
              }
            }
            // this is necesary to protect against false id'ed moz'es and new moz'es.
            // this corrects for galeon, or any other moz browser without an rv number
            if ( !$moz_rv ) 
            { 
              $moz_rv = substr( $moz_version_number, 0, 3 ); 
              $moz_rv_full = $moz_version_number; 
              /* 
              // you can use this instead if you are running php >= 4.2
              $moz_rv = floatval( $moz_version_number ); 
              $moz_rv_full = $moz_version_number;
              */
            }
            // this corrects the version name in case it went to the default 'rv' for the test
            if ( $moz_version == 'rv' ) 
            {
              $moz_version = 'mozilla';
            }
            
            //the moz version will be taken from the rv number, see notes above for rv problems
            $version_number = $moz_rv;
            // gets the actual release date, necessary if you need to do functionality tests
            $moz_release = browser_version( $browser_user_agent, 'gecko/' );
            /* 
            Test for mozilla 0.9.x / netscape 6.x
            test your javascript/CSS to see if it works in these mozilla releases, if it does, just default it to:
            $safe_browser = true;
            */
            if ( ( $moz_release < 20020400 ) || ( $moz_rv < 1 ) )
            {
              $safe_browser = false;
            }
            break;
          case 'ie':
            //$version_number = browser_version( $browser_user_agent, $s_browser, $substring_length );
            $version_number = browser_version( $browser_user_agent, $s_browser );
            if ( $os == 'mac' )
            {
              $ie_version = 'ieMac';
            }
            // this assigns a general ie id to the $ie_version variable
            if ( $version_number >= 5 )
            {
              $ie_version = 'ie5x';
            }
            elseif ( ( $version_number > 3 ) && $version_number < 5 )
            {
              $dom_browser = false;
              $ie_version = 'ie4';
              $safe_browser = true; // this depends on what you're using the script for, make sure this fits your needs
            }
            else
            {
              $ie_version = 'old';
              $safe_browser = false; 
            }
            break;
          case 'op':
            //$version_number = browser_version( $browser_user_agent, $s_browser, $substring_length );
            $version_number = browser_version( $browser_user_agent, $s_browser );
            if ( $version_number < 5 )// opera 4 wasn't very useable.
            {
              $safe_browser = false; 
            }
            break;
          case 'saf':
            //$version_number = browser_version( $browser_user_agent, $s_browser, $substring_length );
            $version_number = browser_version( $browser_user_agent, $s_browser );
            break;
          default:
            break;
        }

        $dom_browser = $b_dom;
        $b_success = true;

        break;
      }
    }
    //assigns defaults if the browser was not found in the loop test
    if ( !$b_success ) 
    {
      $safe_browser = false;
      $dom_browser = false;
      $browser_name = '';
    }
    // this ends the run through once if clause, set the boolean 
    //to true so the function won't retest everything
    $b_repeat = true;
  }
  /*
  This is where you return values based on what parameter you used to call the function
  $which_test is the passed parameter in the initial browser_detection('os') for example call
  */
  switch ( $which_test )
  {
    case 'safe':// returns true/false if your tests determine it's a safe browser
      // you can change the tests to determine what is a safeBrowser for your scripts
      // in this case sub rv 1 Mozillas and Netscape 4x's trigger the unsafe condition
      return $safe_browser; 
      break;
    case 'ie_version': // returns iemac or ie5x
      return $ie_version;
      break;
    case 'moz_version':// returns array of all relevant moz information
      $moz_array = array( $moz_version, $moz_version_number, $moz_rv, $moz_rv_full, $moz_release );
      return $moz_array;
      break;
    case 'dom':// returns true/fale if a DOM capable browser
      return $dom_browser;
      break;
    case 'os':// returns os name
      return $os; 
      break;
    case 'os_number':// returns os number if windows
      return $os_number;
      break;
    case 'browser':// returns browser name
      return $browser_name; 
      break;
    case 'number':// returns browser number
      return $version_number;
      break;
    case 'full':// returns all relevant browser information in an array
      $full_array = array( $browser_name, $version_number, $ie_version, $dom_browser, $safe_browser, $os, $os_number );
      return $full_array;
      break;
    default:
      break;
  }
}

// gets which os from the browser string
function which_os ( $browser_string )
{
  // initialize variables
  $os = '';
  $os_version = '';
  /*
  packs the os array
  use this order since some navigator user agents will put 'macintosh' in the navigator user agent string
  which would make the nt test register true
  */
  $a_os = array('lin', 'mac', 'unix', 'sunos', 'bsd', 'nt', 'win');

  //os tester
  for ($i = 0; $i < count($a_os); $i++)
  {
    //unpacks os array, assigns to variable
    $s_os = $a_os[$i];

    //assign os to global os variable, os flag true on success
    if (stristr( $browser_string, $s_os )) 
    {
      $os = $s_os;

      switch ( $os )
      {
        case 'win':
          if ( strstr( $browser_string, '95' ) )
          {
            $os_version = '95';
          }
          elseif ( strstr( $browser_string, '98' ) )
          {
            $os_version = '98';
          }
          elseif ( strstr( $browser_string, 'me' ) )
          {
            $os_version = 'me';
          }
          elseif ( strstr( $browser_string, '2000' ) )// windows 2000, for opera ID
          {
            $os_version = '5.0';
            $os = 'nt';
          }
          elseif ( strstr( $browser_string, 'xp' ) )// windows 2000, for opera ID
          {
            $os_version = '5.1';
            $os = 'nt';
          }
          break;
        case 'nt':
          if ( strstr( $browser_string, 'nt 5.1' || strstr( $browser_string, 'xp' )) )// windows xp
          {
            $os_version = '5.1';//
          }
          elseif ( strstr( $browser_string, 'nt 5' ) || strstr( $browser_string, '2000' ) )// windows 2000
          {
            $os_version = '5.0';
          }
          elseif ( strstr( $browser_string, 'nt 4' ) )// nt 4
          {
            $os_version = '4';
          }
          elseif ( strstr( $browser_string, 'nt 3' ) )// nt 4
          {
            $os_version = '3';
          }
          break;
        default:
          break;
      }
      break;
    }
  }

  // pack the os data array for return to main function
  $os_data = array( $os, $os_version );
  return $os_data;
}

// function returns browser number, gecko rv number, or gecko release date
//function browser_version( $browser_user_agent, $search_string, $substring_length )
function browser_version( $browser_user_agent, $search_string )
{
  // 8 is the longest that will be required, handles release dates: 20020323; 0.8.0+
  $substring_length = 8;
  //initialize browser number, will return '' if not found
  $browser_number = '';

  // use the passed parameter for $search_string
  // start the substring slice right after these moz search strings
  $start_pos = strpos( $browser_user_agent, $search_string ) + strlen( $search_string );

  // this is just to get the release date, not other moz information
  if ( ( $search_string != 'gecko/' ) ) 
  { 
    $start_pos++; 
  }

  // Initial trimming
  $browser_number = substr( $browser_user_agent, $start_pos, $substring_length );

  // Find the space, ;, or parentheses that ends the number
  $browser_number = substr( $browser_number, 0, strcspn($browser_number, ' );') );

  //make sure the returned value is actually the id number and not a string
  // otherwise return ''
  if ( !is_numeric( substr( $browser_number, 0, 1 ) ) )
  { 
    $browser_number = ''; 
  }

  return $browser_number;
}

/* 
Here are some typical navigator.userAgent strings so you can see where the data comes from
Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.5) Gecko/20031007 Firebird/0.7 
Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:0.9.4) Gecko/20011128 Netscape6/6.2.1
*/
?>