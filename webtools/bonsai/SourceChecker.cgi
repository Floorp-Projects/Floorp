#!/usr/bonsaitools/bin/perl -w
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

#  SourceChecker.cgi -- tools for creating or modifying the dictionary
#  used by cvsblame.cgi.
#
#  Created: Scott Collins <scc@netscape.com>, 4 Feb 1998.
#
#  Arguments (passes via GET or POST):
#               ...
#

use strict;

use CGI;
use SourceChecker;



        #
        # Global
        #

my $query = new CGI;



        #
        # Subroutines
        #


sub print_page_header()
        {
                print <<'END_OF_HEADER';
<H1>SourceChecker Dictionary Maintainance</H1>
END_OF_HEADER
        }


sub print_page_trailer()
        {
                print <<'END_OF_TRAILER';
<HR>
<FONT SIZE=-1>
Last updated 5 Feb 1998. 
<A HREF="SourceChecker.cgi">Dictionary maintainance and help</A>.</FONT>
Mail feedback to <A HREF="mailto:scc?subject=[SourceChecker.cgi]">&lt;scc@netscape.com&gt;</A>. 
END_OF_TRAILER
        }



my $error_header = '<HR><H2>I couldn\'t process your request...</H2>';

sub print_error($)
        {
                my $message = shift;
                print "$error_header<P><EM>Error</EM>: $message</P>";
                $error_header = '';
        }


sub print_query_building_form()
        {
                print $query->start_multipart_form;

                print '<HR><H2>Build a new request</H2>';
                print '<P>...to modify or create a remote dictionary with words from one or more local files.</P>';

                print '<H3>Files on the server</H3>';
                print '<P>...i.e., the dictionary to be created or modified.</P>';

                print $query->textfield( -name=>'dictionary',
                                      -default=>'',
                                     -override=>1,
                                         -size=>30 );
                print '-- the path to dictionary.';

                print '<H3>Files on your local machine</H3>';
                print '<P>...that will be uploaded to the server, so their contents can be added to the dictionary.</P>';

                print '<BR>';
                print $query->filefield( -name=>'ignore_english', -size=>30 );
                print '-- contains english (i.e., transformable) words to ignore.';

                print '<BR>';
                print $query->filefield( -name=>'ignore_strings', -size=>30 );
                print '-- contains identifiers (i.e., non-transformable) words to ignore.';

                print '<BR>';
                print $query->filefield( -name=>'flag_strings', -size=>30 );
                print '-- contains identifiers words to be flagged.';

                print '<BR>';
                print $query->filefield( -name=>'ignore_names', -size=>30 );
                print '-- contains user names to be ignored.';

                print '<BR>';
                print $query->submit;

                print $query->endform;
        }


sub do_add_good_words($)
        {
                my $file = shift;
                
                while ( <$file> )
                        {
                                next if /\#/;
                                add_good_words($_);
                        }
        }


sub do_add_bad_words($)
        {
                my $file = shift;
                
                while ( <$file> )
                        {
                                next if /\#/;
                                add_bad_words($_);
                        }
        }


sub do_add_good_english($)
        {
                my $file = shift;
                
                while ( <$file> )
                        {
                                next if /\#/;
                                add_good_english($_);
                        }
        }


sub do_add_names($)
        {
                my $file = shift;
                
                while ( <$file> )
                        {
                                next if /\#/;
                                add_names($_);
                        }
        }


sub handle_query()
        {
                my $dictionary_path = $query->param('dictionary');
                if ( ! $dictionary_path )
                        {
                                print_error('You didn\'t supply a path to the dictionary file.');
                                return;
                        }

                dbmopen %SourceChecker::token_dictionary, "$dictionary_path", 0666
                        || print_error("The dictionary you named could not be opened.");

                my $added_some_words = 0;

                my ($file_of_good_english, $file_of_good_words,
                    $file_of_bad_words, $file_of_names);
                if ( $file_of_good_english = $query->param('ignore_english') )
                        {
                                do_add_good_english($file_of_good_english);
                                $added_some_words = 1;
                        }

                if ( $file_of_good_words = $query->param('ignore_strings') )
                        {
                                do_add_good_words($file_of_good_words);
                                $added_some_words = 1;
                        }

                if ( $file_of_bad_words = $query->param('flag_strings') )
                        {
                                do_add_bad_words($file_of_bad_words);
                                $added_some_words = 1;
                        }

                if ( $file_of_names = $query->param('ignore_names') )
                        {
                                do_add_names($file_of_names);
                                $added_some_words = 1;
                        }

                if ( ! $added_some_words )
                        {
                                print_error("You did not supply any words to add to the dictionary.");
                        }

                dbmclose %SourceChecker::token_dictionary;
        }



        #
        # The main script
        #

print $query->header;
print $query->start_html(-title=>'SourceChecker Dictionary Maintainance',
                                                                                                -author=>'scc@netscape.com');

print_page_header();

if ( $query->param )
        {
                handle_query();
        }

print_query_building_form();
print_page_trailer();

print $query->end_html;

__DATA__

