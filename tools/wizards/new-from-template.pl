#!/usr/bin/perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla template processor.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1997-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

use strict;
use FileHandle;
use Getopt::Std;

my $pnm = $0;
my $description_var = "template_description";

my %opts;
getopts ("t:o:fdh?", \%opts);

if ($opts{"?"}) {
    usage();
}

my $vars_file = $opts{"t"} || usage();
my $out_dir = $opts{"o"} || "nft-results/";
if (!($out_dir =~ /\/$/)) {
    $out_dir = $out_dir . "/";
}

my @dereferences_in_progress = ();
my %vars;

$vars{"top_wizard_dir"} = {
                           value => "./",
                           processed => 1
                          };

if ($opts{"h"}) {
    load_vars_file($vars_file);
    show_description();
    exit 0;
}

if (-d $out_dir) {
    if ($opts{"f"}) {
        if ($opts{"d"}) {
            `rm -rf $out_dir`;
        }
    } elsif ($opts{"d"}) {
        die "$pnm: bad option: -d (delete output directory, recusive) can " .
          "only be used with -f (force.)\n";
    } else {
        die "$pnm: output directory ($out_dir) exists: use the -f (force) " .
          "option to continue anyway.  Files in the output directory with the " .
          "same name as template files will be overwritten.\n";
    }
}

load_vars_file($vars_file);
process_all_vars ();
process_template_dir (get_var("template_dir", $pnm), $out_dir);

exit 0;

sub usage () {
    print STDERR
      "Usage: $pnm -t <template-file> [-o <output-dir>]\n" .
      "            [-f[d]] [-h] [-?]\n" .
      "\n" .
      " -t FILE       The template to use.\n" .
      " -o DIRECTORY  The directory to write results to.\n" .
      " -f            Force overwriting of files in the output directory.\n" .
      " -d            Recursively delete the output directory before starting\n" .
      "               (requires -f.)\n" .
      " -h            Display description of the selected template.  Template\n" .
      "               will not be processed.\n" .
      " -?            Display this message.\n";

    exit 1;

}

sub show_description () {
    my $desc = get_var_with_default ($description_var);

    if ($desc) {
        print "Description of $vars_file:\n\n$desc\n\n";
    } else {
        print "Template file $vars_file contains no description.\n";
    }

}

sub process_template_dir {
    my ($from_dir, $to_dir) = @_;

    print "Processing template files $from_dir -> $to_dir\n";

    if (!( -d $to_dir)) {
        print "mkdir $to_dir\n";
        mkdir $to_dir;
    }
    
    my @dirs = get_subdirs($from_dir);
    my $dir;
    foreach $dir (@dirs) {
        process_template_dir ("$from_dir$dir/", "$to_dir$dir/");
    }

    my @files = get_files($from_dir);
    my $file;
    foreach $file (@files) {
        my $to_file = get_var_with_default ("filename:$file", $file,
                                            "rename target for file $file");
        open (TO,  ">$to_dir$to_file") || die "Couldn't open $to_dir$to_file\n";
        print TO process_file ("$from_dir$file");
        close TO;
    }
}

sub load_vars_file {
    my ($filename) = @_;
    my $fh = new FileHandle;

    $fh->open ($filename) || die ("Couldn't open vars file $filename\n");

    my $line;
    my $line_no = 1;
    my $continue_line;
    my $var_name = "";
    my $var_value = "";

    while ($line = <$fh>) {
        chomp($line);
        if ($continue_line) {
            # continuation of previous line
            $line =~ /^(.*)(\\)?$/;
            $var_value .= $1;
            $continue_line = $2 ? 1 : 0;
        } elsif ($line =~ /^\s*([\w\:\-]+)\s*=\s*(.*)$/) {
            # var=value line
            $var_name = $1;
            $var_value = $2;
            if ($var_value =~ /\\$/) {
                $continue_line = 1;
                $var_value .= "$var_value\n";
            } else {
                $continue_line = 0;
            }
        } elsif ($line =~ /^include\s*\"(.*?)\"\s*$/i) {
            # include line
            load_vars_file(process_string($1, "$filename, line $line_no"));
        } elsif ($line =~ /^rename\s*\(\s*\"(.*?)\"\s*,\s*\"(.*?)\"\s*\)\s*$/i) {
            # rename line
            $var_name = "filename:$1";
            $var_value = $2;
        } elsif ($line =~ /^(\s*\#.*)?$/) {
            # comment or blank line, ignore
            $var_name = "";
        } else {
            die ("Huh?\nFile: $filename, Line: $line_no\n$line\n");
        }

        if ($var_name && !$continue_line) {
            if ($var_name ne $description_var ||
                $filename eq $vars_file) {
                # don't set the description unless it's comming from the
                # main template file.
                $vars{$var_name} = {
                                    value => $var_value,
                                    processed => 0
                                   };
            }
        }
        ++$line_no;
    }

    close ($fh);
}

sub get_var_with_default {
    my ($var_name, $default, $source) = @_;
    my $c = $vars{$var_name};

    #print "getting var $var_name\n";

    if (!$c) {
        return $default;
    }
        
    if (!$c->{"processed"}) {
        if (grep(/^$var_name$/, @dereferences_in_progress)) {
            die "Circular reference to $var_name while processing $source\n";
        }
        push (@dereferences_in_progress, $var_name);
        my $val = $c->{"value"};
        if ($val =~ /^\s*file\s*\(\s*\"(.*)\"\s*\)\s*$/i) {
            # get value from a file
            #print "loading $1\n";
            $c->{"value"} = process_file (process_string($1, $source));
        } elsif ($val =~ /^eval\s*\(\s*\"(.*)\"\s*\)\s*$/i) {
            # get value from a perl eval() call
            my $eval_str = process_string ($1, $source);
            $c->{"value"} = eval ($eval_str);
        } else {
            $c->{"value"} = process_string ($val, "variable $var_name");
        }
        $c->{"processed"} = 1;
        if (pop(@dereferences_in_progress) ne $var_name) {
            die "Internal error: dereference stack mismatch.";
        }
    }

    return $c->{"value"};
}

sub get_var {
    my ($var_name, $source) = @_;
    my $c = $vars{$var_name};
    my $default;

    #print "getting var $var_name\n";

    if (!$c) {
        if ($var_name =~ /filename:(.+)/i) {
            $default = $1;
        } else {
            die ("Undefined variable $var_name referenced in $source\n");
        }
    }
        
    return get_var_with_default ($var_name, $default, $source);
    
}

sub process_file {
    my ($file) = @_;

    #print "processing $file\n";
    open (FROM, "$file") || die "Couldn't open $file\n";
    my @contents = <FROM>;
    my $results = process_string (join ("", @contents), "file $file");
    close FROM;

    return $results;
}

sub process_string {
    my ($str, $source) = @_;

    my @lines = split(/\n/, $str, -1);
    my $i;
    for $i (0 .. $#lines) {
        @lines[$i] = process_single_line_string ($lines[$i],
                                                 "$source, line " . ($i + 1));
    }
    return join ("\n", @lines);
}

sub process_single_line_string {
    my ($str, $source) = @_;
    my $start_pos = index ($str, '${');
    if ($start_pos == -1) {
        return $str;
    }
    my $end_pos = -1;
    my $str_len = length ($str);

    #print "processing string '$str'\n";

    my $result_str = substr($str, 0, $start_pos);

    while ($start_pos != -1 && $start_pos != $str_len) {
        $start_pos += 2;
        $end_pos = index ($str, "}", $start_pos);

        if ($end_pos == -1) {
            $end_pos = $start_pos;
        } else {
            my $var_name = substr ($str, $start_pos, $end_pos - $start_pos);
            $result_str .= get_var ($var_name, $source);
        }
        $start_pos = index ($str, '${', $end_pos);
        if ($start_pos == -1) {
            $start_pos = $str_len;
        }
        $result_str .= substr ($str, $end_pos + 1, $start_pos - $end_pos - 1);
    }

    return $result_str;
}

sub process_all_vars {
    # calling get_var on all variables causes them all to be dereferenced.
    # useful for flushing out undefined variables.
    my $var_name;
    
    foreach $var_name (keys(%vars)) {
        my $var_value = get_var ($var_name);
        #print "var name $var_name is $var_value\n";
    }
}

#
# given a uuid, return it in a format suitable for #define-ing
#
sub define_guid {
    my ($uuid) = @_;

    $uuid =~ /([a-f\d]+)-([a-f\d]+)-([a-f\d]+)-([a-f\d]+)-([a-f\d]+)/i;
    my $uuid_out = "{ /* $uuid */         \\\n" .
                   "     0x$1,                                     \\\n" .
                   "     0x$2,                                         \\\n" .
                   "     0x$3,                                         \\\n";

    my @rest =  ( "0x" . substr ($4, 0, 2), "0x" . substr ($4, 2, 2) );
    my $i = 0;
    while ($i < length ($5)) {
        push (@rest, "0x" . substr ($5, $i, 2));
        $i += 2;
    }

    my $rv = $uuid_out;
    $rv .= "    {" . join (", ", @rest) . "} \\\n";
    $rv .= "}\n";

    return $rv;
}

#
# given a directory, return an array of all the files that are in it.
#
sub get_files {
    my ($subdir) = @_;
    my (@file_array, @subdir_files);
    
    opendir (SUBDIR, $subdir) || die ("couldn't open directory $subdir: $!");
    @subdir_files = readdir(SUBDIR);
    closedir(SUBDIR);
    
    foreach (@subdir_files) {
        my $file = $_;
        if (!($file =~ /[\#~]$/) && -f "$subdir$file") {
            $file_array[$#file_array+1] = $file;
        }
    }
    
    return @file_array;
}

#
# given a directory, return an array of all subdirectories
#
sub get_subdirs {
    my ($dir)  = @_;
    my @subdirs;
    
    if (!($dir =~ /\/$/)) {
        $dir = $dir . "/";
    }
        
    opendir (DIR, $dir) || die ("couldn't open directory $dir: $!");
    my @testdir_contents = readdir(DIR);
    closedir(DIR);
    
    foreach (@testdir_contents) {
        if ((-d ($dir . $_)) && ($_ ne 'CVS') && (!($_ =~ /^\..*/))) {
            @subdirs[$#subdirs + 1] = $_;
        }
    }

    return @subdirs;
}
