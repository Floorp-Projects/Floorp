#!/usr/bin/perl -wT 
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Garrett Braden <garrett@tnbd.org>

# This script reads in an environment in xml and persists it to the database.


=head1 NAME

tr_import_environment.cgi - Imports an environment from a XML file into the database.

=head1 DESCRIPTION

Administrator's and testopia users can import an environment from XML into the database.
A tool is being designed to automatically scan a system and create such an XML file to
import.

=cut

#************************************************** Uses ****************************************************#
use strict;
use CGI;
use lib ".";
use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Xml;
use Data::Dumper;
use vars qw($vars);

#************************************ Variable Declarations/Initialization **********************************# 
Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';
my $upload_dir = "testopia/temp";
my $env_filename = $cgi->param('env_file');
my $env_fh = $cgi->upload('env_file');
my $action = $cgi->param('action') || '';
my $xml = $cgi->param('xml');
my $submit = $cgi->param('submit');
my $environment = Bugzilla::Testopia::Environment->new({'environment_id' => 0});
my $user_can_edit = $environment->canedit;
#my $user_can_edit = 0;                                                     # Remove on production <- used to toggle user's rights.
my $message = '';
$CGI::POST_MAX = 1024 * 500;        # max file size 500K

#***************************************** UI Logic ************************************************************#
print $cgi->header;
# Make sure the file isn't too big.
if (!$env_filename && $cgi->cgi_error()) {
    $vars->{'tr_error'} .= "File size cannot exceed 500K.<BR/>";
}
# Upload the file and read it into a string if it's been posted.
if ($action eq 'import' && $env_filename) {
    trick_taint($env_filename);
    # Continue only if import exits without errors, otherwise display the errors.
    if (slurp_env_file() && $xml && import()) {
        # Check for new data
        if (check_new_items()) {
            if (write_env_file()) {
                # If there's new data and the user can edit have them approve the changes.
                if ($user_can_edit) {
                    admin_approve();
                }
                else {
                    $vars->{'tr_error'} .= "Please contact an admin who can import the above mentioned non-existing data.<BR/>";
                }
            }
        }
        # If there's no new Categories, Elements, or Properties then store the environment.
        else {
            store_environment();      # Stores the new Environment values based on existing Categories, Elements, Properties, and Valid Expressions.
        }
    }
}
# If the admin form posted
elsif ($action eq 'admin' && $xml) {
    if ($user_can_edit) {
        my $file = "$upload_dir/$xml";
        trick_taint($file);
        # If the admin clicked the 'Add Now' and not the 'Cancel' button
        if ($submit eq 'Add Now') {
            if (read_env_file($file)) {
                import("admin");              # Creates Bugzilla::Testopia::Environment::Xml object and store's it to the database.
            }
        }
        else {
            $vars->{'tr_error'} .= "Import XML Environment Cancelled.<BR/>";
            delete_env_file($file);
        }
    }
    else {
        $vars->{'tr_error'} .= "Error reading from file.  Please try again.<BR/>If the problem persists please contact the site administrator.<BR/>";
    }
}
display();
#*********************************************** EXISTS HERE *************************************************#




#************************************* Sub Routine Deffinitions Below ****************************************#


=head2 Create XML document

=head2 DESCRIPTION

Creates the Environment XML Document and node lists.

=cut

sub import {
    # If the user is an admin and has approved to add the values pass 1 to parse().
    my $admin = @_;
    $environment = Bugzilla::Testopia::Environment::Xml->new($xml, $admin);
    if ($environment->{'error'}) {
        $vars->{'tr_error'} .= $environment->{'error'};
        return 0;
    }
    return 1;
}


=head2 Checking Exists

=head2 DESCRIPTION

Checking if the Environment, Elements, Categories, and Properties already exist or not.

=cut

sub check_new_items {
    return $environment->check_new_items();
}


=head2 Admin Approve

=head2 Description

Prepares the $vars being passed to the template to display the
Admin Approval Form first before importing an environment with
new data.

=cut

sub admin_approve {
    $vars->{'action'} = "admin";
    $vars->{'xml'} = $xml;
}


=head2 Store the Environment

=head2 Description

Stores the Environment based on existing Categories, Elements, Properties, and selectable values.

=cut

sub store_environment {
    $environment->store();
}


=head2 Upload and read the XML file

=head2 Description

Uploads and reads the XML Environment file and prepares it into an array for parsing.

=cut

sub slurp_env_file {
    my $untainted_filename;
    my $post_max = $CGI::POST_MAX;
    if (!$env_filename) {
        $vars->{'tr_error'} .= "Please upload an Environment XML file.<BR/>";
        return 0;
    }
    # untaint $env_file
    if ($env_filename =~ /^([-\@:\/\\\w.]+)$/) {
        $untainted_filename = $1;
    }
    else {
        $vars->{'tr_error'} .= "The filename must contain numbers and letters only.  Please try again.<BR/>";
        return 0;
    }
    if ($untainted_filename =~ m/\.\./) {
        $vars->{'tr_error'} .= "The filename cannot conain the sequence '..'  Please try again.<BR/>";
        return 0;
    }        
    my $num_bytes = $CGI::POST_MAX;
    my ($totalbytes, $byteswritten, $buffer);
    while ($byteswritten = read($env_fh, $buffer, $num_bytes)) {
        $xml .= $buffer;
        $totalbytes += $num_bytes;
    }
    if (!($byteswritten && $totalbytes)) {
        $vars->{'error'} .= "Problem uploading file " . $env_filename . ".<BR/>"
    }
    if (!$xml) {
        $vars->{'tr_error'} .= "File is empty.  Please upload a valid Environment XML file.<BR/>";
        return 0;
    }
    if (length($xml) > $post_max) {
        $vars->{'tr_error'} .= "File uploaded was too large.  Please make sure the file size is no bigger than 500 KB.<BR/>";
        return 0;
    }
    return 1;
}


=head2 Write the Environment XML File to the Server

=head Description

Writes the environment XML File to the $upload_dir directory where it will be read in later by the admin to be approved. Once approved it's deleted.

=cut

sub write_env_file {
    # If a writable $upload_dir exists, log error details there.
    if (-w "$upload_dir") {
        my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
        my $filename = $env_filename;
        $filename =~ s/(.xml)//;
        $filename .= "_" . $timestamp;
        $filename =~ s/\:/./g;
        $filename =~ s/[\: -]/_/g;
        $filename .= ".xml";
        my $untainted_filename;
        if (!$filename) {
            $vars->{'tr_error'} .= "Please upload an Environment XML file.<BR/>";
            return 0;
        }
        # untaint $env_file
        if ($filename =~ /^([-\@:\/\\\w.]+)$/) {
            $untainted_filename = $1;
        }
        else {
            $vars->{'tr_error'} .= "The filename must contain numbers and letters only.  Please try again.<BR/>";
            return 0;
        }
        if ($untainted_filename =~ m/\.\./) {
            $vars->{'tr_error'} .= "The filename cannot conain the sequence '..'  Please try again.<BR/>";
            return 0;
        }
        open (my $fh,  ">$upload_dir/$filename") || die "PROBLEM WRITING FILE.<BR/>";
        print $fh $xml;
        close $fh;
        $xml = $filename;
    }
    else {
        $vars->{'tr_error'} .= "Unable to write temporary environment file.  Please try again.<BR/>If the problem persists please contact the site administrator.<BR/>";
        return 0;
    }
    return 1;
}


=head2 Read the uploaded Environment XML file

=head2 Description

Uploads and reads the XML Environment file and prepares it into an array for parsing.

=cut

sub read_env_file {
    my ($file) = @_;
    $xml = '';
    open (my $fh, "<$file") || do {
        $vars->{'tr_error'} .= "Unable to read temporary environment file.  Please to try again.<BR/>If the problem persists please contact the site administrator.<BR/>";
        return 0;
    };
    binmode($fh);
    while (<$fh>) {
        $xml .= $_;  
    }
    close $fh;
    if (!$xml) {
        $vars->{'tr_error'} .= "File is empty.  Please upload a valid Environment XML file.<BR/>";
        return 0;
    }
    if (length($xml) > $CGI::POST_MAX) {
        $vars->{'tr_error'} .= "File uploaded was too large.  Please make sure the file size is no bigger than 500 KB.<BR/>";
        return 0;
    }
    return delete_env_file($file);
}


=head2 delete_env_file

=head2 Description

Deletes the temporary environment file.

=cut

sub delete_env_file {
    my ($file) = @_;
    if (!unlink($file)) {
        $vars->{'tr_error'} .= "Unable to delete temporary environment file.<BR/>If the problem persists please contact the site administrator.<BR/>";
        return 0;
    }
    return 1;
}


=head2 Display

=head2 Description

Displays the Import Environment template
 
=cut

sub display {
    $vars->{'tr_message'} .= $message . $environment->{'message'};
    $template->process("testopia/environment/import.xml.tmpl", $vars) || print $template->error();
}