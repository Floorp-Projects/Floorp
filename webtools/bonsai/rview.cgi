#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.


#
# Query the CVS database.
#
require 'CGI.pl';

$|=1;

$CVS_ROOT = $::FORM{"cvsroot"};
$CVS_ROOT = pickDefaultRepository() unless $CVS_ROOT;

LoadTreeConfig();
$::TreeID = $::FORM{'module'} 
     if (!exists($::FORM{'treeid'}) &&
         exists($::FORM{'module'}) &&
         exists($::TreeInfo{$::FORM{'module'}}{'repository'}));
$::TreeID = 'default'
     if (!exists($::TreeInfo{$::TreeID}{'repository'}) ||
         exists($::TreeInfo{$::TreeID}{'nobonsai'}));


# get dir, remove leading and trailing slashes

$dir = $::FORM{"dir"};
$dir =~ s/^\/([^:]*)/$1/;
$dir =~ s/([^:]*)\/$/$1/;

$rev = $::FORM{"rev"};

print "Content-type: text/html\n\n";


&setup_script;
$Setup_String = $script_str;


if( $CVS_ROOT eq ""  ){
    $CVS_ROOT = pickDefaultRepository();
}

validateRepository($CVS_ROOT);

if( $rev ne "" ){
    $s = "for branch <i>$rev</i>";
}

CheckHidden("$CVS_ROOT/$dir");

$revstr = '';
$revstr = "&rev=$rev" unless $rev eq '';
$rootstr = '';
$rootstr .= "&cvsroot=$::FORM{'cvsroot'}" if defined $::FORM{'cvsroot'};
$rootstr .= "&module=$::TreeID";
$module = $::TreeInfo{$::TreeID}{'module'};

$toplevel = Param('toplevel');

$output = "<DIV ALIGN=LEFT>";
$output .= "<A HREF='toplevel.cgi" . BatchIdPart('?') . "'>$toplevel</a>/ ";

$dir = $module unless ($dir);

($dir_head, $dir_tail) = $dir =~ m@(.*/)?(.+)@;
foreach $path (split('/',$dir_head)) {
    $link_path .= $path;
    $output .= "<A HREF='rview.cgi?dir=$link_path$rootstr$revstr'>$path</A>/ ";
    $link_path .= '/';
}
chop ($output);
$output .= " $s";
$output .= "</DIV>";

PutsHeader("Repository Directory $toplevel/$dir $s", $output);

cvsmenu("align=right width=30%");

($other_dir = $dir) =~ s!^$module/?!!;
$other_dir_used = 1;

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
<TABLE CELLPADDING=0 CELLSPACING=0>
<FORM action=rview.cgi method=get><TR><TD>
Goto Directory:
</TD><TD><INPUT name=dir value='$dir' size=30>
<INPUT name=rev value='$rev' type=hidden>
<INPUT name=module value='$::TreeID' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='chdir'>
</TD></TR></FORM>
<FORM action=rview.cgi method=get><TR><TD>
Branch:
</TD><TD><INPUT name=rev value='$rev' size=30>
<INPUT name=dir value='$dir' type=hidden>
<INPUT name=module value='$::TreeID' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='Set Branch'>
</TR></FORM>
</TABLE>

";

@dirs = ();


DIR:
while( <*> ){
    if( -d $_ ){
        push @dirs, $_;
    }
}



if( @dirs != 0 ){
    $j = 1;
    $split = int(@dirs/4)+1;
    print "<P><FONT SIZE=+1><B>Directories:</B></FONT><table><TR VALIGN=TOP><td>";


    for $i (@dirs){
        $::FORM{"dir"} = ($dir ne "" ? "$dir/$i" : $i);
        $anchor = &make_cgi_args;
        print "<dt><a href=rview.cgi${anchor}>$i</a>\n";
        if( $j % $split == 0 ){
            print "\n<td>\n";
        }
        $j++;
    }
    $::FORM{"dir"} = $dir;
    print "\n</tr></table>\n";
}


print "<P><FONT SIZE=+1><B>Files:</B></FONT>";
print "<table><TR VALIGN=TOP><td>";
@files = <*,v>;
$j = 1;
$split = int(@files/4)+1;

for $_ (@files){
    $_ =~ s/\,v//;
    print "<a href=../registry/file.cgi?cvsroot=$CVS_ROOT&file=$_&dir=$dir"
          . " onclick=\"return js_file_menu('$dir','$_','$rev','$CVS_ROOT',event)\">\n";
    print "<dt>$_</a>\n";
    if( $j % $split == 0 ){
        print "\n<td>\n";
    }
    $j++;
}
print "\n</tr></table>\n";

PutsTrailer();


sub setup_script {

$script_str =<<'ENDJS';
<script>

var event = new Object;

function js_who_menu(n,extra,d) {
    if( parseInt(navigator.appVersion) < 4 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="../registry/who.cgi?email="+n+extra;

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
    if( parseInt(navigator.appVersion) < 4 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="../registry/file.cgi?file="+file+"&dir="+dir+"&rev="+rev+"&cvsroot="+root+"&linked_text="+d.target.text;
    
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


</script>

<layer name="popup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

ENDJS

}

