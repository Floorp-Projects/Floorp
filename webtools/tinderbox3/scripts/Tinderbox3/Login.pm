package Tinderbox3::Login;

use strict;
use LWP::UserAgent;
use CGI;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(check_session can_admin can_edit_tree can_sheriff_tree login_fields);

sub login {
  my ($login, $password) = @_;

  my $p = new CGI({Bugzilla_login => $login, Bugzilla_password => $password, GoAheadAndLogIn => 1});
  my $url = "http://bugzilla.mozilla.org/query.cgi?" . $p->query_string;

  my $ua = new LWP::UserAgent;
  $ua->agent("TinderboxServer/0.1");
  my $req = new HTTP::Request(GET => $url);
  my $response = $ua->request($req);
  if ($response->is_success) {
    if (${$response->content_ref()} =~ /Log(\s|&nbsp;)*out/i &&
        ${$response->content_ref()} =~ /$login/) {
      return 1;
    }
  }

  return 0;
}

sub delete_session {
  my ($dbh, $session_id) = @_;
  $dbh->do("DELETE FROM tbox_session WHERE session_id = ?", undef, $session_id);
  $dbh->commit();
}

sub check_session {
  my ($p, $dbh) = @_;
  my $session_id = $p->cookie('tbox_session');
  my $login = $p->param('-login');
  my $password = $p->param('-password');

  my ($login_return, $cookie);
  if ($login) {
    if (login($login, $password)) {
      if ($session_id) {
        delete_session($dbh, $session_id);
      }
      my $new_session_id = time . "-" . int(rand*100000) . "-" . $login;
      $dbh->do("INSERT INTO tbox_session (login, session_id, activity_time) VALUES (?, ?, current_timestamp())", undef, $login, $new_session_id);
      $dbh->commit();
      $cookie = $p->cookie(-name => 'tbox_session', -value => $new_session_id);
      $login_return = $login;
    }
  } elsif ($p->param('-logout') && $session_id) {
    delete_session($dbh, $session_id);
  } elsif($session_id) {
    my $row = $dbh->selectrow_arrayref("SELECT login, EXTRACT(EPOCH FROM activity_time) FROM tbox_session WHERE session_id = ?", undef, $session_id);
    if (defined($row)) {
      if (time > $row->[1]+24*60*60) {
        delete_session($dbh, $session_id);
      } else {
        $dbh->do("UPDATE tbox_session SET activity_time = current_timestamp() WHERE session_id = ?", undef, $session_id);
        $dbh->commit();
        $login_return = $row->[0];
      }
    }
  }

  return ($login_return, $cookie);
}

sub can_admin {
  my ($login) = @_;
  if (grep { $_ eq $login } @Tinderbox3::Login::superusers) {
    return 1;
  } else {
    return 0;
  }
}

sub can_edit_tree {
  my ($login, $editors) = @_;
  if ((grep { $_ eq $login } split(/,/, $editors)) ||
      (grep { $_ eq $login } @Tinderbox3::Login::superusers)) {
    return 1;
  } else {
    return 0;
  }
}

sub can_sheriff_tree {
  my ($login, $editors, $sheriffs) = @_;
  if ((grep { $_ eq $login } split(/,/, $editors)) ||
      (grep { $_ eq $login } split(/,/, $sheriffs)) ||
      (grep { $_ eq $login } @Tinderbox3::Login::superusers)) {
    return 1;
  } else {
    return 0;
  }
}

sub login_fields {
  return "<strong>Login:</strong> <input type=text name='-login'> <strong>Password:</strong> <input type=password name='-password'>";
}

our @superusers = ('jkeiser@netscape.com');

1
