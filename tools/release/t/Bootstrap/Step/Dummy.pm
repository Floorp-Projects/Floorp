package t::Bootstrap::Step::Dummy;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');

    if (not $productTag) {
        print("testfailed, could not get productTag var from Config: $!\n");
    }

    eval {
        $this->Shell(cmd => 'echo', 
                     cmdArgs => ['success'],
                     logFile => 't/test.log' 
        );
    };
    if ($@) {
        print("testfailed, shell call to echo success to test log should not throw exception: $!\n");
    }

    eval {
        $this->CheckLog( 
            log => './t/test.log',
            checkForOnly => '^success',
        );
    };
  
    if ($@) {
        print("testfailed, should not throw exception, log contains only success: $!\n");
    }


    eval {
        $this->Shell( cmd => 'true', logFile => 't/test.log' );
    };

    if ($@) {
        print("testfailed, shell call to true should not throw exception: $!\n");
    }

    eval {
        $this->Shell( cmd => 'false', logFile => 't/test.log' );
    };

    if (not $@) {
        print("testfailed, shell call to false should throw exception: $!\n");
    }

    eval {
        $this->CheckLog( 
            log => './t/test.log',
            checkFor => '^success',
        );
    };
  
    if ($@) {
        print("testfailed, should throw exception, log contains success: $!\n");
    }

}

sub Verify {
    my $this = shift;
    $this->Shell(
      cmd => 'echo', cmdArgs => ['Verify', 'tag'], logFile => 't/test.log');
    $this->Log(msg => 'finished');
}

1;
