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

use strict;

require 'get_line.pl';

my $NOT_LOCAL = 1;
my $IS_LOCAL = 2;

$::modules = {};

if( $::CVS_ROOT eq "" ){
    $::CVS_ROOT = pickDefaultRepository();
}

my $CVS_MODULES;

if( defined($ENV{"OS"}) && $ENV{"OS"} eq "Windows_NT" ){
    $CVS_MODULES='modules';
}
else {
    $CVS_MODULES="$::CVS_ROOT/CVSROOT/modules";
}

open( MOD, "<$CVS_MODULES") || die "can't open $CVS_MODULES";
&parse_modules;
close( MOD );

1;

sub in_module {
    my($mod_map, $dirname, $filename ) = @_;
    my( @path );
    my( $i, $fp, $local );

    #
    #quick check if it is already in there.
    #
    if( $mod_map->{$dirname} ){
        return 1;
    }


    @path = split(/\//, $dirname);

    $fp = '';

    for( $i = 0; $i < @path; $i++){

        $fp .= ($fp ne '' ? '/' : '') . $path[$i];

        if( $local = $mod_map->{$fp} ){
            if( $local == $IS_LOCAL ){
                if( $i == (@path-1) ){
                    return 1;
                }
            }
            else {
                # Add directories to the map as we encounter them so we go
                #  faster
                if( $mod_map->{$dirname} == 0 ){
                    $mod_map->{$dirname} = $IS_LOCAL;
                }
                return 1;
            }
        }
    }
    if( $mod_map->{ $fp . '/' .  $filename} ) {
        return 1;
    }
    else {
        return 0;
    }
}


sub get_module_map {
    my($name) = @_;
    my($mod_map);
    $mod_map = {};
    &build_map( $name, $mod_map );
    return $mod_map;
}

sub parse_modules {
    my @finaloptions=();
    my $l;
    while( $l = &get_line ){
        @finaloptions=();

        my ($mod_name, $flag, @params) = split(/[ \t]+/,$l);
        while ( $flag =~ /^-.$/){
            if( $flag eq '-a' ){
                $flag="";
                last;
            }
            if ( $flag eq '-l' ){ # then keep it
                push @finaloptions, ($flag, shift @params);
                $flag= @params ? shift @params : "";
                next;
            }
            if( $flag =~ /^-.$/ ){ 
                shift @params; # skip parameter's argument 
                $flag= @params ? shift @params : "";
                next;
            }
            last; # No options found...
        }
        unshift @params, $flag if ( $flag ne "" );
        $::modules->{$mod_name} = [(@finaloptions,@params)];
    }
}

sub build_map {
    my ($name,$mod_map) = @_;
    my ($bFound, $local);

    $local = $NOT_LOCAL;
    $bFound = 0;

#    printf "looking for $name in %s<br>\n",join(",", @{$::modules->{$name}});
    for my $i ( @{$::modules->{$name}} ){
        $bFound = 1;
        if( $i eq '-l' ){
            $local = $IS_LOCAL;
        } 
        elsif( ($i eq $name) || !build_map($i, $mod_map )){
            $mod_map->{$i} = $local;   
        }
    }
    return $bFound;
}
