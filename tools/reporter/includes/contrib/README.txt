Contributed Libraries should go here

Use the below listed "reference point's" to ensure that they are in the proper location.

===
Smarty
http://smarty.php.net

Reference point (check if this structure is valid):
[base_path]/includes/contrib/smarty/libs/Smarty.class

===
ADOdb
http://adodb.sourceforge.net/

Reference point (check if this structure is valid):
[base_path]/includes/contrib/adodb/adodb.inc

===
NuSOAP
http://sourceforge.net/projects/nusoap/

Scroll down to about line 130 in nusoap.php and comment out the following line:
    //var $soap_defencoding = 'ISO-8859-1';
and uncomment the following line:
    var $soap_defencoding = 'UTF-8';


Reference point (check if this structure is valid):
[base_path]/includes/contrib/nusoap/lib/nusoap.php

