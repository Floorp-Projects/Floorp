#!/usr/bin/perl -wI/home/ianh/lib/perl/
use strict;
use Template::Parser;
use File::Basename;

my @entries = ('output');
while (@entries) {
    my $file = pop @entries;
    if (-d $file) {
        # print "$file: recursing...\n";
        my $dir = $file;
        $dir =~ s/^output/output-compiled/os;
        mkdir($dir);
        push(@entries, getdir($file));
    } else {
        print "$file\n";
        process($file);
    }
}

sub getdir {
    my $dir = shift;
    local *DIR;
    unless (opendir(DIR, $dir)) {
        die "$dir: $!";
    }
    my @entries;
    while (my $entry = readdir(DIR)) {
        next if ($entry =~ m/^\.\.?$/os
                 || $entry eq 'CVS'
                 || $entry =~ m/[~\#]/os);
        push(@entries, "$dir/$entry");
    }
    closedir(DIR) or die "$dir: $!";
    return @entries;
}

sub process {
    my $file = shift;
    local *FILE;
    open(FILE, $file) or die "$file: $!";
    my $type = <FILE>;
    chomp $type;
    my @data = ();
    my $line;
    do {
        $line = <FILE>;
        die "file: $!" unless defined($line);
        chomp $line;
        push(@data, $line);
    } while ($line ne '');
    local $/ = undef;
    my $content = <FILE>;
    close(FILE) or die "$file: $!";
    if ($type eq 'TemplateToolkit') {
        $content = compile($content);
        $type = 'TemplateToolkitCompiled';
    }
    $file =~ s/^output/output-compiled/os;
    open(FILE, ">$file") or die "$file: $!";
    local $" = "\n";
    print FILE "$type\n@data\n$content" or die "$file: $!";
    close(FILE) or die "$file: $!";
}

sub compile {
    my $data = shift;

    my $parser = Template::Parser->new();
    my $content = $parser->parse($data);

    die $parser->error() unless $content;

    my ($block, $defblocks, $metadata) = @$content{ qw( BLOCK DEFBLOCKS METADATA ) };

    $defblocks = join('', 
		      map { "'$_' => $defblocks->{ $_ },\n" }
		      keys %$defblocks);
    
    $metadata = join('', 
                     map { 
                         my $x = $metadata->{ $_ }; 
                         $x =~ s/([\'\\])/\\$1/g;
                         "'$_' => '$x',\n";
                     } keys %$metadata);

    return "bless{${metadata}_HOT=>0,_BLOCK=>$block,_DEFBLOCKS=>{$defblocks}},'Template::Document'";

}

