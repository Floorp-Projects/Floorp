#!/usr/bin/perl -w
#
# This script is eval'ed by the fileViewer.cgi script.
#
# Input is a scalar, $FILE, that contains the file text
#
#
#################################################################
#
#  Add colour to perl code (html)
#
#################################################################

my $COMMENT_COLOR="red" ;
my $MACRO_COLOR="#006000" ;
my $STRING_CONSTANT_COLOR="green" ;

my $RESERVED_WORD_COLOR="blue" ;
my $VARIABLE_WORD_COLOR="#a000a0" ;

sub comment {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(.*?\n)//is ;
    $$outr .= "<font color=\"$COMMENT_COLOR\"><b>${token}</b>$1</font>" ;
}
sub string {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(.*?)(?<!\\)&quot;// ;
    $$outr .= "$token<font color=\"$STRING_CONSTANT_COLOR\">$1</font>&quot;" ;    
}
sub resword {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$outr .= "<font color=\"$RESERVED_WORD_COLOR\"><b>$token</b></font>" ;
}
sub variable {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $token =~ s/^(.)// ;
    $$outr .= "<font color=\"$VARIABLE_WORD_COLOR\"><b>$1</b>$token</font>" ;
}

				# "Reserved words"
my @reswrds = ("if",             "else",          "elsif",         "unless",
	       "while",          "foreach",       "until",         "do",
	       "chomp",          "abs",           "accept",        "alarm",
	       "atan2",          "bind",          "binmode",       "bless",
	       "caller",         "chdir",         "chmod",         "chomp",
	       "chop",           "chown",         "chr",           "chroot",
	       "close",          "closedir",      "connect",       "continue",
	       "cos",            "crypt",         "dbmclose",      "dbmopen",
	       "defined",        "delete",        "die",           "do",
	       "dump",           "each",          "endgrent",      "endhostent",
	       "endnetent",      "endprotoent",   "endpwent",      "endservent",
	       "eof",            "eval",          "exec",          "exists",
	       "exit",           "exp",           "fcntl",         "fileno",
	       "flock",          "fork",          "format",        "formline",
	       "getc",           "getgrent",      "getgrgid",      "getgrnam",
	       "gethostbyaddr",  "gethostbyname", "gethostent",    "getlogin",
	       "getnetbyaddr",   "getnetbyname",  "getnetent",     "getpeername",
	       "getpgrp",        "getppid",       "getpriority",   "getprotobyname",
	       "getprotobynumber","getprotoent",  "getpwent",      "getpwnam",
	       "getpwuid",       "getservbyname", "getservbyport", "getservent",
	       "getsockname",    "getsockopt",    "glob",          "gmtime",
	       "goto",           "grep",          "hex",           "import",
	       "index",          "int",           "ioctl",         "join",
	       "keys",           "kill",          "last",          "lc",
	       "lcfirst",        "length",        "link",          "listen",
	       "local",          "localtime",     "log",           "lstat",
	       "map",            "mkdir",         "msgctl",        "msgget",
	       "msgrcv",         "msgsnd",        "my",            "next",
	       "no",             "oct",           "open",          "opendir",
	       "ord",            "pack",          "package",       "pipe",
	       "pop",            "pos",           "print",         "printf",
	       "prototype",      "push",          "q",             "qq",
	       "qr\/",           "quotemeta",     "qw",            "qw\/",
	       "qx",             "qx\/",          "rand",          "read",
	       "readdir",        "readline",      "readlink",      "readpipe",
	       "recv",           "redo",          "ref",           "rename",
	       "require",        "reset",         "return",        "reverse",
	       "rewinddir",      "rindex",        "rmdir",         "s\/",
	       "scalar",         "seek",          "seekdir",       "select",
	       "semctl",         "semget",        "semop",         "send",
	       "setgrent",       "sethostent",    "setnetent",     "setpgrp",
	       "setpriority",    "setprotoent",   "setpwent",      "setservent",
	       "setsockopt",     "shift",         "shmctl",        "shmget",
	       "shmread",        "shmwrite",      "shutdown",      "sin",
	       "sleep",          "socket",        "socketpair",    "sort",
	       "splice",         "split",         "sprintf",       "sqrt",
	       "srand",          "stat",          "study",         "sub",
	       "substr",         "symlink",       "syscall",       "sysopen",
	       "sysread",        "sysseek",       "system",        "syswrite",
	       "tell",           "telldir",       "tie",           "tied",
	       "time",           "times",         "tr\/",          "truncate",
	       "uc",             "ucfirst",       "umask",         "undef",
	       "unlink",         "unpack",        "unshift",       "untie",
	       "use",            "utime",         "values",        "vec",
	       "wait",           "waitpid",       "wantarray",     "warn",
	       "write",   	     "y\/") ;	

my %routines ;
my @re ;
my $w ;
foreach $w (@reswrds) {
    $routines{$w} = \&resword ;
    push @re,'\b'.$w.'\b' ;
} 

# Comment
$routines{"\#"} = \&comment ;
push @re,"\#" ;

# String
$routines{"&quot;"} = \&string ;
push @re,"&quot;" ;

# Var
push @re,"[\$\@\%][\\w][\\w_-\\d]*" ;

my $in = $FILE ;
$FILE = "" ;
my $re = join("|",@re) ;

while($in =~ s/^(.*?)($re)//s) {
    $FILE .= $1 ;
    my $tok = $2 ;
    if(exists $routines{$tok}) {
	&{$routines{$tok}}($tok,\$in,\$FILE) ;
    }
    else {
	if($tok =~ /^[\$\@\%]/) {
	    &variable($tok,\$in,\$FILE) ;
	}
	else {
	    $FILE .= $tok ;
	}
    }
}
$FILE .= $in ;

#
# End
#

