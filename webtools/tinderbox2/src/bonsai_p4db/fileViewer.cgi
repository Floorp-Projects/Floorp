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
#  P4 file  viewer
#  View a file
#
#################################################################

use viewConfig ;
use colorView ;

# A hash containing file extensions that can be viewed with special viewers
# Data is:
#             <extension> => <semicolon separated list of:
#                             url      - Url to be used
#                             typecode - Will be sent as parameter TYPE to url
#                             text     - A text for the href to url>
# Other than the TYPE parameter mentioned above the file name (depot format) will
# be sent as FILE parameter and file revision as REV parameter.
#

# Get file spec argument

my $file = P4CGI::cgi()->param("FSPC") ;
$file = &P4CGI::extract_printable_chars($file);
&P4CGI::bail("No file specified") unless defined $file ;
&P4CGI::bail("Invalid file spec.") if ($file =~ /[<>"&:;'`]/);

my $ext = $file ;
$ext =~ s/^.*\.// ;

my $revision = P4CGI::cgi()->param("REV") ;
$revision = &P4CGI::extract_digits($revision);
#    &P4CGI::bail("No revision specified") unless defined $revision ;
$revision = "#$revision" if defined $revision ;
$revision = "" unless defined $revision ;
&P4CGI::bail("Invalid revision.") unless ($revision =~ /^#?\d*$/);

my $force = P4CGI::cgi()->param("FORCE") ;
$force = "Yes" if defined $force;


				# find out if p4br.perl is available, if true set smart
local *P4 ;
my $smart;
my ( $name, $rev, $type ) ;
if(-x "p4pr.perl") {
    open(P4,"./p4pr.perl \"$file$revision\" |") or 
	&P4CGI::bail("Can't start p4pr!!!!. too bad!") ;
    # Get header line
    #  line   author/branch change  rev //main/jam/Jamfile#39 - edit change 1749 (text)
    $_ = <P4>;	
    if(defined $_ and (/^\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)\#(\d+) - \S+ \S+ \S+ \((.+)\)/)) {
	( $name, $rev, $type ) = ($1,$2,$3) ;
	$_ = <P4>;
	$smart="Yes";
    }
    else {
	close P4 ;
    }
}
if(!defined $smart) {
    &P4CGI::p4call( *P4, "print \"$file$revision\"" );
    $smart="No, stupid." ;
    # Get header line
    # //main/jam/Jamfile#39 - edit change 1749 (text)
    $_ = <P4>;
    if(defined $_ and (/^(.+)\#(\d+) - \S+ \S+ \S+ \((\w+)\)/)) {	
	( $name, $rev, $type ) = ($1,$2,$3) ;
    } ;
}

my $legend = "" ;
if($smart eq "Yes") {
    $legend = 
	&P4CGI::ul_list("<b>Change number</b> -- see the change description",
			"<b>Revision number</b> -- see diff at selected revision") ;
}
$ext = uc($ext) ;

if(exists $viewConfig::ExtensionToType{$ext}) {
    my $type = $viewConfig::ExtensionToType{$ext} ;
    my ($url,$desc,$content,$about) = @{$viewConfig::TypeData{$type}} ;
    $legend .= &P4CGI::ahref(-url => $url,
			     "TYPE=$type",
			     "FSPC=$file",
			     "REV=$rev",
			     "View $desc") ;
    $legend .= "&nbsp;&nbsp;&nbsp;<small><i>$about</i></small>" if defined $about ;
    $legend .= "<br>";
} ;
$legend .= &P4CGI::ahref(-url => "fileDownLoad.cgi",
			 "FSPC=$file",
			 "REV=$rev",
			 "Download file") ;


my @back = &P4CGI::back_buttons($file);
print 
    "",
    &P4CGI::start_page("File<br>@back<code>\#$rev</code>",$legend) ;

if(!defined $force and ($type =~ /.*binary/))
{
    print
	"<h2>Type is $type.</h2>\n",
	&P4CGI::ahref(-url => &P4CGI::cgi()->url,
		      "FSPC=$file",
		      "REV=$rev",
		      "FORCE=Y",
		      "View anyway!") ;

}
else
{
    print "Type: $type<br>\n<pre>\n";

    my @prompts  ;
    my @filetext ;

    if(!defined $force and $smart eq "Yes"){
	my ($lineno,$authorBranch,$change,$rev,$line) ;
	print "<small>Line<tt>   Author  </tt>Ch.  Rev</small>\n";
	my $oldch=-1;
	while( <P4> ) {
	    ($lineno,$authorBranch,$change,$rev,$line) =
		m/^\s+(\d+)\s+(\S+)\s+(\d+)\s+(\d+) (.*)$/ ;		
	    my $linenos = sprintf("<A Name=\"L%d\"></A><tt>%3d:</tt>",$lineno,$lineno) ;	   
	    my($chstr,$revstr,$authorstr)=("     ","   ", "        ");
	    if($oldch != $change){
		$chstr=
		    substr("     ",0,5-length("$change")) . 
			&P4CGI::ahref("-url","changeView.cgi",
				      "CH=$change",
				      "$change") ;
		$revstr = 
		    substr("     ",0,3-length("$rev")) .
			&P4CGI::ahref("-url","fileDiffView.cgi",
				      "FSPC=$name",
				      "REV=$rev","ACT=edit",
				      "$rev");
		$authorstr = 
		    substr("        ",0,8-length("$authorBranch")).
			&P4CGI::ahref("-url","changeList.cgi",
				      "FSPC=$name",
				      "USERS=$authorBranch",
				      "$authorBranch");
			   	    }		    
	    $oldch= $change ;
	    if(($lineno % 5) != 0) {
		while($linenos =~ s/>( *)\d/>$1 /) {} ;
		$linenos =~ s/:<\/tt>/ <\/tt>/ ;
            }
	    $authorBranch =  $change =~ m/\d/ ;
	    push @filetext,&P4CGI::fixSpecChar($line) ;
	    push @prompts,"<small>$linenos $authorstr $chstr$revstr </small><font color=red>|</font>" ;
	}	    
    }
    else {
	while( <P4> ) {
	    chomp ;
	    push @filetext,&P4CGI::fixSpecChar($_) ;
	    push @prompts,"" ;
	}	    
    }
    my $FILE = join("\n",@filetext) ;
    if(&P4CGI::VIEW_WITH_COLORS()) {
	&colorView::color($file,\$FILE) ;
	@filetext = split("\n",$FILE) unless $@ ;
    } ;
	
    while(@filetext) {
	print (shift @prompts,shift @filetext,"\n") ;
    }
    print "</pre>\n";
}
close P4;

print
    "",    
    &P4CGI::end_page() ;

#
# That's all folks
#
