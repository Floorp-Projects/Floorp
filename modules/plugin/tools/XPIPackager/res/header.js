/** 

Doron Rosenberg -- doronAtNoSpamAllowedHerenetscape.com
Arun K. Ranganathan -- arunerAtNoSpamAllowedHerenetscape.com
Petter Ericson -- petterAtNoSpamAllowedHerecycore.com
Anthony Davis -- anthonydAtNoSpamAllowedHerenetscape.com

This is an install.js file that does the following --

1. Installs to the current browser that is invoking the installation
2. Additionally installs to a secondary location on the Windows desktop,
   in this case C:\WINNT\System32\MyPlugin\
3. Writes registry keys exposing the above secondary install location for
   other browsers to find.  The keys are written according to the specification:
   http://mozilla.org/projects/plugins/first-install-problem.html and follows the 
   PLID specification: http://mozilla.org/projects/plugins/plugin-identifier.html
 
**/

// Define some global variables


