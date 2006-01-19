#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
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
# The Original Code is the Hendrix Feedback System.
#
# The Initial Developer of the Original Code is
# Gervase Markham.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# The Initial Developer wrote this software to the Glory of God.
# ***** END LICENSE BLOCK *****

use strict;

# This application requires installation of the "Email::Send" (note: not 
# Mail::Send) module.
use Template;
use CGI;
use Email::Send qw(NNTP);

# use CGI::Carp qw(fatalsToBrowser);

# Configuration
my $newsgroup = "mozilla.feedback";
my $server    = "news.mozilla.org";

my $cgi = new CGI;
my $form = $cgi->Vars;
my $vars;
$vars->{'form'} = $form;
$vars->{'newsgroup'} = $newsgroup;
$vars->{'stylesheet'} = "skin/planet.css";

my $template = Template->new({
    INCLUDE_PATH => ["template"],
    PRE_CHOMP => 1,
    TRIM => 1,
    FILTERS => {
        email => \&emailFilter,
    },  
}) || die("Template creation failed.\n");

my $action = $cgi->param("action");

if (!$action) {
    # If no action, show the submission form
    print "Content-Type: text/html\n\n";
    $template->process("index.html.tmpl", $vars)
      || die("Template process failed: " . $template->error() . "\n");
}
elsif ($action eq "submit") {
    # Format the parameters and send to the newsgroup.
    
    # Check for compulsory parameters
    if (!$form->{'name'} || !$form->{'subject'} || !$form->{'product'}) {
      throwError("bad_parameters");
    }
    
    my $message;
    my $headers;
    
    $template->process("message-headers.txt.tmpl", $vars, \$headers)
      || die("Template process failed: " . $template->error() . "\n");
    $template->process("message.txt.tmpl", $vars, \$message)
      || die("Template process failed: " . $template->error() . "\n");
       
    # Post formatted message to newsgroup
    my $newsMsg = Email::Simple->new($headers . "\n\n" . $message);
    my $success = send NNTP => $newsMsg, $server;

    throwError("cant_post") if (!$success);

    # Give user feedback on success
    $vars->{'headers'} = $headers;
    $vars->{'message'} = $message;
    
    print "Content-Type: text/html\n\n";
    $template->process("submit-successful.html.tmpl", $vars)
      || die("Template process failed: " . $template->error() . "\n");
}
else {
    die("Unknown action $action\n");
}

exit;

# Simple email obfuscation
sub emailFilter {
    my ($var) = @_;
    $var =~ s/\@/_at_/;
    return $var;
}

sub throwError {
    my ($error) = @_;
    $vars->{'error'} = $error;
    
    print "Content-Type: text/html\n\n";
    $template->process("error.html.tmpl", $vars)
      || die("Template process failed: " . $template->error() . "\n");
    
    exit;
}
