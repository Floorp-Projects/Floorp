# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Gervase Markham <gerv@gerv.net>

#################
#Bugzilla Test 8#
#####filter######

# This test scans all our templates for every directive. Having eliminated
# those which cannot possibly cause XSS problems, it then checks the rest
# against the safe list stored in the filterexceptions.pl file. 

# Sample exploit code: '>"><script>alert('Oh dear...')</script>

use strict;
use lib 't';

use vars qw(%safe);

use Support::Templates;
use File::Spec 0.82;
use Test::More tests => $Support::Templates::num_actual_files;
use Cwd;

# Undefine the record separator so we can read in whole files at once
my $oldrecsep = $/;
my $topdir = cwd;
$/ = undef;

foreach my $path (@Support::Templates::include_paths) {
    $path =~ m|template/([^/]+)/([^/]+)|;
    my $lang = $1;
    my $flavor = $2;

    chdir $topdir; # absolute path
    my @testitems = Support::Templates::find_actual_files($path);
    chdir $topdir; # absolute path
    
    next unless @testitems;
    
    # Some people require this, others don't. No-one knows why.
    chdir $path; # relative path
    
    # We load a %safe list of acceptable exceptions.
    if (!-r "filterexceptions.pl") {
        ok(0, "$path has templates but no filterexceptions.pl file. --ERROR");
        next;
    }
    else {
        do "filterexceptions.pl";
    }
    
    # We preprocess the %safe hash of lists into a hash of hashes. This allows
    # us to flag which members were not found, and report that as a warning, 
    # thereby keeping the lists clean.
    foreach my $file (keys %safe) {
        my $list = $safe{$file};
        $safe{$file} = {};
        foreach my $directive (@$list) {
            $safe{$file}{$directive} = 0;    
        }
    }

    foreach my $file (@testitems) {
        # There are some files we don't check, because there is no need to
        # filter their contents due to their content-type.
        if ($file =~ /\.(txt|png)\.tmpl$/) {
            ok(1, "($lang/$flavor) $file is filter-safe");
            next;
        }
        
        # Read the entire file into a string
        open (FILE, "<$file") || die "Can't open $file: $!\n";    
        my $slurp = <FILE>;
        close (FILE);

        my @unfiltered;

        # /g means we execute this loop for every match
        # /s means we ignore linefeeds in the regexp matches
        while ($slurp =~ /\[%(.*?)%\]/gs) {
            my $directive = $1;

            my @lineno = ($` =~ m/\n/gs);
            my $lineno = scalar(@lineno) + 1;

            # Comments
            next if $directive =~ /^[+-]?#/;        

            # Remove any leading/trailing + or - and whitespace.
            $directive =~ s/^[+-]?\s*//;
            $directive =~ s/\s*[+-]?$//;

            # Directives
            next if $directive =~ /^(IF|END|UNLESS|FOREACH|PROCESS|INCLUDE|
                                     BLOCK|USE|ELSE|NEXT|LAST|DEFAULT|FLUSH|
                                     ELSIF|SET|SWITCH|CASE)/x;

            # Simple assignments
            next if $directive =~ /^[\w\.\$]+\s+=\s+/;

            # Conditional literals with either sort of quotes 
            # There must be no $ in the string for it to be a literal
            next if $directive =~ /^(["'])[^\$]*[^\\]\1/;

            # Special values always used for numbers
            next if $directive =~ /^[ijkn]$/;
            next if $directive =~ /^count$/;
            
            # Params
            next if $directive =~ /^Param\(/;

            # Other functions guaranteed to return OK output
            next if $directive =~ /^(time2str|GetBugLink)\(/;

            # Safe Template Toolkit virtual methods
            next if $directive =~ /\.(size)$/;

            # Special Template Toolkit loop variable
            next if $directive =~ /^loop\.(index|count)$/;
            
            # Branding terms
            next if $directive =~ /^terms\./;
            
            # Things which are already filtered
            # Note: If a single directive prints two things, and only one is 
            # filtered, we may not catch that case.
            next if $directive =~ /FILTER\ (html|csv|js|url_quote|quoteUrls|
                                            time|uri|xml)/x;

            # Exclude those on the nofilter list
            if (defined($safe{$file}{$directive})) {
                $safe{$file}{$directive}++;
                next;
            };

            # This intentionally makes no effort to eliminate duplicates; to do
            # so would merely make it more likely that the user would not 
            # escape all instances when attempting to correct an error.
            push(@unfiltered, "$lineno:$directive");
        }  

        my $fullpath = File::Spec->catfile($path, $file);
        
        if (@unfiltered) {
            my $uflist = join("\n  ", @unfiltered);
            ok(0, "($lang/$flavor) $fullpath has unfiltered directives:\n  $uflist\n--ERROR");
        }
        else {
            # Find any members of the exclusion list which were not found
            my @notfound;
            foreach my $directive (keys %{$safe{$file}}) {
                push(@notfound, $directive) if ($safe{$file}{$directive} == 0);    
            }

            if (@notfound) {
                my $nflist = join("\n  ", @notfound);
                ok(0, "($lang/$flavor) $fullpath - FEL has extra members:\n  $nflist\n" . 
                                                                  "--WARNING");
            }
            else {
                # Don't use the full path here - it's too long and unwieldy.
                ok(1, "($lang/$flavor) $file is filter-safe");
            }
        }
    }
}

$/ = $oldrecsep;

exit 0;
