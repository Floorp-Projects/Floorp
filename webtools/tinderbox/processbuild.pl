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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

require 'globals.pl';
require 'timelocal.pl';

umask 0;

#$logfile = '';

%MAIL_HEADER = ();
$DONE = 0;
$building = 0;
$endsection = 0;

open( LOG, "<$ARGV[0]") || die "cant open $!";
&parse_mail_header;
while ($DONE == 0) {
    %tbx = ();
    &get_variables;

    # run thru if EOF and we haven't hit our section end marker
 
    if ( !$DONE || !$endsection) {
        &check_required_vars;
        $tree = $tbx{'tree'} if (!defined($tree));
        $logfile = "$builddate.$$.gz" if (!defined($logfile));
	$building++ if ($tbx{'status'} =~ m/building/);
        &lock;
        &write_build_data;
	&build_html_from_log;
        &unlock;
    }
}
close(LOG);

&compress_log_file;
&unlink_log_file;

$err = system("./buildwho.pl $tree");

#
# This routine will scan through log looking for 'tinderbox:' variables
#
sub  get_variables{

    #while( ($k,$v) = each( %MAIL_HEADER ) ){
    #    print "$k='$v'\n";
    #}

    &parse_log_variables;

    #while( ($k,$v) = each( %tbx ) ){
    #    print "$k='$v'\n";
    #}
}


sub parse_log_variables {
    my ($line, $stop);
    $stop = 0;
    while($stop == 0){
        $line = <LOG>;
        $DONE++, return if !defined($line);
        chomp($line);
        if( $line =~ /^tinderbox\:/ ){
            if( $line =~ /^tinderbox\:[ \t]*([^:]*)\:[ \t]*([^\n]*)/ ){
                $tbx{$1} = $2;
            } elsif ( $line =~ /^tinderbox: END/ ) {
                $stop++, $endsection++;
            }
        }
    }
}

sub parse_mail_header {
    my $line;
    my $name = '';
    while($line = <LOG> ){
        chomp($line);

        if( $line eq '' ){
            return;
        }

        if( $line =~ /([^ :]*)\:[ \t]+([^\n]*)/ ){
            $name = $1;
            $name =~ tr/A-Z/a-z/;
            $MAIL_HEADER{$name} = $2;
            #print "$name $2\n";
        }
        elsif( $name ne '' ){
            $MAIL_HEADER{$name} .= $2;
        }
    }

}

sub check_required_vars {
    $err_string = '';
    if( $tbx{'tree'} eq ''){
        $err_string .= "Variable 'tinderbox:tree' not set.\n";
    }
    elsif( ! -r $tbx{'tree'} ){
        $err_string .= "Variable 'tinderbox:tree' not set to a valid tree.\n";
    }
    elsif(($MAIL_HEADER{'to'} =~ /external/i ||
           $MAIL_HEADER{'cc'} =~ /external/i) &&
          $tbx{'tree'} !~ /external/i) {
        $err_string .= "Data from an external source didn't specify an 'external' tree.";
    }
    if( $tbx{'build'} eq ''){
        $err_string .= "Variable 'tinderbox:build' not set.\n";
    }
    if( $tbx{'errorparser'} eq ''){
        $err_string .= "Variable 'tinderbox:errorparser' not set.\n";
    }

    #
    # Grab the date in the form of mm/dd/yy hh:mm:ss
    #
    # Or a GMT unix date
    #
    if( $tbx{'builddate'} eq ''){
        $err_string .= "Variable 'tinderbox:builddate' not set.\n";
    }
    else {
        if( $tbx{'builddate'} =~ 
            /([0-9]*)\/([0-9]*)\/([0-9]*)[ \t]*([0-9]*)\:([0-9]*)\:([0-9]*)/ ){
            
            $builddate = timelocal($6,$5,$4,$2,$1-1,$3);
                        
        }
        elsif( $tbx{'builddate'} > 7000000 ){
            $builddate = $tbx{'builddate'};
        }
        else {
            $err_string .= "Variable 'tinderbox:builddate' not of the form MM/DD/YY HH:MM:SS or unix date\n";
        }

    }

    #
    # Build Status
    #
    if( $tbx{'status'} eq ''){
        $err_string .= "Variable 'tinderbox:status' not set.\n";
    }
    elsif( ! $tbx{'status'} =~ /success|busted|building|testfailed/ ){
        $err_string .= "Variable 'tinderbox:status' must be 'success', 'busted', 'testfailed', or 'building'\n";
    }

    #
    # Report errors
    #
    if( $err_string ne ''  ){
        die $err_string;
    }
}

sub write_build_data {
    $t = time;
    open( BUILDDATA, ">>$tbx{'tree'}/build.dat" )|| die "can't open $! for writing";
    print BUILDDATA "$t|$builddate|$tbx{'build'}|$tbx{'errorparser'}|$tbx{'status'}|$logfile|$tbx{binaryname}\n";
    close( BUILDDATA );
}

sub compress_log_file {
    local( $done, $line);

    return if ( $building );

    open( LOG2, "<$ARGV[0]") || die "cant open $!";

    #
    # Skip past the the RFC822.HEADER
    #
    $done = 0;
    while( !$done && ($line = <LOG2>) ){
        chomp($line);
        $done = ($line eq '');
    }

    open( ZIPLOG, "| $gzip -c > ${tree}/$logfile" ) || die "can't open $! for writing";
    $inBinary = 0;
    $hasBinary = ($tbx{'binaryname'} ne '');
    while( $line = <LOG2> ){
        if( !$inBinary ){
            print ZIPLOG $line;
            if( $hasBinary ){
                $inBinary = ($line =~ /^begin [0-7][0-7][0-7] /);
            }
        }
        else {
            if( $line =~ /^end\n/ ){
                $inBinary = 0;
            }
        }
    }
    close( ZIPLOG );
    close( LOG2 );

    #
    # If a uuencoded binary is part of the build, unpack it.
    #
    if( $hasBinary ){
        $bin_dir = "$tbx{'tree'}/bin/$builddate/$tbx{'build'}";
        $bin_dir =~ s/ //g;

        system("mkdir -m 0777 -p $bin_dir");

        # LTNOTE: I'm not sure this is cross platform.
        system("/tools/ns/bin/uudecode --output-file=$bin_dir/$tbx{binaryname} < $ARGV[0]");
    }
}
        

# Some tinderbox html pages can be build preprocessed and cached
sub build_html_from_log {
  if ($tbx{status} eq 'busted') {
    my $buildname = url_encode($tbx{build});
    system("QUERY_STRING='tree=$tbx{tree}&errorparser=$tbx{errorparser}&buildname=$buildname&buildtime=$builddate&logfile=$logfile&cache=1' showlog.cgi");
  }
}

sub unlink_log_file {
    unlink( $ARGV[0] );
}
