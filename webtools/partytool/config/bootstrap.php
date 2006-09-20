<?php
/* REQUIRED - APP_NAME is used on all <title>s and mail names/subjects. APP_BASE
 * should be a FQDN with protocol minus the trailing slash e.g. http://example.tld/party 
 */
define('APP_NAME', '');
define('APP_EMAIL', '');
define('APP_BASE', '');

/* You should specify a Google Map API key here. Without it, all mapping features
 * will be disabled. To obtain a key, visit http://www.google.com/apis/maps/
 */ 
define('GMAP_API_KEY', '');

/* The search API key is used to generate spelling suggestions for locations not
 * not found during a Geocode operation. You may obtain a key here: http://www.google.com/apis/
 */
define('GSEARCH_API_KEY', '');

/* The maximum year shown for party registrations */
define('MAX_YEAR', 2007);

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