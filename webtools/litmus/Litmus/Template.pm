# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is the Bugzilla Bug Tracking System.
 #
 # The Initial Developer of the Original Code is Netscape Communications
 # Corporation. Portions created by Netscape are
 # Copyright (C) 1998 Netscape Communications Corporation. All
 # Rights Reserved.
 #
 # Contributor(s): Terry Weissman <terry@mozilla.org>
 #                 Dan Mosedale <dmose@mozilla.org>
 #                 Jacob Steenhagen <jake@bugzilla.org>
 #                 Bradley Baetz <bbaetz@student.usyd.edu.au>
 #                 Christopher Aillon <christopher@aillon.com>
 #                 Tobias Burnus <burnus@net-b.de>
 #                 Myk Melez <myk@mozilla.org>
 #                 Max Kanat-Alexander <mkanat@bugzilla.org>
 #                 Zach Lipton <zach@zachlipton.com>
 #                 Chris Cooper <ccooper@deadsquid.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

# This is mostly a placeholder. At some point in the future, we might 
# want to be more like Bugzilla and support multiple languages and 
# other fine features, so we keep this around now so that adding new 
# things later won't require changing every source file. 

package Litmus::Template;

use strict;

use Litmus::Config;
use HTML::StripScripts::Parser;

use base qw(Template);

my $template_include_path;

# Returns the path to the templates based on the Accept-Language
# settings of the user and of the available languages
# If no Accept-Language is present it uses the defined default
sub getTemplateIncludePath () {
    return "templates/en/default";
}

# Constants:
my %constants;
$constants{litmus_version} = $Litmus::Config::version;

# html tag stripper:
my $strip = HTML::StripScripts::Parser->new({
                            Context => 'Inline', 
                            AllowHref => 1,
                            BanAllBut => ['a', 'b', 'br', 'em', 'p', 'i', 'hr'],
                            strict_names => 1,
                            });

###############################################################################
# Templatization Code

# Use the Toolkit Template's Stash module to add utility pseudo-methods
# to template variables.
use Template::Stash;

# Add "contains***" methods to list variables that search for one or more 
# items in a list and return boolean values representing whether or not 
# one/all/any item(s) were found.
$Template::Stash::LIST_OPS->{ contains } =
  sub {
      my ($list, $item) = @_;
      return grep($_ eq $item, @$list);
  };

$Template::Stash::LIST_OPS->{ containsany } =
  sub {
      my ($list, $items) = @_;
      foreach my $item (@$items) { 
          return 1 if grep($_ eq $item, @$list);
      }
      return 0;
  };

# Allow us to still get the scalar if we use the list operation ".0" on it
$Template::Stash::SCALAR_OPS->{ 0 } = 
  sub {
      return $_[0];
  };

# Add a "substr" method to the Template Toolkit's "scalar" object
# that returns a substring of a string.
$Template::Stash::SCALAR_OPS->{ substr } = 
  sub {
      my ($scalar, $offset, $length) = @_;
      return substr($scalar, $offset, $length);
  };

# Add a "truncate" method to the Template Toolkit's "scalar" object
# that truncates a string to a certain length.
$Template::Stash::SCALAR_OPS->{ truncate } = 
  sub {
      my ($string, $length, $ellipsis) = @_;
      $ellipsis ||= "";
      
      return $string if !$length || length($string) <= $length;
      
      my $strlen = $length - length($ellipsis);
      my $newstr = substr($string, 0, $strlen) . $ellipsis;
      return $newstr;
  };

# Create the template object that processes templates and specify
# configuration parameters that apply to all templates.
sub create {
    my $class = shift;
    return $class->new({
        INCLUDE_PATH => &getTemplateIncludePath,
        CONSTANTS => \%constants,
        PRE_PROCESS => "variables.none.tmpl",
        POST_CHOMP => 1,
        
        COMPILE_DIR => $Litmus::Config::datadir,
        
        FILTERS => {
            # disallow all html in testcase data except for non-evil tags
            testdata => sub {
                my ($data) = @_;
                
                $strip->parse($data);
                $strip->eof();
                
                return $strip->filtered_document();
            }, 
            
            # Returns the text with backslashes, single/double quotes,
            # and newlines/carriage returns escaped for use in JS strings.
            # thanks to bugzilla!
            js => sub {
                my ($var) = @_;
                $var =~ s/([\\\'\"\/])/\\$1/g;
                $var =~ s/\n/\\n/g;
                $var =~ s/\r/\\r/g;
                $var =~ s/\@/\\x40/g; # anti-spam for email addresses
                return $var;
            },
            
            # anti-spam filtering of email addresses
            email => sub {
                my ($var) = @_;
                $var =~ s/\@/\&#64;/g;
                return $var;    
            }
        },
    });
}
