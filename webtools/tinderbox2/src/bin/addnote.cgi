#!#perl# -T --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# $Revision: 1.3 $ 
# $Date: 2000/09/18 19:27:42 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bin/addnote.cgi,v $ 
# $Name:  $ 

# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 



# Standard perl libraries
use CGI ':standard';
use File::Basename;

# Tinderbox libraries

use lib '#tinder_libdir#';

use TinderConfig;
use FileStructure;
use TreeData;
use Persistence;
use HTMLPopUp;
use Utils;



sub get_params {

  $SIG{'__DIE__'} = \&fatal_error;

  $REMOTE_HOST = remote_host();
  $TREE = param("tree");
  (TreeData::tree_exists($TREE)) ||
    die("tree: $TREE does not exist\n");    

  # tree is safe, untaint it.
  $TREE =~ m/(.*)/;
  $TREE = $1;

  $MAILADDR = ( param("mailaddr") ||
                cookie(-name=>"tinderbox_mailaddr"));

  $MAILADDR = main::extract_user($MAILADDR);

  $USE_COOKIE = param("use_cookie");
  ($USE_COOKIE) &&
    ($USE_COOKIE = 1);

  $REMOTE_HOST = remote_host();

  $NOTE=param("note");

  # Remove any known "bad" tags.  Since any user can post notices we
  # have to prevent bad scripts from being posted.

  $NOTE = extract_html_chars($NOTE);

  return 1;
}




sub format_input_page {
  my ($tree) = @_;

  my (@out);

  my ($title) = "Add a Notice to tree: $tree";

  push @out, (
              start_html(-title=>$title),
              h2($title),
              start_form,
             );
  
  push @out, (
              HTMLPopUp::Link( 
                              "linktxt"=>"Return to tree: $tree",
                              "href"=> FileStructure::get_filename($tree, 'tree_URL').
                              "/$FileStructure::DEFAULT_HTML_PAGE",
                             ).
              p());

  push @out, (
	      "Email address: ",p(),
	      textfield(-name=>'mailaddr', 
                        -default=>$MAILADDR),
	      p(),
	      checkbox( -label=>"remember mail address as a cookie",
			-name=>"use_cookie"),
	      p(),
	     );
  
  push @out, (
	      "Enter Notice: \n",p(),
	      textarea(-name=>'note', 
		       -rows=>10, -cols=>30, -wrap=>'physical',),
	      p(),
	     );
  
  push @out, (
	      submit(-name=>'Submit'),
	      p(),
	     );

  # We need the post operation to remember all the parameters which
  # were passed as arguments as well as those passed as form
  # variables.
  
  foreach $param ( param() ) {
    push @out, hidden($param)."\n";
  }
  
  
  push @out, end_form;
  
  push @out, "\n\n\n";
  
  return @out;
}




sub save_note {
  my ($tree) = @_;

  my (@out);

  my ($time) = time();
  my ($localtime) = localtime($time);
  
  my ($pretty_time) = HTMLPopUp::timeHTML($time);

  # We embed the IP address of the host, just in case there is some bad
  # html in the notice that gets through our defenses.  If we know that
  # there is a problem with a page, then we know which machine it came
  # from.

  my ($rendered_notice) = (
                           "<!-- posted from remote host: $REMOTE_HOST -->\n".
                            "\t\t<p>\n".
                           "\t\t\t[<b>".
                           HTMLPopUp::Link(
                                           "linktxt"=>$MAILADDR,
                                           "href"=>"mailto:$MAILADDR",
                                          ).
                           " - $pretty_time".
                           "</b>]<\p>\n".
                           "\t\t\t<p>\n".
                           "$NOTE\n".
                           "\t\t</p>\n"
                          );
  my ($record) = {
                  'tree' => $TREE,
                  'mailaddr' => $MAILADDR,
                  'notice' => $NOTE,
                  'rendered_notice' => $rendered_notice, 
                  'time' => $time,
                  'localtime' => $localtime,
                  'remote_host' => $REMOTE_HOST,
                 };

  my ($update_file) = (FileStructure::get_filename($TREE, 'TinderDB_Dir').
                       "/Notice\.Update\.$time\.$MAILADDR"); 
  
  Persistence::save_structure( 
                             $record,
                             $update_file
                            );
  
  push @out, "posted notice: \n",p().
    pre($NOTE);

  return @out;
}


# save the note to disk and update the cookies

sub make_all_changes {
  my (@results) = ();

  my $submit = param("Submit");

  if ($submit) {
    push @results, save_note($TREE);
    
    if ($USE_COOKIE) {
    # this must be called before header()

      my ($cookie1,);
      my ($expires) = 'Sun, 1-Mar-2020 00:00:00 GMT';
      my ($mailaddr) = param('mailaddr');
      
      $cookie1 = cookie( 
                        -name=>"tinderbox_mailaddr",
                        -value=>$mailaddr,
                        -expires => $expires,
                        -path=>'/',
                       );
      $SET_COOKIES = [$cookie1,];
    }
    
    if (@results) {
      @results =  (
                   h2("Update Results"),
                   p()."\n",
                   join (p(), (
                               "Remote host: $REMOTE_HOST\n",
                               "Local Time: $LOCALTIME\n",
                               "Mail Address: $MAILADDR\n",
                               @results,
                              )
                        ),
                   "\n\n",
                  );
    }
  }
  
  return @results;
}




#       Main        
{
  set_static_vars();
  get_env();
  
  get_params();

  my (@out) = make_all_changes();
  
  print header(-cookie=>$SET_COOKIES);

  push @out, format_input_page($TREE);

  print @out;

  print end_html();

  print "\n\n\n";

  exit 0;
}
