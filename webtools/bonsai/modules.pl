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


$NOT_LOCAL = 1;
$IS_LOCAL = 2;

$modules = {};

if( $CVS_ROOT eq "" ){
    $CVS_ROOT = pickDefaultRepository();
}

if( $ENV{"OS"} eq "Windows_NT" ){
    $CVS_MODULES='modules';
}
else {
    $CVS_MODULES="${CVS_ROOT}/CVSROOT/modules";
}

open( MOD, "<$CVS_MODULES") || die "can't open $CVS_MODULES";
&parse_modules;
close( MOD );

1;

sub in_module {
    local($mod_map, $dirname, $filename ) = @_;
    local( @path );
    local( $i, $fp, $local );

    #
    #quick check if it is already in there.
    #
    if( $mod_map{$dirname} ){
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
                if( $mod_map{$dirname} == 0 ){
                    $mod_map{$dirname} = $IS_LOCAL;
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
    local($name) = @_;
    local($mod_map);
    $mod_map = {};
    &build_map( $name, $mod_map );
    return $mod_map;
}

sub parse_modules {
    local @finaloptions=();
    while( $l = &get_line ){
        @finaloptions=();

        ($mod_name, $flag, @params) = split(/[ \t]+/,$l);
        while ( $flag =~ /^-.$/){
            if( $flag eq '-a' ){
                $flag="";
                last;
            }
            if ( $flag eq '-l' ){ # then keep it
                push @finaloptions, ($flag, shift @params);
                $flag= shift @options;
                next;
            }
            if( $flag =~ /^-.$/ ){ 
                shift @params; # skip parameter's argument 
                $flag = shift @params;
                next;
            }
            last; # No options found...
        }
        unshift @params, $flag if ( $flag ne "" );
        $modules->{$mod_name} = [(@finaloptions,@params)];
    }
}

sub build_map {
    local($name,$mod_map) = @_;
    local($bFound, $local);

    $local = $NOT_LOCAL;
    $bFound = 0;

#    printf "looking for $name in %s<br>\n",join(",", @{$modules->{$name}});
    for $i ( @{$modules->{$name}} ){
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



sub get_line {
    local($l, $save);
    
    $bContinue = 1;

    while( $bContinue && ($l = <MOD>) ){
        chop($l);
        if( $l =~ /^[ \t]*\#/ 
                || $l =~ /^[ \t]*$/ ){
            $l='';              # Starts with a "#", or is only whitespace.
        }
        if( $l =~ /\\[ \t]*$/ ){
            # Ends with a slash, so append it to the last line.
            chop ($l);
            $save .= $l . ' ';
        }
        elsif( $l eq '' && $save eq ''){
            # ignore blank lines
        }
        else {
            $bContinue = 0;
        }
    }
    return $save . $l;
}
