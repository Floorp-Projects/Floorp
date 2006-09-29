package Bootstrap::Step::Stage;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo stage');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify stage');
}

1;
