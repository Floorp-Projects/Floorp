package Bootstrap::Step::Tag;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo tag');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify tag');
}

1;
