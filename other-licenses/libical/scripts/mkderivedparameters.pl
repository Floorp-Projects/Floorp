#!/usr/bin/env perl

require "readvaluesfile.pl";

use Getopt::Std;
getopts('chspi:');

%no_xname = (RELATED=>1,RANGE=>1,RSVP=>1,XLICERRORTYPE=>1,XLICCOMPARETYPE=>1);

%params = read_parameters_file($ARGV[0]);


# Write the file inline by copying everything before a demarcation
# line, and putting the generated data after the demarcation

if ($opt_i) {

  open(IN,$opt_i) || die "Can't open input file $opt_i";

  while(<IN>){
    if (/<insert_code_here>/){
      insert_code();
    } else {
      print;
   }

  }    

  if($opt_p){
     print "# Everything below this line is machine generated. Do not edit. \n";
  } else {
    print "/* Everything below this line is machine generated. Do not edit. */\n";
  }

}

sub insert_code
{

# Write parameter string map
if ($opt_c){
}

# Write parameter enumerations and datatypes

if($opt_h){
  print "typedef enum icalparameter_kind {\n    ICAL_ANY_PARAMETER = 0,\n";
  foreach $param (sort keys %params) {
    
    next if !$param;
    
    next if $param eq 'NO' or $param eq 'ANY';

    my $uc = join("",map {uc($_);}  split(/-/,$param));

    my @enums = @{$params{$param}->{'enums'}};
        
    print "    ICAL_${uc}_PARAMETER, \n";
    
  }  
  print "    ICAL_NO_PARAMETER\n} icalparameter_kind;\n\n";

  # Now create enumerations for parameter values
  $idx = 20000;
  
  print "#define ICALPARAMETER_FIRST_ENUM $idx\n\n";
  
  foreach $param (sort keys %params) {
    
    next if !$param;
    
    next if $param eq 'NO' or $prop eq 'ANY';

    my $type = $params{$param}->{"C"};
    my $ucv = join("",map {uc(lc($_));}  split(/-/,$param));    
    my @enums = @{$params{$param}->{'enums'}};

    if(@enums){

      print "typedef enum $type {\n";
      my $first = 1;

      unshift(@enums,"X");

      push(@enums,"NONE");

      foreach $e (@enums) {
	if (!$first){
	  print ",\n";
	} else {
	  $first = 0;
	}
	
	my $uce = join("",map {uc(lc($_));}  split(/-/,$e));    
	
	print "    ICAL_${ucv}_${uce} = $idx";
	
	$idx++;
      }
      $c_type =~ s/enum //;

      print "\n} $type;\n\n";
    }
  }

  print "#define ICALPARAMETER_LAST_ENUM $idx\n\n";

}

if ($opt_c){

  # Create the icalparameter_value to icalvalue_kind conversion table
  print "struct  icalparameter_value_kind_map value_kind_map[] = {\n";
  
  foreach $enum (@{$params{'VALUE'}->{'enums'}}){
    next if $enum eq 'NO' or $enum eq 'ERROR';
    $uc = join("",map {uc(lc($_));}  split(/-/,$enum));    
    print "    {ICAL_VALUE_${uc},ICAL_${uc}_VALUE},\n";
  }
  
  print "    {ICAL_VALUE_X,ICAL_X_VALUE},\n";
  print "    {ICAL_VALUE_NONE,ICAL_NO_VALUE}\n};\n\n";
  
  #Create the parameter Name map
  print "static struct icalparameter_kind_map parameter_map[] = { \n";

  foreach $param (sort keys %params) {
    
    next if !$param;
    
    next if $param eq 'NO' or $prop eq 'ANY';

    my $lc = join("",map {lc($_);}  split(/-/,$param));    
    my $uc = join("",map {uc(lc($_));}  split(/-/,$param));    


    print "    {ICAL_${uc}_PARAMETER,\"$param\"},\n";

  }

  print "    { ICAL_NO_PARAMETER, \"\"}\n};\n\n";
  
  # Create the parameter value map

  print "static struct icalparameter_map icalparameter_map[] = {\n";
  print "{ICAL_ANY_PARAMETER,0,\"\"},\n";

  foreach $param (sort keys %params) {
    
    next if !$param;
    
    next if $param eq 'NO' or $prop eq 'ANY';

    my $type = $params{$param}->{"C"};
    my $uc = join("",map {uc(lc($_));}  split(/-/,$param));    
    my @enums = @{$params{$param}->{'enums'}};

    if(@enums){

      foreach $e (@enums){
	my $uce = join("",map {uc(lc($_));}  split(/-/,$e));    

	print "    {ICAL_${uc}_PARAMETER,ICAL_${uc}_${uce},\"$e\"},\n";
      }

    }
  }

  print "    {ICAL_NO_PARAMETER,0,\"\"}};\n\n";

}

foreach $param  (keys %params){

  my $type = $params{$param}->{'C'};

  my $ucf = join("",map {ucfirst(lc($_));}  split(/-/,$param));
  
  my $lc = lc($ucf);
  my $uc = uc($lc);
 
  my $charorenum;
  my $set_code;
  my $pointer_check;
  my $pointer_check_v;
  my $xrange;

  if ($type=~/char/ ) {

     $charorenum = "    icalerror_check_arg_rz( (param!=0), \"param\");\n    return ($type)((struct icalparameter_impl*)param)->string;";
    
     $set_code = "((struct icalparameter_impl*)param)->string = icalmemory_strdup(v);";

     $pointer_check = "icalerror_check_arg_rz( (v!=0),\"v\");"; 
     $pointer_check_v = "icalerror_check_arg_rv( (v!=0),\"v\");"; 

  } else {

    $xrange ="     if ( ((struct icalparameter_impl*)param)->string != 0){\n        return ICAL_${uc}_X;\n        }\n" if !exists $no_xname{$uc};
    
    $charorenum= "icalerror_check_arg( (param!=0), \"param\");\n$xrange\nreturn ($type)((struct icalparameter_impl*)param)->data;";
     
    $pointer_check = "icalerror_check_arg_rz(v >= ICAL_${uc}_X,\"v\");\n    icalerror_check_arg_rz(v < ICAL_${uc}_NONE,\"v\");";

    $pointer_check_v = "icalerror_check_arg_rv(v >= ICAL_${uc}_X,\"v\");\n    icalerror_check_arg_rv(v < ICAL_${uc}_NONE,\"v\");";

     $set_code = "((struct icalparameter_impl*)param)->data = (int)v;";

   }
  
  
  
  if ($opt_c) {
    
  print <<EOM;
/* $param */
icalparameter* icalparameter_new_${lc}($type v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   $pointer_check
   impl = icalparameter_new_impl(ICAL_${uc}_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_${lc}((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

${type} icalparameter_get_${lc}(icalparameter* param)
{
   icalerror_clear_errno();
$charorenum
}

void icalparameter_set_${lc}(icalparameter* param, ${type} v)
{
   $pointer_check_v
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   $set_code
}

EOM

  } elsif( $opt_h) {

  print <<EOM;
/* $param */
icalparameter* icalparameter_new_${lc}($type v);
${type} icalparameter_get_${lc}(icalparameter* value);
void icalparameter_set_${lc}(icalparameter* value, ${type} v);

EOM

}

if ($opt_p) {
    
  print <<EOM;

# $param 

package Net::ICal::Parameter::${ucf};
\@ISA=qw(Net::ICal::Parameter);

sub new
{
   my \$self = [];
   my \$package = shift;
   my \$value = shift;

   bless \$self, \$package;

   my \$p;

   if (\$value) {
      \$p = Net::ICal::icalparameter_new_from_string(\$Net::ICal::ICAL_${uc}_PARAMETER,\$value);
   } else {
      \$p = Net::ICal::icalparameter_new(\$Net::ICal::ICAL_${uc}_PARAMETER);
   }

   \$self->[0] = \$p;

   return \$self;
}

sub get
{
   my \$self = shift;
   my \$impl = \$self->_impl();

   return Net::ICal::icalparameter_as_ical_string(\$impl);

}

sub set
{
   # This is hard to implement, so I've punted for now. 
   die "Set is not implemented";
}

EOM

}

}

if ($opt_h){

print <<EOM;
#endif /*ICALPARAMETER_H*/

EOM
}

}
