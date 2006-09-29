package Bootstrap::Step::Source;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo source');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify source');
}

1;
