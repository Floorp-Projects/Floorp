#!/usr/bin/perl -w
# -*- perl -*-
use P4CGI ;
use strict ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  P4 "smart" HTML file viewer (maybe too smart....)
#  View a HTML file
#
#################################################################
use viewConfig ;

# Get type arg
my $type = P4CGI::cgi()->param("TYPE") ;
&P4CGI::bail("No file type specified") unless defined $type ;
&P4CGI::bail("Invalid file type.") if ($type =~ /[<>"&:;'`]/);

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;


if($type ne "ABOUT") {
    my $content = "text/html"  ;
    
    # Get file spec argument
    my $file = P4CGI::cgi()->param("FSPC") ;
    &P4CGI::bail("No file specified") unless defined $file ;
    &P4CGI::bail("Invalid file spec.") if ($file =~ /[<>"&:;'`]/);

    my $revision = P4CGI::cgi()->param("REV") ;
    $revision = "#$revision" if defined $revision ;
    $revision = "" unless defined $revision ;
    &P4CGI::bail("Invalid file spec.") unless ($revision =~ /^#?\d*$/);

    my $filename=$file ;
    $filename =~ s/^.*\///;
    
    print
	"Content-Type: $content\n",
	"Content-Disposition: filename=$filename\n",
	"\n" ;
    
    my $fileText ;
    &P4CGI::p4call(\$fileText, "print -q \"$file$revision\"" );
    my @file = split(/(<[^>]+>)/,$fileText) ;
    my $l  ;
    foreach $l (@file) {
	if($l =~ /^<(\w+)/) {
	    my $fld=uc($1) ;
	    my $prompt ;
	    $prompt = "href" if $fld eq "A" ;
	    $prompt = "src"  if $fld eq "FRAME" ;
	    $prompt = "src"  if $fld eq "IMG" ;
	    $prompt = "background" if $fld eq "BODY" ;
	    unless(defined $prompt) {
		print $l ;
		next ;
	    }
	    if ($l =~ /^(<.*$prompt=\")([^\"]+)(\".*)/i ) { #"
		my ($s,$url,$e) = ($1,$2,$3) ;
		if ($url =~ m|^\w+://| or 
		    $url =~ m|^/| or
		    $url =~ m|:\d+$|) {
		    print $l ; 
		    next ;
		} ;
		my $ext = "" ;
		my $anchor = "" ;
		if($url =~ /(.*)\#(.*)/) {
		    $url = $1 ;
		    $anchor = "#$2" ;
		}
		if($url =~ /.*\.(\w+)$/) {
		    $ext = uc($1) ;
		}
		my $dir = $file ;
		$dir =~ s|[^/]*$|| ;
		my $lnfile = &P4CGI::fixspaces("$dir$url") ;
		
		my @log ;
		&P4CGI::p4call(\@log, "filelog \"$dir$url\" $err2null" );
		if(@log == 0 or $log[1] =~ /delete on/) {
		    print $l ; 
		    next ;
		}
                
		if(exists $viewConfig::ExtensionToType{$ext}) {
		    my $type = $viewConfig::ExtensionToType{$ext} ;
		    my ($nurl,$desc,$content,$about) =
			@{$viewConfig::TypeData{$type}} ;
		    $url = "$nurl?FSPC=$lnfile&TYPE=$type$anchor" ;
		}
		else {
		    $url = "fileViewer.cgi" . "?FSPC=$lnfile" ;
		} 
		print "$s$url$e" ;
		next ;
	    }
	}
	print "$l" ;
    } ;    
} 
else {
    while(<DATA>) {
	print ;
    } ;
}

#
# That's all folks
#
__END__
Content-Type: text/html

<HTML><HEAD><TITLE>P4DB: About HTML Viewer</TITLE>
</HEAD>
<BODY BGCOLOR="#e0f0f0" VLINK="#663366" TEXT="#000000" LINK="#000099" ALINK="#993399">
<table bgcolor="#FFFFFF" border=0 cellspacing=8>
<tr>
<th>Note about the HTML viewer</th>
</tr>
<tr>
<td>
The "smart" HTML viewer translate relative links to links to 
files in the depot.
</td>
</tr>
</table>
</body>
</html>
