#!/usr/bin/perl -w
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
# The Original Code is Doctor.
#
# The Initial Developer of the Original Code is Netscape 
# Communications Corporation. Portions created by Netscape 
# are Copyright (C) 2002 Netscape Communications Corporation. 
# All Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder to do dangerous things in Perl.
use diagnostics;
use strict;

# Make it easier to access Perl's built-in variables by turning on the option 
# of referencing them by sensible name instead of punctuation mark.
use English;

# Include the standard Perl CGI library and create a new request object
# to handle this CGI request.
use CGI;
my $request = new CGI;

use Doctor qw($template $vars %CONFIG);

use Doctor::File;

################################################################################
# Script Configuration
################################################################################

# Store the home directory so we can get back to it after changing directories
# in certain places in the code.
use Cwd;
my $HOME = cwd;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value 
# of that variable and runs the appropriate code.

my $action = lc($request->param('action'));
if (!$action) {
  $action = $request->param('file') ? "edit" : "choose";
}

# Displays a form for choosing the page you want to edit.
if    ($action eq "choose")       { choose()      }

# Displays a page with UI for editing the page online, downloading it
# for local editing, viewing the original and the user's modified versions,
# getting a diff of the user's changes, and committing the user's changes
# or submitting them to the editors for review.  This is the principle
# interface to Doctor functionality, and many other actions are called
# from links, forms, and JavaScript on this page.
elsif ($action eq "edit")         { edit()        }

# Retrieves a page from CVS and returns it.  Called by the "download the page"
# link on the Edit page for users who want to download and edit pages locally
# instead of via the embedded textarea.  Also called by the View Original panel
# of the Edit page to show the original version of the page being edited.
elsif ($action eq "download" || $action eq "display")
                                  { retrieve()    }

# Generates and returns a diff of changes between the submitted version
# of a page and the version in CVS.  Called by the Show Diff panel of the
# Edit page.
elsif ($action eq "diff")         { diff()        }

# Returns the content that was submitted to it.  Useful for displaying
# the modified version of a page the user downloaded and edited locally,
# since for security reasons there's no way to get access to a file
# in a file upload control on the client side.  Called by JavaScript
# when the user focuses the View Edited panel on the Edit page after entering
# a filename into the file upload control.
elsif ($action eq "regurgitate")  { regurgitate() }

# Submits a change to the editors for review.  Requires the EDITOR_EMAIL config
# parameter to be set to the editors' email addresses.
elsif ($action eq "queue")        { queue()       }

# Commits changes to a (possibly new) page to the repository.
elsif ($action eq "create" || $action eq "commit")
                                  { commit()      }

else {
    ThrowCodeError("couldn't recognize the value of the action parameter",
                   "Unknown Action");
}

exit;

################################################################################
# Main Execution Functions
################################################################################

sub choose
{
  print $request->header;
  $template->process("select.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub edit {
    $vars->{file} = Doctor::File->new($request->param('file'));
    
    print $request->header;
    $template->process("edit.tmpl", $vars)
      || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub retrieve {
    my $file = Doctor::File->new($request->param('file'));
   
    my $disposition = $action eq "download" ? "attachment" : "inline";
      
    print $request->header(
        -type                =>  "text/html; name=\"" . $file->name . "\"",
        -content_disposition =>  "$disposition; filename=\"" . $file->name . "\"",
        -content_length      =>  length($file->content) );

    print $file->content;
}

sub diff {
    my $file = Doctor::File->new($request->param('file'));
    ValidateVersions($request->param('version'), $file->version);
    my $diff = $file->diff(GetContent())
      || "There are no differences between the version in CVS and your revision.";

    print $request->header(-type=>"text/plain");
    print $diff;
}

sub regurgitate {
    # Returns the content that was submitted to it.  Useful for displaying
    # the modified version of a document the user downloaded and edited locally
    # in the "View Edited" tab, since for security reasons there's no way
    # to get access to a file being uploaded on the client side.  When the user
    # clicks on the "View Edited" tab, client-side JS checks to see if there is
    # a value in the file upload control, and if so it adds an iframe
    # to the "View Edited" panel and posts the file to it with action=regurgitate.

    my $content = GetContent();
    my $filename = $request->param('content_file')
                   || $request->param('file')
                   || "modified.html";
    $filename =~ s/^(.*[\/\\])?([^\/\\]+)$/$2/;

    print $request->header(
        -type                   =>  qq|text/html; name="$filename"|,
        -content_disposition    =>  qq|inline; filename="$filename"|,
        -content_length         =>  length($content));
    print $content;
}

sub queue {
    # Sends the diff or new file to an editors mailing list for review.
    
    if (!$CONFIG{EDITOR_EMAIL}) {
        ThrowCodeError("The administrator has not enabled submission of patches
                        for review.", "Review Not Enabled");
    }
    
    my $file = Doctor::File->new($request->param('file'));
    my $comment = $request->param('comment') || "No comment.";
    my $content = GetContent() || "";

    my $email;
    use Email::Valid;
    if (!($email = Email::Valid->address($request->param('email')))) {
        ThrowUserError("address $email invalid: $Email::Valid::Details");
    }

    # Prefer the name of the file being uploaded, if any; otherwise append
    # ".diff" to the name of the file in CVS.
    my $filename = $request->param('content_file') || $file->name . ".diff";
    $filename =~ s/^(.*[\/\\])?([^\/\\]+)$/$2/;

    my ($patch, $version);
    if ($file->version eq "new") {
        $patch = $content;
        $version = "(new file)";
    }
    else {
        ValidateVersions($request->param('version'), $file->version);
        $patch = $file->diff($content);
        if (!$patch) {
            ThrowUserError("There are no differences between the version
                            in CVS and your revision.", "No Differences Found");
        }
        $version = "v" . $file->version;
    }

    my $subject = "patch: " . $file->spec . " $version";

    eval {
        use MIME::Entity;
        my $mail = MIME::Entity->build(Type     =>"multipart/mixed",
                                       From     => $email,
                                       To       => $CONFIG{EDITOR_EMAIL},
                                       Subject  => $subject);
        $mail->attach(Data        => \$comment,
                      Encoding    => "quoted-printable");
        $mail->attach(Data        => \$patch,
                      Encoding    => "quoted-printable",
                      Disposition => "inline",
                      Filename    => $filename);
        # Set the record separator because otherwise MIME::Entity seems
        # to get stuck in an infinite loop.
        $/ = "\n";
        $mail->send();
    };
    if ($@) {
        ThrowCodeError($@, "Mail Failure");
    }

    $vars->{file} = $file;

    print $request->header;
    $template->process("queued.tmpl", $vars)
      || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub commit {
    # Commits a (possibly new) file to the repository.
    my $file = Doctor::File->new($request->param('file'));

    my $username = ValidateUsername();
    my $password = ValidatePassword();
    my $comment = ValidateComment();

    if ($action eq "commit") {
        ValidateVersions($request->param('version'), $file->version);
        $file->patch(GetContent());
    }
    else {
        $file->add(GetContent());
    }
    my ($rv, $output, $errors) = $file->commit($username, $password, $comment);

    if ($rv != 0) {
        ThrowUserError("An error occurred while committing the file: $output $errors",
                       undef,
                       $output,
                       $rv,
                       $errors);
    }
  
    $vars->{file} = $file;
    $vars->{output} = $output;
    $vars->{errors} = $errors;

    print $request->header;
    $template->process("committed.tmpl", $vars)
      || ThrowCodeError($template->error(), "Template Processing Failed");
}

################################################################################
# Input Validation
################################################################################

sub ValidateUsername {
    $request->param('username')
      || ThrowUserError("You must enter your username.");
  
    my $username = $request->param('username');
  
    # If the username has an at sign in it, convert it to a percentage sign,
    # since that's probably what the user meant.  CVS usernames are often email
    # addresses in which the at sign has been converted to a percentage sign,
    # and since at signs aren't legal characters in CVS usernames, it's a good
    # bet that any occurrences are supposed to be percentage signs.
    $username =~ s/@/%/;
  
    return $username;
}

sub ValidatePassword {
    $request->param('password')
      || ThrowUserError("You must enter your password.");
    return $request->param('password');
}

sub ValidateComment {
    $request->param('comment')
      || ThrowUserError("You must enter a check-in comment describing your changes.");
    return $request->param('comment');
}

sub ValidateVersions() {
    # Throws an error if the version of the file that was edited
    # does not match the version in the repository.  In the future
    # we should try to merge the user's changes if possible.
  
    my ($oldversion, $newversion) = @_;
  
    if ($oldversion && $newversion && $oldversion != $newversion) {
        ThrowCodeError("You edited version <em>$oldversion</em> of the file,
          but version <em>$newversion</em> is in the repository.  Reload the edit 
          page and make your changes again (and bother the authors of this script 
          to implement change merging
          (<a href=\"http://bugzilla.mozilla.org/show_bug.cgi?id=164342\">bug 164342</a>).");
    }
}


################################################################################
# Error Handling
################################################################################

sub ThrowUserError {
    # Throw an error about a problem with the user's request.  This function
    # should avoid mentioning system problems displaying the error message, since
    # the user isn't going to care about them and probably doesn't need to deal
    # with them after fixing their own mistake.  Errors should be gentle on 
    # the user, since many "user" errors are caused by bad UI that trip them up.
    
    # !!! Mail code errors to the system administrator!

    ($vars->{'message'}, 
     $vars->{'title'}, 
     $vars->{'cvs_command'}, 
     $vars->{'cvs_error_code'}, 
     $vars->{'cvs_error_message'}) = @_;
    
    chdir($HOME);
    print $request->header;
    $template->process("user-error.tmpl", $vars)
      || print( ($vars->{'title'} ? "<h1>$vars->{'title'}</h1>" : "") . 
                "<p>$vars->{'message'}</p><p>Please go back and try again.</p>" );
    exit;
}

sub ThrowCodeError {
    # Throw error about a problem with the code.  This function should be
    # apologetic and deferent to the user, since it isn't the user's fault
    # the code didn't work.
    
    # !!! Mail code errors to the system administrator!
    
    ($vars->{'message'}, $vars->{'title'}) = @_;
    
    chdir($HOME);
    print $request->header;
    $template->process("code-error.tmpl", $vars)
      || print("
            <p>
            Unfortunately Doctor has experienced an internal error from which
            it was unable to recover.  More information about the error is
            provided below. Please forward this information along with any
            other information that would help diagnose and fix this problem
            to the system administrator at
            <a href=\"mailto:$CONFIG{ADMIN_EMAIL}\">$CONFIG{ADMIN_EMAIL}</a>.
            </p>
            <p>
            couldn't process error.tmpl template: " . $template->error() . 
            "; error occurred while trying to display error message: " . 
            ($vars->{'title'} ? "$vars->{'title'}: ": "") . $vars->{'message'} .
            "</p>");
    exit;
}

################################################################################
# Misc
################################################################################

sub GetContent {
    my $fh = $request->upload('content_file');
    my $content;
    if ($fh) {
        local $/; # enable 'slurp' mode
        $content = <$fh>;
    }
    if (!$content) {
        $content = $request->param('content');
    }
    return $content;
}
