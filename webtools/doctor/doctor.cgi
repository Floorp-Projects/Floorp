#!/export2/gnu/files/perl-5.6.0/bin/perl -w
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

# Use a useful library for capturing filehandle output into a variable.
# http://groups.google.com/groups?selm=yr0e8.12924%24ZC3.1033373%40newsread2.prod.itd.earthlink.net
use IO::Capture;

# Include the Perl library for creating and deleting directory hierarchies.
use File::Path;

# Use the Template Toolkit (http://www.template-toolkit.org/) to generate
# the user interface using templates in the "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to templates processed in this script.
my $template = Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "templates" ,

    FILTERS => 
    {
        linebreak => sub 
        {
          my ($var) = @_; 
          $var =~ s/\n/\&nl;/g;
          $var =~ s/\r/\&cr;/g;
          return $var;
        }
    }
  }
);

# Define the global variables and functions that will be passed to the UI
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
my $vars = {};

################################################################################
# Script Configuration
################################################################################

# Note: Look in the configuration file for descriptions of each parameter.

# Use the AppConfig module for handling script configuration.
use AppConfig qw(:expand :argcount);

# Create an AppConfig object and populate it with parameters defined
# in the configuration file.
my $config = AppConfig->new(
  { 
    CREATE => 1 , 
    GLOBAL => {
      ARGCOUNT => ARGCOUNT_ONE , 
    }
  }
);
$config->file("doctor.conf");

# Write parameters into global variables.
my $TEMP_DIR = $config->get('TEMP_DIR');
my $READ_CVS_SERVER = $config->get('READ_CVS_SERVER');
my $READ_CVS_USERNAME = $config->get('READ_CVS_USERNAME');
my $READ_CVS_PASSWORD = $config->get('READ_CVS_PASSWORD');
my $WRITE_CVS_SERVER = $config->get('WRITE_CVS_SERVER');

# Store the home directory so we can get back to it after changing directories
# in certain places in the code.
use Cwd;
my $HOME = cwd;

# Whether or not the user submitted their username with an at sign (@) instead
# of a percentage mark (%).  CVS usernames cannot contain at signs, so the sign
# gets replaced by a percentage mark when people use their email addresses as
# usernames, but those people generally forget about the replacement and enter
# their username with the at sign, so we convert it automatically and set this
# variable so we know to warn them about it if it happens.
my $at_sign = 0;

################################################################################
# Main Body Execution
################################################################################

# Note: All calls to this script should contain an "action" variable 
# whose value determines what the user wants to do.  The code below 
# checks the value of that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = lc($request->param('action')) || "edit";

if ($action eq "edit")
{
  ValidateFile();
 
  $vars->{'file'} = $request->param('file');
  
  if ($request->param('content') && $request->param('version'))
  {
    ValidateContent();
    $vars->{'content'} = $request->param('content');
    $vars->{'version'} = $request->param('version');
  }
  else
  {
    ($vars->{'content'}, $vars->{'version'}) = RetrieveFile($request->param('file'));
  }
  
  print $request->header;
  $template->process("edit.tmpl", $vars)
    || DisplayError("Template Process Failed", $template->error())
    && exit;
}
elsif ($action eq "review")
{
  ReviewChanges();
}
elsif ($action eq "commit")
{
  ValidateContent();
  ValidateUsername();
  ValidatePassword();
  ValidateComment();
  CommitFile();
}
else
{
  DisplayError("I could not figure out what you wanted to do.", "Unknown action");
}

exit;

################################################################################
# Input Validation
################################################################################

sub ValidateFile
{
  $request->param("file")
    || DisplayError("You must include the name of the file.")
    && exit;
  
  my $file = $request->param("file");

  # Collapse multiple consecutive slashes (i.e. dir//file.txt) 
  # into a single slash.
  $file =~ s:/{2,}:/:;

  # If the filename (the last name in the path) contains no period, it is
  # probably a directory, so add a slash.
  if ($file =~ m:^[^\./]+$: || $file =~ m:/[^\./]+$:) { $file .= "/" }

  # If the file ends with a forward slash, it is a directory, so add
  # the name of the default file.
  if ($file =~ m:/$:) { $file .= "index.html" }

  $request->param("file", $file);
  
  # Note: we don't need to validate further because CVS will tell us
  # whether or not the filename is valid.
}

sub ValidateUsername
{
  $request->param('username')
    || DisplayError("You must enter a username.")
    && exit;

  my $username = $request->param('username');
  if ($username =~ s/@/%/) { $at_sign = 1 };
  $request->param('username', $username);
}

sub ValidatePassword
{
  $request->param('password')
    || DisplayError("You must enter a password.")
    && exit;
}

sub ValidateComment
{
  $request->param('comment')
    || DisplayError("You must enter a check-in comment.")
    && exit;
}

sub ValidateContent
{
  my $content = $request->param('content');
  # The HTML spec tells browsers to remove line breaks (carriage return
  # and newline characters) from form values in some cases.  This happens
  # even if those characters are encoded into entities (&#10; and &#13;).
  # To prevent corruption of file content in this case, we have to convert
  # those characters into our own custom entities (&cr; and &nl;), when
  # using them as form values, so here we convert them back.
  $content =~ s/\&nl;/\n/g;
  $content =~ s/\&cr;/\r/g;
  $request->param('content', $content);
}

################################################################################
# Utility Functions
################################################################################

sub RetrieveFile
{
  my ($name) = @_;
  
  my @args = ("-d",
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "checkout",
              "-p", # write to STDOUT
              $name);
  
  # Check out the file from the CVS repository, capturing the file content
  # and any errors/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;

  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the username/password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    DisplayError("I could not check the file out from the repository.<br> 
                  <strong>Command:</strong> <em>$command</em><br>
                  <strong>Error Code:</strong> <em>$error_code</em><br> 
                  <strong>Error Message:</strong><b><pre>$errors</pre>");
    exit;
  }
  
  # Extract the file version from the errors/notices.
  my $version = "";
  if ($errors =~ /VERS:\s([0-9.]+)\s/) { $version = $1 }
  
  return ($output, $version);
}

sub ReviewChanges 
{
  # Displays a diff between the version of a file in the repository
  # and the version submitted by the user so the user can review the
  # changes they are making to the file.  Offers the user the option
  # to either commit their changes or edit the file some more.
  
  # Create and change to a temporary sub-directory into which 
  # we will check out the file being reviewed.
  CreateTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  # Throw an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    DisplayError("You edited version <em>$oldversion</em> of the file,
                  but version <em>$newversion</em> is in the repository.
                  Reload the edit page and make your changes again
                  (and bother the authors of this script to implement
                  change merging).");
    DeleteTempDir();
    exit;
  }
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file, and display it to the user.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  
  # Delete the temporary directory into which we checked out the file.
  DeleteTempDir();
  
  $vars->{'file'} = $file;
  $vars->{'content'} = $request->param('content');
  $vars->{'version'} = $request->param('version');
  
  print $request->header;
  $template->process("review.tmpl", $vars)
    || DisplayError("Template Process Failed", $template->error())
    && exit;
}

sub CommitFile 
{
  # Commit a file to the repository.  Checks out the file via cvs, 
  # applies the changes, generates a diff, and then checks the file 
  # back in.  It would be easier if we could commit a file by piping
  # it into standard input, but cvs does not provide for this.
  
  # Create and change to a temporary sub-directory into which 
  # we will check out the file being committed.
  CreateTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  # Throw an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    DisplayError("You edited version <em>$oldversion</em> of the file,
                  but version <em>$newversion</em> is in the repository.
                  Reload the edit page and make your changes again
                  (and bother the authors of this script to implement
                  change merging).");
    DeleteTempDir();
    exit;
  }
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file and the version in the repository,
  # and check in the edited file.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  $vars->{'checkin_results'} = CheckInFile($file);
  
  # Delete the temporary directory into which we checked out the file.
  DeleteTempDir();
  
  $vars->{'file'} = $file;
  $vars->{'at_sign'} = $at_sign;
  
  print $request->header;
  $template->process("committed.tmpl", $vars)
    || DisplayError("Template Process Failed", $template->error())
    && exit;
}

sub CheckOutFile
{
  # Checks out a file from the repository.
  
  my ($file) = @_;
  
  my @args = ("-t", # debugging messages from which to extract the version
              "-d", 
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "checkout", 
              $file);
  
  # Check out the file from the repository, capturing the output of the
  # command and any error messages/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    DisplayError("I could not check the file out from the repository.<br> 
                  <strong>Command:</strong> <em>$command</em><br>
                  <strong>Error Code:</strong> <em>$error_code</em><br> 
                  <strong>Error Message:</strong><b><pre>$errors</pre>");
    
    DeleteTempDir();
    exit;
  }

  # Extract the file version from the errors/notices.
  my $version = "";
  if ($errors =~ /\Q$file\E,v,\s([0-9.]+),/) { $version = $1 }
  
  return $version;
}

sub ReplaceFile
{
  # Replaces the file checked out from the repository with the edited version.
  
  my ($file) = @_;
  
  my $content = $request->param('content');
  
  # Replace funky end-of-line combinations with good ole unix equivalents.
  $content =~ s/\r\n|\r/\n/g;
  
  if (!open(DOC, ">$file"))
  {
    DisplayError("I could not open the temporary file <em>$file</em> 
                  for writing: $!");
    DeleteTempDir();
    exit;
  }
  print DOC $content;
  close(DOC);
}

#$windows = ($content =~ /\r\n/);
#$unix = ($content =~ /\n/);
#$mac = ($content =~ /\r/);

sub DiffFile
{
  # Returns the results of diffing a file against the version in the repository.
  
  my ($file) = @_;
  
  my @args = ("-d", 
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "diff", 
              "-u", 
              $file);
  
  # Diff the file against the version in the repository, capturing the diff
  # and any error messages/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  # Check the error messages/notices in addition to the error code 
  # returned by the system call, because for some reason the call 
  # sometimes returns a non-zero error code (256) upon success.
  if ($error_code && $errors) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    DisplayError("I could not diff your version of the file against 
                  the version in the repository.<br>
                  <strong>Command:</strong> <em>$command</em><br>
                  <strong>Error Code:</strong> <em>$error_code</em><br> 
                  <strong>Error Message:</strong><b><pre>$errors</pre>");
    
    DeleteTempDir();
    exit;
  }
  
  if (!$output)
  {
    DisplayError("You didn't change anything!");
    DeleteTempDir();
    exit;
  }
  
  return $output;
}

sub CheckInFile
{
  # Checks a file into the repository.
  
  my ($file) = @_;
  
  my $username = $request->param('username');
  my $password = $request->param('password');
  
  my $comment = $request->param('comment');
  my @args = ("-d" , 
              ":pserver:$username:$password\@$WRITE_CVS_SERVER" , 
              "commit" , 
              "-m" , 
              $comment , 
              $file);
  
  # Check the file into the repository and capture the results.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$password\E/[password]/g;
    $errors =~ s/\Q$password\E/[password]/g;
    
    DisplayError("I could not check your version of the file
                  into the repository.<br>
                  <strong>Command:</strong> <em>$command</em><br>
                  <strong>Error Code:</strong> <em>$error_code</em><br> 
                  <strong>Error Message:</strong><br><pre>$errors</pre>");
    
    DeleteTempDir();
    exit;
  }
  
  return $output;
}

sub DisplayError 
{
  ($vars->{'message'}, $vars->{'title'}) = @_;
  
  chdir($HOME) 
    || die("Could not change back to original working directory 
            to deliver error message <strong>$vars->{'title'}</strong>: 
            <em>$vars->{'message'}</em>.");
  
  print $request->header;
  $template->process("error.tmpl", $vars)
    || die("<strong>Template Error</strong>: <em>" . $template->error() . "</em>
            while trying to deliver error message <strong>$vars->{'title'}</strong>: 
            <em>$vars->{'message'}</em>.");
}

sub CreateTempDir
{
  # Creates and changes to a temporary sub-directory unique to this process
  # that can be used for CVS activities requiring the checking out of files.
  
  if (!-e $TEMP_DIR and !mkdir($TEMP_DIR))
  {
    DisplayError("I could not create the temporary directory <em>$TEMP_DIR</em>: $!");
    exit;
  }
  elsif (!-d $TEMP_DIR)
  {
    DisplayError("I could not use the temporary directory <em>$TEMP_DIR</em> 
                  because it is not a directory.");
    exit;
  }
  
  # Create and change to a unique sub-directory in the temporary directory.
  mkdir("$TEMP_DIR/$PID") 
    || DisplayError("I could not create a temporary sub-directory <em>$TEMP_DIR/$PID</em>: $!") 
    && exit;
  
  chdir("$TEMP_DIR/$PID") 
    || DisplayError("I could not change to the temporary sub-directory <em>$TEMP_DIR/$PID</em>: $!") 
    && exit;
}

sub DeleteTempDir
{
  # Changes back to the home directory and deletes the temporary directory.
  
  chdir($HOME);
  rmtree("$TEMP_DIR/$PID", 0, 1)
    || DisplayError("I could not delete the temporary sub-directory <em>$TEMP_DIR/$PID</em>: $!") 
    && exit;
}
