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

use diagnostics;
use strict;

require 'CGI.pl';

my $file= $::FORM{'file'};
my $mark= $::FORM{'mark'};
my $ln = ($mark > 10 ? $mark-10 : 1 );
my $rev = sanitize_revision($::FORM{'rev'});
my $debug = $::FORM{'debug'};

print "Content-Type: text/html\n\n";

my $CVS_ROOT = $::FORM{'root'};
if( !defined($CVS_ROOT) || $CVS_ROOT eq '' ){ 
    $CVS_ROOT = pickDefaultRepository();
}
validateRepository($CVS_ROOT);

my $CVS_REPOS_SUFIX = $CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;
    
ConnectToDatabase();

my $f = SqlQuote($file);
my $qstring = "select distinct dirs.dir from checkins,dirs,files,repositories where dirs.id=dirid and files.id=fileid and repositories.id=repositoryid and repositories.repository='$CVS_ROOT' and files.file=$f order by dirs.dir";

if ($debug) {
    print "<pre wrap>$qstring</pre>\n";
}

my (@row, $d, @fl, $s);

SendSQL($qstring);
while(@row = FetchSQLData()){
    $d = $row[0];
    push @fl, "$d/$file";
}

if( @fl == 0 ){
    print "<h3>No files matched this file name.  It may have been added recently.</h3>";
}
elsif( @fl == 1 ){
    $s = $fl[0];
    print "<head>
    <meta http-equiv=Refresh
      content=\"0; URL=cvsblame.cgi?file=$s&rev=$rev&root=$CVS_ROOT&mark=$mark#$ln\">
    </head>
    ";    
}
else {
    print "<h3>Pick the file that best matches the one you are looking for:</h3>\n";
    for $s (@fl) {
        print "<dt><a href=cvsblame.cgi?file=$s&rev=$rev&mark=$mark#$ln>$s</a>";
    }
}
