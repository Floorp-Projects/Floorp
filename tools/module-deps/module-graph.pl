#! perl -w

# Todo:
# - eliminate arcs implied by transitive dependancies
#   (i.e. in a -> b -> c; a->c;, eliminate a->c;
#   (discovered that "tred" will do this, but isn't super-helpful)
# - group together strongly-connected components, where strongly connected
#   means there exists a cycle, and all dependancies off the cycle.

if ($^O eq "linux") {
  $makecommand = "make";
} elsif ($^O eq "MSWin32") {
  $makecommand = "nmake /nologo /f makefile.win";
}

use Cwd;
$curdir = getcwd();
if (!@ARGV) {
  @dirs = (getcwd());
} else {
  @dirs = @ARGV;
  # XXX does them in reverse order..
  foreach $arg (@ARGV) {
    push @dirs, "$curdir/$arg";
  }
}

MFILE:
while ($#dirs != -1) {
  my ($current_dirs, $current_module, $current_requires);
  # pop the curdir
  $curdir = pop @dirs;

  chdir "$curdir";
  $current_dirs = "";
  open(MAKEOUT, "$makecommand echo-dirs echo-module echo-requires|") || die "Can't make: $!\n";

  $current_dirs = <MAKEOUT>; $current_dirs && chop $current_dirs;
  $current_module = <MAKEOUT>; $current_module && chop $current_module;
  $current_requires = <MAKEOUT>; $current_requires && chop $current_requires;
  close MAKEOUT;

  if ($current_module) {
    #
    # now keep a list of all dependancies of the module
    #
    my @require_list = split(/\s+/,$current_requires);
    foreach $req (@require_list) {
      $deps{$current_module}{$req}++;
    }
  }
  

  next if !$current_dirs;

  # now push all child directories onto the list
  @local_dirs = split(/\s+/,$current_dirs);
  for (@local_dirs) {
    push @dirs,"$curdir\\$_" if $_;
  }

}

print "digraph G {\n";
print "    concentrate=true;\n";

# figure out the internal nodes, and place them in a cluster

print "    subgraph cluster0 {\n";
print "        node [style=fill];\n";
print "        color=blue;\n";

foreach $module (sort { scalar keys %{$deps{$b}} <=> scalar keys %{$deps{$a}} } keys %deps) {
  print "        $module;\n";
}
print "    };\n";

# try to figure out "strongly connected components" by looking for cycles
# keep a list of all visited modules in %clustered
#foreach $module (keys %deps) {
#  next if ($clustered{$module});
#  
#}

foreach $module (sort { scalar keys %{$deps{$b}} <=> scalar keys %{$deps{$a}} } keys %deps) {
  foreach $req ( sort { $deps{$module}{$b} <=> $deps{$module}{$a} }
                 keys %{ $deps{$module} } ) {
#    print "    $module -> $req [weight=$deps{$module}{$req}];\n";
    print "    $module -> $req;\n";
  }
}

print "}";
