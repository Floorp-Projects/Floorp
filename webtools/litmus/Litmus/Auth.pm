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
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::Auth;

use strict;

# IMPORTANT! 
## You _must_ call Litmus::Auth methods before sending a Content-type
## header so that any required cookies can be sent.

require Exporter;
use Litmus;
use Litmus::DB::User;
use Litmus::Error;
use Litmus::Template;
use Time::Piece;
use Time::Seconds;

use CGI;

our @ISA = qw(Exporter);
our @EXPORT = qw();

my $logincookiename = $Litmus::Config::user_cookiename;
my $cookie_expire_days = 7;

my $curSession;

# Given a username and password, validate the login. Returns the 
# Lutmus::DB::User object associated with the username if the login 
# is sucuessful. Returns false otherwise.
sub validate_login($$) {
  my $username = shift;
  my $password = shift;

  return 0 if (!$username or $username eq '' or 
               !$password or $password eq '');

  my ($userobj) = Litmus::DB::User->search(email => $username);

  if (!$userobj) { 
    return 0; 
  } 

  if (!$userobj->enabled() || $userobj->enabled() == 0) { 
    die "Account ".$userobj->username()." has been disabled by the administrator"; 
  }
	
  # for security reasons, we use the real (correct) crypt'd passwd
  # as the salt:
  my $realPasswordCrypted = $userobj->getRealPasswd();
  return 0 if (!$realPasswordCrypted or $realPasswordCrypted eq '');
  my $enteredPasswordCrypted = crypt($password, $realPasswordCrypted);
  if ($enteredPasswordCrypted eq $realPasswordCrypted) {
    return $userobj;
  } else {
    return 0;
  }
}

# Used by a CGI when a login is required to proceed beyond a certain point.
# requireLogin() will return a Litmus::User object to indicate that the user 
# is logged in, or it will redirect to a login page to allow the login to be 
# completed. Once the login is complete, the user will be redirected back to 
# $return_to and any parameters in the current CGI.pm object will be passed 
# to the new script. 
#
sub requireLogin {
  my $return_to = shift;
  my $admin_login_required = shift;
  my $cgi = Litmus->cgi();

  # see if we are already logged in:
  my $user = getCurrentUser();
  return $user if $user; # well, that was easy...

  my $vars = {
              title => "Login",
              return_to => $return_to,
              adminrequired => $admin_login_required,
              params => $cgi,
             };

  print $cgi->header();
  Litmus->template()->process("auth/login.html.tmpl", $vars) ||
    internalError(Litmus->template()->error());
  
  exit;	
}

# Used by a CGI in much the same way as requireLogin() when the user must
# be an admin to proceed.
sub requireAdmin {
  my $return_to = shift;
  my $cgi = Litmus->cgi();
  
  my $user = requireLogin($return_to, 1);
  if (!$user || !$user->is_admin()) { 
  	print $cgi->header();
    basicError("You must be a Litmus administrator to perform this function.");
  }
  return $user;
}

# Returns the current Litmus::DB::Session object corresponding to the current 
# logged-in user, or 0 if no valid session exists
sub getCurrentSession() {
	my $c = Litmus->cgi();
	
	# we're actually processing the login form right now, so the cookie hasn't
	# been sent yet...
	if ($curSession) { return $curSession } 
	
	my $sessionCookie = $c->cookie($logincookiename);
  	if (! $sessionCookie) {
    	return 0
    }
  
	my @sessions = Litmus::DB::Session->search(sessioncookie => $sessionCookie);
	my $session = $sessions[0];
	if (! $session) { return 0 }
  
	# see if it's still valid and that the user hasn't been disabled
	if (! $session->isValid()) { return 0 }
  
	return $session;
}

# Returns the Litmus::User object corresponding to the current logged-in
# user, or 0 if no valid login cookie exists
sub getCurrentUser() {
	my $session = getCurrentSession();
	
	if ($session) {
		return $session->user_id();
	} else {
		return 0;
	}
}


#
# ONLY NON-PUBLIC API BEYOND THIS POINT
#

# Processes data from a login form (auth/login.html.tmpl) using data 
# from the current Litmus::CGI object in use. When complete, returns the 
# flow of control to the script the user wanted to reach before the login,
# setting a login cookie for the user.
sub processLoginForm {
  my $c = Litmus->cgi();
  
  my $type = $c->param("login_type");
  
  if ($c->param("accountCreated")  && 
      $c->param("accountCreated") eq "true") {
    # make sure they really are logged in and didn't just set
    # the accountCreated flag:
    requireLogin("index.cgi");
    return; # we're done here
  }
  
  if ($type eq "litmus") {
    my $username = $c->param("email");
    my $password = $c->param("password");
    
    my $user = validate_login($username, $password);
    
    if (!$user) {
      loginError($c, "Username/Password incorrect. Please try again.");
    }
    
    my $session = makeSession($user);
    $c->storeCookie(makeCookie($session));
    
  } elsif ($type eq "newaccount") {
    my $email = $c->param("email");
    my $name = $c->param("realname");
    my $password = $c->param("password"); 
    my $nickname = $c->param("irc_nickname");
    
    # some basic form-field validation:
    my $emailregexp = q:^[\\w\\.\\+\\-=]+@[\\w\\.\\-]+\\.[\\w\\-]+$:;
    if (! $email || ! $email =~ /$emailregexp/) {
      loginError($c, "You must enter a valid email address");
    }
    if (! $password eq $c->param("password_confirm")) {
      loginError($c, "Passwords do not match. Please try again.");
    }

    my @users = Litmus::DB::User->search(email => $email);
    if ($users[0]) {
      loginError($c, "User ".$users[0]->email() ." already exists.");
    }
    if ($nickname and $nickname ne '') {
      @users = Litmus::DB::User->search(irc_nickname => $nickname);
      if ($users[0]) {
        loginError($c, "An account with that IRC nickname already exists.");
      }
    } else {
      $nickname = undef;
    }
    
    my $userobj = 
      Litmus::DB::User->create({email => $email, 
                                password => bz_crypt($password),
                                bugzilla_uid => 0,
                                realname => $name,
                                enabled => 1, 
                                is_admin => 0,
                                irc_nickname => $nickname
                               });
    
    my $session = makeSession($userobj);
    $c->storeCookie(makeCookie($session));
    
    my $vars = {
                title => "Account Created",
                email => $email,
                realname => $name,
                return_to => $c->param("login_loc"),
                params => $c
               };
    
    print $c->header();
    Litmus->template()->process("auth/accountcreated.html.tmpl", $vars) ||
      internalError(Litmus->template()->error());
    
    exit;	
    
  } elsif ($type eq "bugzilla") {
    my $username = $c->param("username");
    my $password = $c->param("password");
    
    
  } elsif ($type eq "convert") {
    # convert an old-school Litmus account (pre authentication system) 
    # to a new one with a password:
    my $username = $c->param("email");
    
    my @userobjs = Litmus::DB::User->search(email => $username);
    my $userobj = $userobjs[0];
    if (! $userobj || $userobj->email() ne $username) {
      loginError($c, "User $username does not exist.");
    }
    
    if ($userobj->password() ne "") {
      # already a new-style account
      loginError($c, "User $username has already been updated.");
    }
    
    # display a "convert login" form to get the rest of the information
    print $c->header();
    my $return_to = $c->param("login_loc") || "";
    unsetFields();
    my $vars = {
                title => "Update Login",
                email => $username,
                return_to => $return_to,
                params => $c,
               };
    
    Litmus->template()->process("auth/convertaccount.html.tmpl", $vars) ||
      internalError(Litmus->template()->error());
    exit;
  } elsif ($type eq "reallyconvert") {
    my $username = $c->param("email");
    my $realname = $c->param("realname");
    my $password = $c->param("password");
    my $password_confirm = $c->param("password_confirm");
    my $nickname = $c->param("irc_nickname");
    
    if (! $password eq $password_confirm) {
      loginError($c, "Passwords do not match. Please try again.");
    }
    
    my @userobjs = Litmus::DB::User->search(email => $username);
    my $userobj = $userobjs[0];
    
    # just to be safe:
    if (! $userobj || $userobj->email() ne $username) {
      loginError($c, "User $username does not exist.");
    }
    
    if ($userobj->password() ne "" && ($userobj->bugzilla_uid() == 0 || 
                                       !$userobj->bugzilla_uid())) {
      # already a new-style account
      loginError($c, "User $username has already been updated.");
    }
    my ($nickname_exists) = 
      Litmus::DB::User->search(irc_nickname => $nickname);
    if ($nickname_exists) {
      loginError($c,
                 "An account with that IRC nickname already exists.",
                 "auth/convertaccount.html.tmpl",
                 "Update Login");
    }
    
    # do the upgrade:
    $userobj->password(bz_crypt($password));
    $userobj->bugzilla_uid("0");
    $userobj->realname($realname);
    $userobj->enabled(1);
#    $userobj->is_admin(0);
    $userobj->irc_nickname($nickname);
    $userobj->update();
    
    my $session = makeSession($userobj);
    $c->storeCookie(makeCookie($session));

    my $vars = {
                title => "Account Created",
                email => $username,
                realname => $realname,
                return_to => $c->param("login_loc"),
                params => $c
               };
    
    print $c->header();
    Litmus->template()->process("auth/accountcreated.html.tmpl", $vars) ||
      internalError(Litmus->template()->error());
    
    exit;
    
  } else {
    internalError("Unknown login scheme attempted");
  }
}

sub changePassword {
	my $userobj = shift;
	my $password = shift;
	$userobj->password(bz_crypt($password));
	$userobj->update();
	
	my @sessions = $userobj->sessions();
	foreach my $session (@sessions) {
		$session->makeExpire();
	}
}

# Given a userobj, process the login and return a session object
sub makeSession {
  my $userobj = shift;
  my $c = Litmus->cgi();
  
  my $expires = localtime() + ONE_DAY * $cookie_expire_days;
  
  my $sessioncookie = randomToken(64);
  
  my $session = Litmus::DB::Session->create({
                                             user_id => $userobj, 
                                             sessioncookie => $sessioncookie,
                                             expires => $expires});
  
  $curSession = $session;
  return $session;
}

# Given a session, create a login cookie to go with it
sub makeCookie {
  my $session = shift;
  
  my $cookie = Litmus->cgi()->cookie( 
                                     -name    => $logincookiename,
                                     -value   => $session->sessioncookie(),
                                     -domain  => $main::ENV{"HTTP_HOST"},
                                     -expires => $session->expires(),
                                    );
  return $cookie;
}

sub loginError {
  my $c = shift;
  my $message = shift;
  my $template = shift;
  my $title = shift;

  if (!$template) {
    $template = "auth/login.html.tmpl";
  }
  if (!$title) {
    $title = "Login";
  }

  print $c->header();
  
  my $return_to = $c->param("login_loc") || "";
  my $email = $c->param("email") || "";
	
  unsetFields();
  
  my $vars = {
              title => $title,
              return_to => $return_to,
              email => $email,
              params => $c,
              onload => "toggleMessage('failure','$message')", 
            };

  Litmus->template()->process($template, $vars) ||
    internalError(Litmus->template()->error());
  
  exit;
}

sub unsetFields() {
  my $c = Litmus->cgi();
  
  # We need to unset some params in $c since otherwise we end up with 
  # a hidden form field set for "email" and friends and madness results:
  $c->param('email', '');
  $c->param('username', '');
  $c->param('login_type', '');
  $c->param('login_loc', '');
  $c->param('realname', '');
  $c->param('password', '');
  $c->param('password_confirm', '');
  
}

# Like crypt(), but with a random salt. Thanks to Bugzilla for this.
sub bz_crypt {
  my ($password) = @_;
  
  # The list of characters that can appear in a salt.  Salts and hashes
  # are both encoded as a sequence of characters from a set containing
  # 64 characters, each one of which represents 6 bits of the salt/hash.
  # The encoding is similar to BASE64, the difference being that the
  # BASE64 plus sign (+) is replaced with a forward slash (/).
  my @saltchars = (0..9, 'A'..'Z', 'a'..'z', '.', '/');
  
  # Generate the salt.  We use an 8 character (48 bit) salt for maximum
  # security on systems whose crypt uses MD5.  Systems with older
  # versions of crypt will just use the first two characters of the salt.
  my $salt = '';
  for ( my $i=0 ; $i < 8 ; ++$i ) {
    $salt .= $saltchars[rand(64)];
  }
  
  # Crypt the password.
  my $cryptedpassword = crypt($password, $salt);
  
  # Return the crypted password.
  return $cryptedpassword;
}

sub randomToken {
  my $size = shift || 10; # default to 10 chars if nothing specified
  return join("", map{ ('0'..'9','a'..'z','A'..'Z')[rand 62] } (1..$size));
}

# Deprecated:
# DO NOT USE
sub setCookie {
  my $user = shift;
  my $expires = shift;

  my $user_id = 0;
  if ($user) {
    $user_id = $user->userid();
  }
  
  if (!$expires or $expires eq '') {
    $expires = '+3d';
  }
    
  my $c = Litmus->cgi();
  
  my $cookie = $c->cookie( 
                          -name    => $logincookiename,
                          -value   => $user_id,
                          -domain  => $main::ENV{"HTTP_HOST"},
                          -expires => $expires,
                         );
  
  return $cookie;
}

# Deprecated:
sub getCookie() {
  return getCurrentUser();
}

sub istrusted($) {
  my $userobj = shift;

  return 0 if (!$userobj);
  
  if ($userobj->istrusted()) {
    return 1;
  } else {
    return 0;
  }
}

sub canEdit($) {
  my $userobj = shift;
  
  return $userobj->istrusted();
}

#########################################################################
# logout()
#
# Unset the user's cookie
#########################################################################
sub logout() {
  my $c = Litmus->cgi();

  my $cookie = $c->cookie(
                          -name => $logincookiename,
                          -value => '',
                          -domain  => $main::ENV{"HTTP_HOST"},
                          -expires => '-1d'
                         );
  $c->storeCookie($cookie);
  
  # invalidate the session behind the cookie as well:
  my $session = getCurrentSession();
  if ($session) { $session->makeExpire() } 
}


1;


