#!/usr/bin/perl -w
# -*- perl -*-
package viewConfig ;
use strict ;
#
#
#################################################################
# Configuration file for special file viewer.
#################################################################
#
#
my %ExtensionToType ;

my %TypeData ;

BEGIN() 
{

    # Keys: file extension (in uppercase)
    # value: A type code (must be unique)
    %viewConfig::ExtensionToType = 
	(
	 HTML =>"HTML",
	 HTM  =>"HTML",
	 GIF  =>"GIF",
	 JPEG =>"JPEG",
	 JPG  =>"JPEG", 
	 DOC  =>"WINWORD",
	 DOT  =>"WINWORDT",
	 PPT  =>"PWRPT", 
	 RTF  =>"RTF", 
	 PDF  =>"PDF", 
	 ) ;

				# Special "about" url for html viewer
    my $htmlAbout = &P4CGI::ahref(-url => "htmlFileView.cgi" ,
				  "TYPE=ABOUT",
				  "About the HTML viewer") ;
    # Keys: Type code
    # value: A referece to an array: 
    #        [url for viewer,description of content, content type, optinal about]
    %viewConfig::TypeData = 
	(
	 HTML     =>["htmlFileView.cgi","html file using \"smart\" viewer and a browser","text/html",
		     $htmlAbout],
	 GIF      =>["specialFileView.cgi"  ,"gif image using browser","image/gif"],
	 JPEG     =>["specialFileView.cgi"  ,"jpeg image using browser","image/jpeg"],
	 WINWORD  =>["specialFileView.cgi"  ,"MS-Word document","application/msword"],
	 WINWORDT =>["specialFileView.cgi"  ,"MS-Word template","application/msword"],
	 PWRTP    =>["specialFileView.cgi"  ,"MS-PowerPoint document","application/ppt"],
	 RTF      =>["specialFileView.cgi"  ,"RTF document","application/rtf"],
	 PDF      =>["specialFileView.cgi"  ,"PDF document","application/pdf"]
	 ) ;
} ;
1;






