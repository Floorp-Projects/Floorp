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

use Socket;

require 'globals.pl';
require 'imagelog.pl';

# Port an old-style imagelog thing to a newstyle one

open( IMAGELOG, "<$data_dir/imagelog.txt" ) || die "can't open file";
open (OUT, ">$data_dir/newimagelog.txt") || die "can't open output file";
select(OUT); $| = 1; select(STDOUT);

while( <IMAGELOG> ){
    chop;
    ($url,$quote)  = split(/\`/);
    print "$url\n";
    $size = &URLsize($url);
    $width = "";
    $height = "";
    if ($size =~ /WIDTH=([0-9]*)/) {
        $width = $1;
    }
    if ($size =~ /HEIGHT=([0-9]*)/) {
        $height = $1;
    }
    if ($width eq "" || $height eq "") {
        print "Couldn't get image size; skipping.\n";
    } else {
        print OUT "$url`$width`$height`$quote\n";
    }
}






sub imgsize {
    local($file)= @_;

    #first try to open the file
    if( !open(STREAM, "<$file") ){
        print "Can't open IMG $file"; 
        $size="";
    } else {
        if ($file =~ /.jpg/i || $file =~ /.jpeg/i) {
            $size = &jpegsize(STREAM);
        } elsif($file =~ /.gif/i) {
            $size = &gifsize(STREAM);
        } elsif($file =~ /.xbm/i) {
            $size = &xbmsize(STREAM);
        } else {
            return "";
        }
        $_ = $size;
        if(  /\s*width\s*=\s*([0-9]*)\s*/i ){
            ($newwidth)= /\s*width\s*=\s*(\d*)\s*/i;
        }
        if(  /\s*height\s*=\s*([0-9]*)\s*/i ){
            ($newheight)=/\s*height\s*=\s*(\d*)\s*/i;
        }
        close(STREAM);
    }
    return $size;
}

###########################################################################
# Subroutine gets the size of the specified GIF
###########################################################################
sub gifsize {
    local($GIF) = @_;
    read($GIF, $type, 6); 
    if(!($type =~ /GIF8[7,9]a/) || 
       !(read($GIF, $s, 4) == 4) ){
        print "Invalid or Corrupted GIF"; 
        $size="";
    } else {
        ($a,$b,$c,$d)=unpack("C"x4,$s);
        $size=join ("", 'WIDTH=', $b<<8|$a, ' HEIGHT=', $d<<8|$c);
    }
    return $size;
}

sub xbmsize {
    local($XBM) = @_;
    local($input)="";

    $input .= <$XBM>;
    $input .= <$XBM>;
    $_ = $input;
    if( /#define\s*\S*\s*\d*\s*\n#define\s*\S*\s*\d*\s*\n/i ){
       ($a,$b)=/#define\s*\S*\s*(\d*)\s*\n#define\s*\S*\s*(\d*)\s*\n/i;
       $size=join ("", 'WIDTH=', $a, ' HEIGHT=', $b );
   } else {
       print "Hmmm... Doesn't look like an XBM file";
   }
    return $size;
}

# jpegsize : gets the width and height (in pixels) of a jpeg file
# Andrew Tong, werdna@ugcs.caltech.edu           February 14, 1995
# modified slightly by alex@ed.ac.uk
sub jpegsize {
    local($JPEG) = @_;
    local($done)=0;
    $size="";

    read($JPEG, $c1, 1); read($JPEG, $c2, 1);
    if( !((ord($c1) == 0xFF) && (ord($c2) == 0xD8))){
        printf "This is not a JPEG! (Codes %02X %02X)\n", ord($c1), ord($c2);
        $done=1;
    }
    while (ord($ch) != 0xDA && !$done) {
        # Find next marker (JPEG markers begin with 0xFF)
        # This can hang the program!!
        while (ord($ch) != 0xFF) {  read($JPEG, $ch, 1); }
        # JPEG markers can be padded with unlimited 0xFF's
        while (ord($ch) == 0xFF) { read($JPEG, $ch, 1); }
        # Now, $ch contains the value of the marker.
            $marker=ord($ch);

            if (($marker >= 0xC0) && ($marker <= 0xCF) &&
            ($marker != 0xC4) && ($marker != 0xCC)) {  # it's a SOFn marker
            read ($JPEG, $junk, 3); read($JPEG, $s, 4);
            ($a,$b,$c,$d)=unpack("C"x4,$s);
            $size=join("", 'HEIGHT=',$a<<8|$b,' WIDTH=',$c<<8|$d );
            $done=1;
        } else {
            # We **MUST** skip variables, since FF's within variable 
            # names are NOT valid JPEG markers
            read ($JPEG, $s, 2); 
            ($c1, $c2) = unpack("C"x2,$s); 
            $length = $c1<<8|$c2;
            if( ($length < 2) ){
                print "Erroneous JPEG marker length";
                $done=1;
            } else {
                read($JPEG, $junk, $length-2);
            }
        }
    }
    return $size;
}

###########################################################################
# Subroutine grabs a gif from another server and gets its size
###########################################################################


sub URLsize {
    my ($fullurl) = @_;
    my($dummy, $dummy, $serverstring, $url) = split(/\//, $fullurl, 4);
    my($them,$port) = split(/:/, $serverstring);
    my $port = 80 unless $port;
    $them = 'localhost' unless $them;
    my $size="";

    $_=$url;
    if( /gif/i || /jpeg/i || /jpg/i || /xbm/i ) {
        my ($remote, $iaddr, $paddr, $proto, $line);
        $remote = $them;
        if ($port =~ /\D/) { $port = getservbyname($port, 'tcp') }
        die "No port" unless $port;
        $iaddr   = inet_aton($remote)               || die "no host: $remote";
        $paddr   = sockaddr_in($port, $iaddr);

        $proto   = getprotobyname('tcp');
        socket(S, PF_INET, SOCK_STREAM, $proto)  || return "socket: $!";
        connect(S, $paddr)    || return "connect: $!";
        select(S); $| = 1; select(STDOUT);



        print S "GET /$url\n";
        if ($url =~ /.jpg/i || $url =~ /.jpeg/i) {
            $size = &jpegsize(S);
        } elsif($url =~ /.gif/i) {
            $size = &gifsize(S);
        } elsif($url =~ /.xbm/i) {
            $size = &xbmsize(S);
        } else {
            return "";
        }               
        $_ = $size;
        if(  /\s*width\s*=\s*([0-9]*)\s*/i ){
            ($newwidth)= /\s*width\s*=\s*(\d*)\s*/i;
        }
        if(  /\s*height\s*=\s*([0-9]*)\s*/i ){
            ($newheight)=/\s*height\s*=\s*(\d*)\s*/i;
        }
    } else {
        $size="";
    }
    return $size;
}

sub dokill {
    kill 9,$child if $child;
}
