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
require 'lloydcgi.pl';
require 'cvsmenu.pl';

$|=1;

$CVS_ROOT = $form{"cvsroot"};

# get dir, remove leading and trailing slashes

$dir = $form{"dir"};
$dir =~ s/^\/([^:]*)/$1/;
$dir =~ s/([^:]*)\/$/$1/;

$rev = $form{"rev"};

print "Content-type: text/html

<HTML>";


&setup_script;
print $script_str;


if( $CVS_ROOT eq ""  ){
    $CVS_ROOT = pickDefaultRepository();
}

validateRepository($CVS_ROOT);

if( $rev ne "" ){
    $s = "for branch <i>$rev</i>";
}

print "
<head><title>Repository Directory $CVS_ROOT/$dir $s</title></head>";

CheckHidden("$CVS_ROOT/$dir");

$output = "<DIV ALIGN=LEFT>";

($dir_head, $dir_tail) = $dir =~ m@(.*/)?(.+)@;
foreach $path (split('/',$dir_head)) {
    $link_path .= $path;
    $output .= "<A HREF='rview.cgi?dir=$link_path";
    $output .= "&cvsroot=$form{'cvsroot'}" if defined $form{'cvsroot'};
    $output .= "&rev=$rev" unless $rev eq '';
    $output .= "'>$path</A>/ ";
    $link_path .= '/';
}
chop ($output);
$output .= " $s";
$output .= "</DIV>";

EmitHtmlHeader("Repository Directory", $output);

cvsmenu("align=right width=20%");

chdir "$CVS_ROOT/$dir";

print "
<TABLE CELLPADDING=0 CELLSPACING=0>
<FORM action=rview.cgi method=get><TR><TD>
Goto Directory:
</TD><TD><INPUT name=dir value='$dir' size=30>
<INPUT name=rev value='$rev' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='chdir'>
</TD></TR></FORM>
<FORM action=rview.cgi method=get><TR><TD>
Branch:
</TD><TD><INPUT name=rev value='$rev' size=30>
<INPUT name=dir value='$dir' type=hidden>
<INPUT name=cvsroot value='$CVS_ROOT' type=hidden>
<INPUT type=submit value='Set Branch'>
</TR>
</TABLE>

";

@dirs = ();

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
        $form{"dir"} = ($dir ne "" ? "$dir/$i" : $i);
        $anchor = &make_cgi_args;
        print "<dt><a href=rview.cgi${anchor}>$i</a>\n";
        if( $j % $split == 0 ){
            print "\n<td>\n";
        }
        $j++;
    }
    $form{"dir"} = $dir;
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
    l.top = d.target.y - 6;
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

    l.top = d.target.y - 6;
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

