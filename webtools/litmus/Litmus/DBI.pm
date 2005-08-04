package Litmus::DBI;

use strict;
use warnings;
use Litmus::Config;
use Litmus::Error;

use base 'Class::DBI::mysql';

my $dsn = "dbi:mysql:$Litmus::Config::db_name:$Litmus::Config::db_host";

Litmus::DBI->set_db('Main',
                         $dsn,
                         $Litmus::Config::db_user,
                         $Litmus::Config::db_pass);
                         
                                         
1;

