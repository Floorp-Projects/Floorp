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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Tobias Burnus <burnus@net-b.de>


package Bugzilla::Template;

use strict;

use Bugzilla::Config;
use Bugzilla::Util;

# for time2str - replace by TT Date plugin??
use Date::Format ();

use base qw(Template);

my $template_include_path;

# Make an ordered list out of a HTTP Accept-Language header see RFC 2616, 14.4
# We ignore '*' and <language-range>;q=0
# For languages with the same priority q the order remains unchanged.
sub sortAcceptLanguage {
    sub sortQvalue { $b->{'qvalue'} <=> $a->{'qvalue'} }
    my $accept_language = $_[0];

    # clean up string.
    $accept_language =~ s/[^A-Za-z;q=0-9\.\-,]//g;
    my @qlanguages;
    my @languages;
    foreach(split /,/, $accept_language) {
        if (m/([A-Za-z\-]+)(?:;q=(\d(?:\.\d+)))?/) {
            my $lang   = $1;
            my $qvalue = $2;
            $qvalue = 1 if not defined $qvalue;
            next if $qvalue == 0;
            $qvalue = 1 if $qvalue > 1;
            push(@qlanguages, {'qvalue' => $qvalue, 'language' => $lang});
        }
    }

    return map($_->{'language'}, (sort sortQvalue @qlanguages));
}

# Returns the path to the templates based on the Accept-Language
# settings of the user and of the available languages
# If no Accept-Language is present it uses the defined default
sub getTemplateIncludePath () {
    # Return cached value if available
    if ($template_include_path) {
        return $template_include_path;
    }
    my $languages = trim(Param('languages'));
    if (not ($languages =~ /,/)) {
        return $template_include_path =
               ["template/$languages/custom", "template/$languages/default"];
    }
    my @languages       = sortAcceptLanguage($languages);
    my @accept_language = sortAcceptLanguage($ENV{'HTTP_ACCEPT_LANGUAGE'} || "" );
    my @usedlanguages;
    foreach my $lang (@accept_language) {
        # Per RFC 1766 and RFC 2616 any language tag matches also its 
        # primary tag. That is 'en' (accept lanuage)  matches 'en-us',
        # 'en-uk' etc. but not the otherway round. (This is unfortunally
        # not very clearly stated in those RFC; see comment just over 14.5
        # in http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.4)
        if(my @found = grep /^$lang(-.+)?$/i, @languages) {
            push (@usedlanguages, @found);
        }
    }
    push(@usedlanguages, Param('defaultlanguage'));
    return $template_include_path =
        [map(("template/$_/custom", "template/$_/default"), @usedlanguages)];
}


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

###############################################################################

# Construct the Template object

# Note that all of the failure cases here can't use templateable errors,
# since we won't have a template to use...

sub create {
    my $class = shift;

    # IMPORTANT - If you make any configuration changes here, make sure to
    # make them in t/004.template.t and checksetup.pl.

    return $class->new({
        # Colon-separated list of directories containing templates.
        INCLUDE_PATH => [\&getTemplateIncludePath],

        # Remove white-space before template directives (PRE_CHOMP) and at the
        # beginning and end of templates and template blocks (TRIM) for better
        # looking, more compact content.  Use the plus sign at the beginning
        # of directives to maintain white space (i.e. [%+ DIRECTIVE %]).
        PRE_CHOMP => 1,
        TRIM => 1,

        COMPILE_DIR => 'data/',

        # Functions for processing text within templates in various ways.
        # IMPORTANT!  When adding a filter here that does not override a
        # built-in filter, please also add a stub filter to checksetup.pl
        # and t/004template.t.
        FILTERS => {
            # Render text in strike-through style.
            strike => sub { return "<strike>" . $_[0] . "</strike>" },

            # Returns the text with backslashes, single/double quotes,
            # and newlines/carriage returns escaped for use in JS strings.
            js => sub {
                my ($var) = @_;
                $var =~ s/([\\\'\"])/\\$1/g;
                $var =~ s/\n/\\n/g;
                $var =~ s/\r/\\r/g;
                return $var;
            },

            # HTML collapses newlines in element attributes to a single space,
            # so form elements which may have whitespace (ie comments) need
            # to be encoded using &#013;
            # See bugs 4928, 22983 and 32000 for more details
            html_linebreak => sub {
                my ($var) = @_;
                $var =~ s/\r\n/\&#013;/g;
                $var =~ s/\n\r/\&#013;/g;
                $var =~ s/\r/\&#013;/g;
                $var =~ s/\n/\&#013;/g;
                return $var;
            },

            xml => \&Bugzilla::Util::xml_quote ,

            # This filter escapes characters in a variable or value string for
            # use in a query string.  It escapes all characters NOT in the
            # regex set: [a-zA-Z0-9_\-.].  The 'uri' filter should be used for
            # a full URL that may have characters that need encoding.
            url_quote => \&Bugzilla::Util::url_quote ,

            quoteUrls => \&::quoteUrls ,

            bug_link => [ sub {
                              my ($context, $bug) = @_;
                              return sub {
                                  my $text = shift;
                                  return &::GetBugLink($text, $bug);
                              };
                          },
                          1
                        ],

            # In CSV, quotes are doubled, and any value containing a quote or a
            # comma is enclosed in quotes.
            csv => sub
            {
                my ($var) = @_;
                $var =~ s/\"/\"\"/g;
                if ($var !~ /^-?(\d+\.)?\d*$/) {
                    $var = "\"$var\"";
                }
                return $var;
            } ,

            # Format a time for display (more info in Bugzilla::Util)
            time => \&Bugzilla::Util::format_time,
        },

        PLUGIN_BASE => 'Bugzilla::Template::Plugin',

        # Default variables for all templates
        VARIABLES => {
            # Function for retrieving global parameters.
            'Param' => \&Bugzilla::Config::Param,

            # Function to create date strings
            'time2str' => \&Date::Format::time2str,

            # Function for processing global parameters that contain references
            # to other global parameters.
            'PerformSubsts' => \&::PerformSubsts ,

            # Generic linear search function
            'lsearch' => \&Bugzilla::Util::lsearch,

            # UserInGroup - you probably want to cache this
            'UserInGroup' => \&::UserInGroup,

            # SendBugMail - sends mail about a bug, using Bugzilla::BugMail.pm
            'SendBugMail' => sub {
                my ($id, $mailrecipients) = (@_);
                require Bugzilla::BugMail;
                Bugzilla::BugMail::Send($id, $mailrecipients);
            },
 
            # SyncAnyPendingShadowChanges
            # - called in the footer to sync the shadowdb
            'SyncAnyPendingShadowChanges' => \&::SyncAnyPendingShadowChanges,

            # Bugzilla version
            # This could be made a ref, or even a CONSTANT with TT2.08
            'VERSION' => $Bugzilla::Config::VERSION ,
        },

   }) || die("Template creation failed: " . $class->error());
}

1;

__END__

=head1 NAME

Bugzilla::Template - Wrapper arround the Template Toolkit C<Template> object

=head1 SYNOPSYS

  my $template = Bugzilla::Template->create;

=head1 DESCRIPTION

This is basically a wrapper so that the correct arguments get passed into
the C<Template> constructor.

It should not be used directly by scripts or modules - instead, use
C<Bugzilla-E<gt>instance-E<gt>template> to get an already created module.

=head1 SEE ALSO

L<Bugzilla>, L<Template>

