#! /usr/bin/perl
#! /usr/local/bin/perl
#! /usr/bin/env perl

$field_separator = "<<fs>>";

use CGI;

$query = new CGI;

open(OPTIONS, "m4 webify-configure.m4 configure.in|")
  or die "Error parsing configure.in\n";

print "Content-type: text/html\n\n<HTML>\n";

print qq(
<HEAD>
<TITLE>Configure Unix Mozilla build</TITLE>
</HEAD>
<body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
);

if ($query->param()) {
  foreach $param ($query->param()) {
    print "$param=".$query->param($param)."<BR>\n";
  }
} else {
  print qq(
	   <FORM action='setup.cgi' method='get'>
	   <table>);

  foreach (<OPTIONS>) {
    chomp;
    ($option_type, $option_name, $option_comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $option_comment, 3;
    $help =~ s/\\\$/\$/g;

    if ($option_type eq 'enable') {
      bool_option("--enable-$option_name", $help);
    }
    elsif ($option_type eq 'disable') {
      bool_option("--disable-$option_name", $help);
    } 
    elsif ($option_type eq 'enable_string') {
      string_option("--enable-$option_name", $help);
    }
    elsif ($option_type eq 'with_string') {
      string_option("--with-$option_name", $help);
    }

  }

  print qq(
	   </table>
	   <input type="Submit">
	   </form>
	  );
}

print "\n</body>\n</html>\n";

sub bool_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>";
  print "<INPUT type='checkbox' name='$name'>";
  print "</td><td>$name";
  print "</td><td>$help</td></tr>\n";
}

sub string_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>$name=</td><td align=left>";
  print "<INPUT type='text' name='$name'>";
  print "</td><td>$help</td></tr>\n";
}
