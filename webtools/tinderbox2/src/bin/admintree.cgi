#!#perl# #perlflags# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# admintree.cgi - the webform used by administrators to close the
#		 tree, set the message of the day and stop build 
#		 columns from being shown on the default pages.


# $Revision: 1.25 $ 
# $Date: 2003/08/17 00:44:03 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bin/admintree.cgi,v $ 
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 



# Standard perl libraries
use File::Basename;
use CGI ':standard';

# Tinderbox libraries

use lib '#tinder_libdir#';

use TinderConfig;
use TreeData;
use FileStructure;
use Persistence;
use TinderHeader;
use Utils;
use HTMLPopUp;


# Normally we filter user input for 'tainting' and to prevent this:

# http://www.ciac.org/ciac/bulletins/k-021.shtml

#   When a victim with scripts enabled in their browser reads this
#   message, the malicious code may be executed
#   unexpectedly. Scripting tags that can be embedded in this way
#   include <SCRIPT>, <OBJECT>, <APPLET>, and <EMBED>.

#    note that we want some tags to be allowed (href) but not       #
#    others.  This requirement breaks the taint perl mechanisms for #
#    checking as we can not escape every '<>'.

# However we trust our administrators not to put bad things in the web
# pages and we wish to allow them to embed scripts.


sub get_params {

  $TREE = param("tree");
  (TreeData::tree_exists($TREE)) ||
    die("tree: $TREE does not exist\n");    

  # tree is safe, untaint it.
  $TREE =~ m/(.*)/;
  $TREE = $1;

  $MAILADDR = ( param("mailaddr") ||
                cookie(-name=>"tinderbox_mailaddr"));
  $MAILADDR = main::extract_user($MAILADDR);

  $NEW_MOTD = param("motd");
  $NEW_MOTD = extract_printable_chars($NEW_MOTD);

  $NEW_TREE_STATE = param("tree_state");
  $NEW_TREE_STATE = extract_printable_chars($NEW_TREE_STATE);

  $NEW_ADMIN = param("newadmin");
  $NEW_ADMIN = extract_printable_chars($NEW_ADMIN);

  $USE_COOKIE = param("use_cookie");
  ($USE_COOKIE) && 
    ($USE_COOKIE = 1);

  $PASSWD = ( param("passwd") ||
              cookie(-name=>"tinderbox_password_$TREE"));

  $NEW_PASSWD1 = param('newpasswd1');
  $NEW_PASSWD2 = param('newpasswd2');
  
  @NEW_IGNORE_BUILDS = param("ignore_builds");
  @NEW_IGNORE_BUILDS = uniq(@NEW_IGNORE_BUILDS);

  $REMOTE_HOST = remote_host();

  $ADMINISTRATIVE_NETWORK_PAT = ($TinderConfig::ADMINISTRATIVE_NETWORK_PAT ||
                                 '.*');

  return ;
}



sub setup_environment {

  $CURRENT_TREE_STATE = TinderHeader::gettree_header('TreeState', $TREE);
  
  @CURRENT_IGNORE_BUILDS = get_current_ignore_builds($TREE);

  $CURRENT_MOTD = TinderHeader::gettree_header('MOTD', $TREE);
  
  get_passwd_table();

  return ;
}




sub encrypt_passwd {
  my ($passwd) = @_;

  # The man page for Crypt says:

  #     KEY is the input string to encrypt, for instance, a user's
  #     typed password.  Only the first eight characters are used; the
  #     rest are ignored.

  my $salt = 'aa';
  my $encoded = crypt($passwd, $salt);

  return $encoded;  
}



sub get_passwd_table {

  my ($file) = FileStructure::get_filename($TREE, 'passwd');

  (-r $file) ||
   return ;
  
  my ($r) = Persistence::load_structure($file);
  $PASSWD_TABLE{$TREE} = $r;

  return ; 
}



# peek inside TinderDB::Build to get the names of the builds for this
# tree

sub get_build_names  {
  my ($tree) = @_;
  my (@build_names);

  # we need an eval since the builds may not be configured 

  eval {
    local $SIG{'__DIE__'} = sub { };

    use TinderDB::Build;

    my ($build_obj) = TinderDB::Build->new();
    
    $build_obj->loadtree_db($tree);
    
    @build_names = TinderDB::Build::all_build_names($tree);
  };

  return @build_names;
}



sub get_current_ignore_builds  {
  my ($tree) = @_;
  my (@ignore_builds) = ();

  @ignore_builds = split(
                         /,/, 
                         TinderHeader::gettree_header('IgnoreBuilds', $TREE)
                        );
  
  @ignore_builds = uniq(@ignore_builds);
  
  # It is possible that the current ignore_builds for this tree are
  # not a subset of the current settings of buildnames, since the
  # two are not maintained together.  Remove settings which no
  # longer apply.
  
  my (@build_names) = get_build_names($tree);

  my ($pat) = '';

  foreach $build_name (@build_names) {
      # careful some buildnames may have '()' or 
      # other metacharacters in them.

      $pat .= "\^". quotemeta($build_name)."\$".
          "\|";
  }
  chop $pat;

  @ignore_builds = grep(/$pat/, @ignore_builds);

  
  return @ignore_builds;
}




sub format_input_page {
  my ($tree)= @_;
  my (@build_names) = get_build_names($tree);
  my (@tree_states) = $TinderHeader::NAMES2OBJS{'TreeState'}->
      get_all_sorted_setable_tree_states();

  my ($title) = "Tinderbox Adminstration for Tree: $tree";
  my (@out);

  my $passwd_string = '';
  if ( !(keys %{ $PASSWD_TABLE{$TREE} }) ) {
    $passwd_string = h3(font({-color=>'red'}, 
                        "No administrators set, ".
                        "no passwords needed."));
  }

  push @out, (
              start_html(-title=>$title),
              h2({-align=>'CENTER'},
                 $title),
              start_form
             );

  push @out, (
              HTMLPopUp::Link( 
                              "linktxt"=>"Return to tree: $tree",
                              "href"=> FileStructure::get_filename($tree, 'tree_URL').
                              "/$FileStructure::DEFAULT_HTML_PAGE",
                             ).
              p());

  push @out, (

              # if we know mailaddr send it back as the default to
              # show we know the user, do not bother to send the
              # passwd back in the form.

              h3("Login",),
              $passwd_string,
              "Email address: ",
              textfield(-name=>'mailaddr',
                        -default=>$MAILADDR,
                        -size=>30,),
                        "Password: ".
              password_field(-name=>'passwd', -size=>8,),
              p(),
              checkbox( -label=>("If correct, ".
                                 "remember password and email field ".
                                 "as a cookie"),
                        -name=>'use_cookie'),
              p(),
             );
  
  push @out, ( 
              h3("Change Password",),
              "Enter the new password twice: <br>\n",
              "(Only the first eight characters are significant)<p>\n",
              p(),
              password_field(-name=>'newpasswd1', -size=>8,),
              password_field(-name=>'newpasswd2', -size=>8,),
              p(),
             );
  
  push @out, ( 
              h3("Add New Administrator",),
              "Enter the new administrators Email Address: <br>\n",
              "(default password will be same as administrators name)<p>\n",
              p(),
              textfield(-name=>'newadmin', -size=>30,),
              p(),
             );
  
  if (@tree_states) {
    
    push @out, ( 
                h3("Tree State"),
                "Change the State of the Tree:",
                p(),
                radio_group(-name=>'tree_state', 
                            -value=>[@tree_states], 
                            -default=>$CURRENT_TREE_STATE,),
                p(),
               )
  } # end if
  
  if (@build_names) {
    push @out, ( 
                h3("Unmonitored Builds"),
                "The set of Builds which will not normally be displayed: ",
                p(),
                checkbox_group(-name=>'ignore_builds', 
                               -value=>[@build_names], 
                               -default=>[@CURRENT_IGNORE_BUILDS],),
                p(),
               )
  } # end if
  
  push @out, (
              h3("Message of the Day"),
              "New Message of the Day (must be valid HTML)",p(),
              textarea(-name=>'motd', -default=>$CURRENT_MOTD,
                       -rows=>30, -cols=>75, -wrap=>'physical',),
              p(),
             );
  
  push @out, ( 
# the default does not work because the tree gets reset to null
#              defaults(-name=>'Defaults',),
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
    
  return @out;
}





sub save_passwd_table {
  my ($file) = FileStructure::get_filename($TREE, 'passwd');

  # We expect tree administration to be a rare event and each change
  # should be independent and noncontradictory, so we do not worry
  # about locking during the updates but we make an effort to keep the
  # updates atomic.

  if ( keys %{ $PASSWD_TABLE{$TREE} } ) {
    Persistence::save_structure($PASSWD_TABLE{$TREE},$file);
  }

  return ;
}



sub change_passwd {
  my (@results) = ();

  if (($NEW_PASSWD1) && 
       ($NEW_PASSWD1 ne $PASSWD ) ) {
    
    if ($NEW_PASSWD1 eq $NEW_PASSWD2) {

      # we need to reload the password table so that the check against
      # existing administrators is nearly atomic.

      get_passwd_table();

      my ($new_encoded1) = encrypt_passwd($NEW_PASSWD1);
      $PASSWD_TABLE{$TREE}{$MAILADDR} = $new_encoded1;
      save_passwd_table();

      push @results, "Password changed.\n";
    } else {
      push @results, ("New passwords do not match, ".
                      "password not changed.\n");
    }

  }

  return @results;  
}


sub add_new_administrator {
  my (@results) =();

  ($NEW_ADMIN) ||
    return ;

  ($NEW_ADMIN !~ m!\@!) &&
    return("New administrator: $NEW_ADMIN does not have an '\@' ".
           "in the email address.\n");

  # we need to reload the password table so that the check against
  # existing administrators is nearly atomic.

  get_passwd_table();

  ($PASSWD_TABLE{$TREE}{$NEW_ADMIN}) &&
    return("New administrator: $NEW_ADMIN already has an account.\n");
 
  
  $PASSWD_TABLE{$TREE}{$NEW_ADMIN} = encrypt_passwd($NEW_ADMIN);
  save_passwd_table();
  push @results, ("Added administrator: $NEW_ADMIN \n".
                  "(default passwd is same as admin name)\n");

  return @results;  
}





sub change_tree_state {
  my (@results) =();

  ($NEW_TREE_STATE) ||
    return ;

  ($NEW_TREE_STATE eq $CURRENT_TREE_STATE) &&
  return ;

  TinderHeader::savetree_header('TreeState', $TREE, $NEW_TREE_STATE);
  push @results, "Tree_State changed: $NEW_TREE_STATE\n";
  
  return @results;  
}



sub change_ignore_builds {
  my (@results) = ();
  my (@out);

  ("@NEW_IGNORE_BUILDS" eq "@CURRENT_IGNORE_BUILDS") &&
    return ;

  $ignore_builds = join(',', @NEW_IGNORE_BUILDS);
  TinderHeader::savetree_header('IgnoreBuilds', $TREE, $ignore_builds);

  push @results, "ignore_builds changed: @NEW_IGNORE_BUILDS \n";

  return @results;
}



sub change_motd {
  my (@results) = ();

  # remember new_motd could be empty.  As long as it is different than
  # old_motd we should save it.

  ($NEW_MOTD eq $CURRENT_MOTD) &&
    return ;

  TinderHeader::savetree_header('MOTD', $TREE, $NEW_MOTD);
  push @results, "MOTD changed: \n\t'\n$NEW_MOTD\n\t' \n";
    
  return @results;  
}



# return empty if there is no security problem otherwise return a list
# of strings explaining the problem

sub security_problem {
  my (@out) = ();

  ($REMOTE_HOST =~ m!$ADMINISTRATIVE_NETWORK_PAT!) ||
    (push @out, ("Error, Host: '$REMOTE_HOST' not valid. ".
                 " Requests must be made from an IP address".
                 " in an administrative network.\n"));

  # If they are not on a valid network they should not see what our
  # other security checks are.

  scalar(@out) &&
    return @out;

  ($MAILADDR) ||
    (push @out, "Error, No Mail Address\n");

  ($MAILADDR =~ m!\@!) ||
    (push @out, "Error, Mail Address must have '\@' in it.\n");

  if ( keys %{ $PASSWD_TABLE{$TREE}} ) {
    ($PASSWD) ||
      (push @out, "Error, must enter Password\n");
    
    my ($encoded) = encrypt_passwd($PASSWD);

    ($encoded eq $PASSWD_TABLE{$TREE}{$MAILADDR}) ||
      (push @out, "Error, Password Not Valid\n");
  }

  return @out;
}



# perform all the updates which have been requested.

sub make_all_changes {
  my (@results) = ();

  my $submit = param("Submit");

  if ($submit) {
    @results = security_problem();
    my ($security_problems) = scalar(@results);

    if (!($security_problems)) {
      push @results, change_passwd();
      push @results, add_new_administrator();
      push @results, change_tree_state();
      push @results, change_ignore_builds();
      push @results, change_motd();
      push @results, ("Check changes are correct on the status page, ".
                      "different administrators can change ".
                      "the settings at the same time.");

      HTMLPopUp::regenerate_HTML_pages();

    } else {
      push @results, "No changes attempted due to security issues.";
    }
    
    if ( ($USE_COOKIE)  && (!($security_problems)) ) {
    # this must be called before header()

      my ($cookie1, $cookie2);
      my ($expires) = 'Sun, 1-Mar-2020 00:00:00 GMT';
      my ($passwd) = param('passwd');
      my ($mailaddr) = param('mailaddr');
      
      $cookie1 = cookie( 
                        -name=>"tinderbox_password_$TREE",
                        -value=>$passwd,
                        -expires => $expires,
                        -path=>'/',
                       );
      $cookie2 = cookie( 
                        -name=>"tinderbox_mailaddr",
                        -value=>$mailaddr,
                        -expires => $expires,
                        -path=>'/',
                       );
      $SET_COOKIES = [$cookie1, $cookie2];
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
      log_warning(@results);
    }
  }
  
  return @results;
}




#       Main        
{
  set_static_vars();
  get_env();
  chk_security();
  
  get_params();

  setup_environment();

  my (@out) = make_all_changes();
  
  print header(-cookie=>$SET_COOKIES);

  push @out, format_input_page($TREE);

  print @out;

  print end_html();

  print "\n\n\n";

  exit 0;
}
