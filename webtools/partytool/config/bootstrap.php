<?php
/* You should specify a Google Map API key here. Without it, all mapping features
 * will be disabled. To obtain a key, visit http://www.google.com/apis/maps/
 */ 
define('GMAP_API_KEY', '');

/* The search API key is used to generate spelling suggestions for locations not
 * not found during a Geocode operation. You may obtain a key here: http://www.google.com/apis/
 */
define('GSEARCH_API_KEY', '');

define('MAX_YEAR', 2007);
define('APP_NAME', 'Firefox Party');
define('APP_EMAIL', '');

/* The Flickr API is used to show photos of each party on the individual party 
 * pages and home page. See http://flickr.com/services/api/keys/ to obtain a key
 */
define('FLICKR_API_KEY', '');
/* The tag prefix is used to limit the results returned to a specific party.
 * e.g. any photo tagged with FirefoxParty11 will be shown on party 11's page.
 * Photos tagged with only the prefix are shown on the front page (so choose wisely! ;) ).
 */
define('FLICKR_TAG_PREFIX', '');
?>