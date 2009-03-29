Last updated September 2008

This SDK is based on the API developed originally for Netscape browsers
starting with Netscape 2.x. It is intended to help in creating plugins
that will work with any modern NPAPI-compliant web browsers.

===============================================================

Samples

The "samples" directory contains NPAPI plugin samples. Within the "samples"
directory the common folder contains stub implementations of the NPAPI methods.
There is no need to modify files in this folder, just include them into your
project if you wish. Each sample plugin contains a readme file describing it.

===============================================================

Plugin developers might find it useful for debugging purposes to turn
off the exception catching mechanism currently implemented in Mozilla 
on Windows. To do this add the following line into your prefs.js file:
user_pref("plugin.dont_try_safe_calls", true);
