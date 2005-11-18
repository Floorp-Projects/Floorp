#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


#
# Query the CVS database.
#

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::Setup_String;
    $zz = $::script_type;
}

require 'CGI.pl';

$|=1;

my $CVS_ROOT = $::FORM{"cvsroot"};
$CVS_ROOT = pickDefaultRepository() unless $CVS_ROOT;
&validateRepository($CVS_ROOT);

&LoadTreeConfig();
my $intreeid = &SanitizeModule($::FORM{'treeid'});
my $inmod = &SanitizeModule($::FORM{'module'});
  
if ($intreeid && exists($::TreeInfo{$intreeid}{'repository'}) &&
    !exists($::TreeInfo{$intreeid}{'nobonsai'})) {
    $::TreeID = $intreeid;
} elsif ($inmod && exists($::TreeInfo{$inmod}{'repository'}) &&
         !exists($::TreeInfo{$inmod}{'nobonsai'})) {
    $::TreeID = $inmod;
} else {
    $::TreeID = 'default';
}


# get dir, remove leading and trailing slashes

my $dir = $::FORM{"dir"} || "";
$dir =~ s/^[\/]+([^:]*)/$1/;
$dir =~ s/([^:]*)[\/]+$/$1/;
my $path = "$CVS_ROOT/$dir";
$path = &ChrootFilename($CVS_ROOT, $path);
die "Invalid directory: " . &shell_escape($dir) . ".\n" if (! -d $path);

my $rev = &SanitizeRevision($::FORM{"rev"});

print "Content-type: text/html\n\n";


my $registryurl = Param('registryurl');
$registryurl =~ s@/$@@;

my $script_str;

&setup_script;
$::Setup_String = $script_str;

my $s = "";

if ($rev) {
    $s = "for branch <i>$rev</i>";
}

my $revstr = '';
$revstr = "&rev=$rev" unless $rev eq '';
my $rootstr = '';
$rootstr .= "&cvsroot=$CVS_ROOT";
$rootstr .= "&module=$::TreeID";
my $module = $::TreeInfo{$::TreeID}{'module'};

my $toplevel = Param('toplevel');

&PutsHeader("Repository Directory $toplevel/" . &html_quote($dir) . " $s", "");

my $output = "<DIV ALIGN=LEFT>";
$output .= "<A HREF='toplevel.cgi" . BatchIdPart('?') . "'>$toplevel</a>/ ";

my ($dir_head, $dir_tail) = $dir =~ m@(.*/)?(.+)@;
$dir_head = "" unless defined $dir_head;
$dir_tail = "" unless defined $dir_tail;
my $link_path = "";
foreach my $path (split('/',$dir_head)) {
    $link_path .= $path;
    $output .= "<A HREF='rview.cgi?dir=" . &url_quote($link_path) .
        "$rootstr$revstr'>$path</A>/ ";
    $link_path .= '/';
}
chop ($output);
$output .= " " . &html_quote($dir_tail) . "/ $s ";
$output .= "</DIV>";

print "$output\n";
print '<table width="100%"><tr><td width="70%" VALIGN=TOP><HR>';

my $other_dir;

($other_dir = $dir) =~ s!^$module/?!!;
my $other_dir_used = 1;

LoadDirList();
if (-d "$CVS_ROOT/$dir") {
     chdir "$CVS_ROOT/$dir";
     $other_dir_used = 0;
} elsif (-d "$CVS_ROOT/$other_dir") {
     chdir "$CVS_ROOT/$other_dir";
} else {
     chdir "$CVS_ROOT";
}

print "
<TABLE CELLPADDING=0 CELLSPACING=5>
<TR>
<TD>Goto Directory:</TD>
<TD>
<FORM action=rview.cgi method=get>
<INPUT name=dir value='$dir' size=30>
<INPUT name=rev value='$rev' type=hidden>
<INPUT name=module value='$::TreeID' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='chdir'>
</FORM>
</TD>
</TR>
<TR>
<TD>Branch:</TD>
<TD>
<FORM action=rview.cgi method=get>
<INPUT name=rev value='$rev' size=30>
<INPUT name=dir value='$dir' type=hidden>
<INPUT name=module value='$::TreeID' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='Set Branch'>
</FORM>
</TD>
</TR>
</TABLE>

";

my @dirs = ();


DIR:
while( <.* *> ){
    if( -d $_ ){
        next if $_ =~ /^\.{1,2}$/;  
        push @dirs, $_;
    }
}



my $j;
my $split;

if( @dirs != 0 ){
    $j = 1;
    $split = int(@dirs/4)+1;
    print "<P><FONT SIZE=+1><B>Directories:</B></FONT>\n";
    print "<table>\n<TR VALIGN=TOP>\n<td>\n";

    for my $i (@dirs){
        my $ldir = ($dir ne "" ? "$dir/$i" : $i);
        my $anchor = "?dir=$ldir$rootstr";
        print "<dt><a href=rview.cgi${anchor}>$i</a>\n";
        if( $j % $split == 0 ){
            print "\n<td>\n";
        }
        $j++;
    }
    print "\n</tr>\n</table>\n";
}

print "<P><FONT SIZE=+1><B>Files:</B></FONT>\n";
print "<table>\n<TR VALIGN=TOP>\n<td>\n";
my @files = <.*,v *,v>;
$j = 1;
$split = int(@files/4)+1;

for $_ (@files){
    $_ =~ s/\,v//;
    print qq{<dt><a href="$registryurl/file.cgi?cvsroot=$CVS_ROOT&file=$_&dir=$dir$revstr"}
          . " onclick=\"return js_file_menu('$dir','$_','$rev','$CVS_ROOT',event)\">";
    print "$_</a>\n";
    if( $j % $split == 0 ){
        print "</td>\n<td>\n";
    }
    $j++;
}
print "</td>\n</tr></table>\n</td>\n<td>\n";
cvsmenu("");
print "</td>\n</tr></table>\n";

PutsTrailer();


sub setup_script {

$script_str = qq%
<script $::script_type><!--

var event = new Object;

function js_who_menu(n,extra,d) {
    if( parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="$registryurl/who.cgi?email="+n+extra;

    if(d.target.y > window.innerHeight + window.pageYOffset - l.clip.height) { 
         l.top = (window.innerHeight + window.pageYOffset - l.clip.height);
    } else {
         l.top = d.target.y - 6;
    }

    l.left = d.target.x - 6;

    if( l.left + l.clipWidth > window.width ){
        l.left = window.width - l.clipWidth;
    }
    l.visibility="show";
    return false;
}

function js_file_menu(dir,file,rev,root,d) {
    if( parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="$registryurl/file.cgi?file="+file+"&dir="+dir+"&rev="+rev+"&cvsroot="+root+"&linked_text="+d.target.text;
    
    if(d.target.y > window.innerHeight + window.pageYOffset - l.clip.height) { 
         l.top = (window.innerHeight + window.pageYOffset - l.clip.height);
    } else {
         l.top = d.target.y - 6;
    }

    l.left = d.target.x - 6;

    if( l.left + l.clipWidth > window.width ){
        l.left = window.width - l.clipWidth;
    }

    l.visibility="show";
    return false;
}


//--></script>

<layer name="popup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

%;

}

