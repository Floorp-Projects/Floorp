#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:10:42 $ 
# $Author: mcafee%netscape.com $ 
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

# Normally we filter user input for 'tainting' and to prevent this:

# http://www.ciac.org/ciac/bulletins/k-021.shtml

#   When a victim with scripts enabled in their browser reads this
#   message, the malicious code may be executed
#   unexpectedly. Scripting tags that can be embedded in this way
#   include <SCRIPT>, <OBJECT>, <APPLET>, and <EMBED>.

#    note that since we want some tags to be allowed (href) but not #
#    others.  This requirement breaks the taint perl mechanisms for #
#    checking as we can not escape every '<>'.

# However we trust our administrators not do put bad things in the web
# pages and we wish to allow them to embed scripts.



sub get_params {

  $SIG{'__DIE__'} = \&fatal_error;

  $REMOTE_HOST = remote_host();
  $TREE = param("tree");
  (TreeData::tree_exists($TREE)) ||
    die("tree: $TREE does not exist\n");    

  $MAILADDR = ( param("mailaddr") ||
                cookie(-name=>"tinderbox_mailaddr"));
  $USE_COOKIE = param("use_cookie");

  $NOTE=param("note");

  return ;
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
  
  push @out, ("<A HREF=".
              FileStructure::get_filename($tree, 'tree_URL').
              "/index\.html>".
              "Return to tree: $tree</A>",
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

  # Remove any known "bad" tags.  Since any user can post notices we
  # have to prevent bad scripts from being posted.  It is just too
  # hard to follow proper security procedures ('tainting') and escape
  # everything then put back the good tags.

  $NOTE =~ s!</?(?:(?:SCRIPT)|(?:OBJECT)|(?:APPLET)|(?:EMBED))>!!ig;

  # remove unprintable characters, for safety.

  $NOTE =~ s![^a-zA-Z0-9\ \n\`\"\'\;\:\,\?\.\-\_\+\=\\\|\/\~\!\@\#\$\%\^\&\*\(\)\{\}\[\]\<\>]+!!g;

  my ($time) = time();
  my ($localtime) = localtime($time);
  
  my ($pretty_time) = HTMLPopUp::timeHTML($time);
  my ($rendered_notice) = (
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
                 };


  my ($tmp_file) = (FileStructure::get_filename($TREE, 'TinderDB_Dir').
                    "/Tmp.Notice\.Update\.$time\.$MAILADDR"); 
  my ($update_file) = (FileStructure::get_filename($TREE, 'TinderDB_Dir').
                       "/Notice\.Update\.$time\.$MAILADDR"); 
  
  main::mkdir_R(dirname($tmp_file));
  main::mkdir_R(dirname($update_file));
  
  Persistence::save_structure( 
                             [$record],
                             ['record'],
                             $tmp_file,
                            );
  
  # Rename is atomic on the same unix filesystem, so the server never
  # sees partially processed updates.
  
  rename ($tmp_file, $update_file) || 
    die("Could not rename: '$tmp_file' to '$update_file'. $!\n");

  push @out, "posted notice: \n",p().
    pre($NOTE);

  return @out;
}



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
