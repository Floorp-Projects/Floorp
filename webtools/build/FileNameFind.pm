package FileNameFind;
$VERSION = 0.01;

require 5.000;

use strict;
use vars qw($VERSION);

#use vars qw(@ISA @EXPORT $VERSION);
#require Exporter;
#@ISA = qw(Exporter);
#@EXPORT = qw();

=head1 NAME

FileNameFind - Build an index of file names in a source tree.

=head1 SYNOPSIS

    use FileNameFind;
    $find = new FileNameFind;
    $root = '/u/slamm/tt/cvsroot/mozilla/';
    $tree = 'SeaMonkey';
    $find->update($root, $tree);

    $find = new FileNameFind;
    $find->open($tree);
    @dirs = $find->lookup('foo.c');
    $find->close;

=head1 DESCRIPTION

The index can be used to find the full path of a file based on its filename.
If a filename appears in a tree more than once, all the directories will
be listed for that filename.

=head1 FILES
    <tree>/file_dirs.db

=head1 AUTHOR

Stephen Lamm, slamm@netscape.com

=cut

$FileNameFind::Filename = 'file_dirs.db';

sub new {
  my $class = shift;
  my $self = {};
  bless($self, $class);
  return $self;
}

sub DESTROY {
  my $self = shift;
  $self->close;
}

sub update {
  my $self = shift;
  my ($root, $tree) = @_;
  use File::Find;

  $root =~ s/\/$//;

  $self->open($tree);

  &find(\&add_to_index,$root);

  sub add_to_index {
    $File::Find::prune = 1 if /^CVS$/;

    if (-T $_ and /\.(cpp|h|c|s),v$/) {
      my $filename = $_;

      $filename =~ s/,v$//;
      my $dir = "$File::Find::dir";
      $dir =~ s/^$root\/?//;
      $dir = '.' if $dir eq '';

      if (defined(my $file_entry = $self->{dbhash}->{$filename})) {

        my @list = split /:/, $file_entry;

        foreach my $dd (@list) {
          if ($dd eq $dir) {
            return;
        } }
        warn "Adding $filename = $dir\n";
        push @list, $dir;

        $self->{dbhash}->{$filename} = join(':',@list);
      } else {
        warn "New $filename = $dir\n";
        $self->{dbhash}->{$filename} = $dir;
} } } }

sub open {
  my $self = shift;
  my $tree = shift;
  my $filename = "$tree/$FileNameFind::Filename";
  my $hash_ref = {};

  use DB_File;

  $self->close;

  tie %{$hash_ref}, 'DB_File', $filename
    or die "Cannot open $filename: $!\n";

  $self->{dbhash} = $hash_ref;
}

sub close {
  my $self = shift;
  if (defined($self->{dbhash})) {
    untie %{$self->{dbhash}};
    delete $self->{dbhash};
  }
}

sub lookup {
  my $self = shift;
  my $filename = shift;
  my $candidate;

  my %args = ( @_ );

  my @dirs = split /:/, $self->{dbhash}->{$filename};

  if (defined($candidate = $args{candidate})) {
    for my $dir (@dirs) {
      return ($dir) if $dir == $candidate;
    }
  }
  return @dirs;
}

sub dump { 
  my $self = shift;

  my ($file, $dirlist);
  while (($file, $dirlist) = each %{ $self->{dbhash} } ) {
    for my $dir (split /:/, $dirlist) {
      print "$dir/$file\n";
} } }

1;
__END__
# Simple Module Test
use FileNameFind;
$find = new FileNameFind;

$tree = 'SeaMonkey';
if (1) {
  $cvsroot = '/u/slamm/tt/cvsroot/mozilla';
  $find->update($cvsroot, $tree);
} else {
  $find->open($tree);
  @dirs = $find->lookup('foo.c');
  print "There are ".($#dirs+1)." occurance(s) of foo.c\n";
  $find->dump;
}
