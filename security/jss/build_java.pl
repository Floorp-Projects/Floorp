#use strict;
use File::Find;
use File::Compare;
use File::Basename;
use File::stat;
use File::Copy;

@excluded_sources = qw(
provider\.new/
org/mozilla/jss/provider/java/security/KeyFactorySpi1_4\.java
org/mozilla/jss/pkix/cert/X509Certificate\.java
samples/
);

@javah_classes = qw(
org.mozilla.jss.DatabaseCloser        
org.mozilla.jss.CryptoManager          
org.mozilla.jss.crypto.Algorithm        
org.mozilla.jss.crypto.EncryptionAlgorithm      
org.mozilla.jss.crypto.PQGParams     
org.mozilla.jss.crypto.SecretDecoderRing
org.mozilla.jss.pkcs11.CertProxy        
org.mozilla.jss.pkcs11.CipherContextProxy 
org.mozilla.jss.pkcs11.PK11Module 
org.mozilla.jss.pkcs11.ModuleProxy 
org.mozilla.jss.pkcs11.PK11Cert     
org.mozilla.jss.pkcs11.PK11Cipher     
org.mozilla.jss.pkcs11.PK11KeyWrapper  
org.mozilla.jss.pkcs11.PK11MessageDigest 
org.mozilla.jss.pkcs11.PK11PrivKey   
org.mozilla.jss.pkcs11.PK11PubKey     
org.mozilla.jss.pkcs11.PK11SymKey      
org.mozilla.jss.pkcs11.PK11KeyPairGenerator 
org.mozilla.jss.pkcs11.PK11KeyGenerator
org.mozilla.jss.pkcs11.PK11Token
org.mozilla.jss.pkcs11.PrivateKeyProxy  
org.mozilla.jss.pkcs11.PublicKeyProxy    
org.mozilla.jss.pkcs11.SymKeyProxy
org.mozilla.jss.pkcs11.KeyProxy    
org.mozilla.jss.pkcs11.PK11Token    
org.mozilla.jss.pkcs11.TokenProxy    
org.mozilla.jss.pkcs11.PK11Signature  
org.mozilla.jss.pkcs11.PK11Store       
org.mozilla.jss.pkcs11.PK11KeyPairGenerator 
org.mozilla.jss.pkcs11.SigContextProxy
org.mozilla.jss.pkcs11.PK11RSAPublicKey
org.mozilla.jss.pkcs11.PK11DSAPublicKey
org.mozilla.jss.pkcs11.PK11SecureRandom 
org.mozilla.jss.provider.java.security.JSSKeyStoreSpi
org.mozilla.jss.SecretDecoderRing.KeyManager
org.mozilla.jss.ssl.SSLSocket 
org.mozilla.jss.ssl.SSLServerSocket 
org.mozilla.jss.ssl.SocketBase 
org.mozilla.jss.util.Debug
org.mozilla.jss.util.Password       
);

@packages = qw(
org.mozilla.jss
org.mozilla.jss.asn1
org.mozilla.jss.crypto
org.mozilla.jss.pkcs7
org.mozilla.jss.pkcs10
org.mozilla.jss.pkcs11
org.mozilla.jss.pkcs12
org.mozilla.jss.pkix.primitive
org.mozilla.jss.pkix.cert
org.mozilla.jss.pkix.cmc
org.mozilla.jss.pkix.cmmf
org.mozilla.jss.pkix.cms
org.mozilla.jss.pkix.crmf
org.mozilla.jss.provider.java.security
org.mozilla.jss.provider.javax.crypto
org.mozilla.jss.SecretDecoderRing
org.mozilla.jss.ssl
org.mozilla.jss.tests
org.mozilla.jss.util
);


# setup variables
setup_vars(\@ARGV);

# run the command with its arguments
my $cmd = (shift || "build");   # first argument is command
grep { s/(.*)/"$1"/ } @ARGV;    # enclose remaining arguments in quotes
my $args = join(",",@ARGV);     # and comma-separate them
eval "$cmd($args)";             # now run the command
if( $@ ) {
    die $@;                     # errors in eval will be put in $@
}

# END

sub grab_cmdline_vars {
    my $argv = shift;

    while( $$argv[0] =~ /(.+)=(.*)/ ) {
        $cmdline_vars{$1} = $2;
        shift @$argv;
    }
}

sub dump_cmdline_vars {
    print "Command variables:\n";
    for(keys %cmdline_vars) {
        print "$_=" . $cmdline_vars{$_} . "\n";
    }
}

sub setup_vars {
    my $argv = shift;

    grab_cmdline_vars($argv);
    dump_cmdline_vars();

    $ENV{JAVA_HOME} or die "Must specify JAVA_HOME environment variable";
    $javac = "$ENV{JAVA_HOME}/bin/javac";
    $javah = "$ENV{JAVA_HOME}/bin/javah";
    $javadoc = "$ENV{JAVA_HOME}/bin/javadoc";

    $dist_dir = $cmdline_vars{SOURCE_PREFIX};
    $jce_jar = $ENV{JCE_JAR};

    $class_release_dir = $cmdline_vars{SOURCE_RELEASE_PREFIX};
    if( $ENV{BUILD_OPT} ) {
        $class_dir = "$dist_dir/classes";
        $class_release_dir .= "/$cmdline_vars{SOURCE_RELEASE_CLASSES_DIR}";
        $javac_opt_flag = "-O";
        $debug_source_file = "org/mozilla/jss/util/Debug_ship.jnot";
    } else {
        $class_dir = "$dist_dir/classes_DBG";
        $class_release_dir .= "/$cmdline_vars{SOURCE_RELEASE_CLASSES_DBG_DIR}";
        $javac_opt_flag = "-g";
        $debug_source_file = "org/mozilla/jss/util/Debug_debug.jnot";
    }
    $jni_header_dir = "$dist_dir/private/jss/_jni";

    if( $jce_jar ) {
        $classpath = "-classpath $jce_jar";
    }
}

sub clean {
    print_do("rm -rf $class_dir");
    print_do("rm -rf $jni_header_dir");
}

sub build {

    #
    # copy the appropriate debug file
    #
    my $debug_target_file = "org/mozilla/jss/util/Debug.java";
    if( compare($debug_source_file, $debug_target_file) ) {
        copy($debug_source_file, $debug_target_file) or die "Copying file: $!";
    }


    #
    # recursively find *.java
    #
    my %source_list;
    find sub {
        my $name = $File::Find::name;
        if( $name =~ /\.java$/) { 
            $source_list{$File::Find::name} = 1;
        }
    }, ".";

    #
    # weed out files that are excluded or don't need to be updated
    #
    my $file;
    foreach $file (keys %source_list) {
        my $pattern;
        foreach $pattern (@excluded_sources) {
            if( $file =~ /$pattern/ ) {
                delete $source_list{$file};
            }
        }
        unless( java_source_needs_update( $file, $class_dir ) ){
            delete $source_list{$file};
        }
    }
    my @source_list = keys(%source_list);

    #
    # build the java sources
    #
    if( scalar(@source_list) > 0 ) {
        ensure_dir_exists($class_dir);
        print_do("$javac $javac_opt_flag -sourcepath . -d $class_dir " .
            "$classpath " . join(" ",@source_list));
    }

    #
    # create the JNI header files
    #
    ensure_dir_exists($jni_header_dir);
    print_do("$javah -classpath $class_dir -d $jni_header_dir " .
        (join " ", @javah_classes) );
}

sub print_do {
    my $cmd = shift;
    print "$cmd\n";
    system($cmd);
    my $exit_status = $?>>8;
    $exit_status and die "Command failed ($exit_status)\n";
}

sub needs_update {
    my $target = shift;
    my @dependencies = @_;

    my $target_mtime = (stat($target))[9];
    my $dep;
    foreach $dep( @dependencies ) {
        my $dep_mtime = (stat($dep))[9];
        if( $dep_mtime > $target_mtime ) {
            return 1;
        }
    }
    return 0;
}

# A quick-and-dirty way to guess whether a .java file needs to be rebuilt.
# We merely look for a .class file of the same name. This won't work if
# the source file's directory is different from its package, and it
# doesn't know about nested or inner classes.
# source_file: the relative path to the source file ("org/mozilla/jss/...")
# dest_dir: the directory where classes are output ("../../dist/classes_DBG")
# Returns 1 if the source file is newer than the class file, or the class file
#   doesn't exist. Returns 0 if the class file is newer than the source file.
sub java_source_needs_update {
    my $source_file = shift;
    my $dest_dir = shift;

    my $class_dir = "$dest_dir/" . dirname($source_file);
    my $class_file = basename($source_file);
    $class_file =~ s/\.java/.class/;
    $class_file = $class_dir . "/" . $class_file;
    if( -f $class_file ) {
        my $class_stat = stat($class_file);
        my $source_stat = stat($source_file);

        if( $source_stat->mtime > $class_stat->mtime) {
            # class file exists and is out of date
            return 1;
        } else {
            #class file exists and is up to date
            return 0;
        }
    } else {
        # class file hasn't been generated yet.
        return 1;
    }
}

# Recursively makes the given directory. Dies at the first sign of trouble
sub ensure_dir_exists {
    my $dir = shift;
    my $parent = dirname($dir);
    if( $parent ne $dir ) {
        ensure_dir_exists($parent);
    }
    if( ! -d $dir ) {
        mkdir($dir, 0777) or die "Failed to mkdir $dir: $!";
    }
}

sub release {
    # copy all class files into release directory
    ensure_dir_exists("$class_release_dir");
    print_do("cp -r $class_dir/* $class_release_dir");
}

sub javadoc {
    my $html_header_opt;
    if( $ENV{HTML_HEADER} ) {
        $html_header_opt = "-header '$ENV{HTML_HEADER}'";
    }
    ensure_dir_exists("$dist_dir/jssdoc");
    my $targets = join(" ", @packages);
    print "$targets\n";
    print_do("$javadoc -breakiterator -sourcepath . -d $dist_dir/jssdoc $html_header_opt $targets");
    print_do("cp $dist_dir/jssdoc/index.html $dist_dir/jssdoc/index.html.bak");
    print_do("cp $dist_dir/jssdoc/overview-summary.html $dist_dir/jssdoc/index.html");
}
