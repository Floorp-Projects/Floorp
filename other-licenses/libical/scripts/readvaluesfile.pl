

sub read_values_file {
  
  my $path = shift;
  my %h;

  open(F,$path) || die "Can't open values file $path";

  while(<F>){
    
    chop; 
 
    s/#.*$//g;
    s/\"//g;
   
    next if ! $_;

    @column = split(/,/,$_);
    
    my $value_name = $column[0];

    my $c_type_str =  $column[1];
    my $c_autogen = ($c_type_str =~ /\(a\)/);

    my $c_type = $c_type_str;
    $c_type =~ s/\(.\)//;

    my $python_type =  $column[2];
    my $components = $column[3];
    my $enum_values = $column[4];

    my @components;
    if($components ne "unitary"){
      @components = split(/;/,$components);
    } else {
      @components = ();
    }

    my @enums;
    if($enum_values) {
      @enums  = split(/;/,$enum_values);

    } else {
      @enums = ();
    }

    $h{$value_name} = { C => [$c_autogen,$c_type],
			perl => $perl_type,
			python => $python_type,
			components=>[@components],
			enums=>[@enums]
		      };
  }

  return %h;
}

sub read_properties_file {
  
  my $path = shift;
  my %h;

  open(F,$path) || die "Can't open properties file $path";

  while(<F>){
    
    chop; 
 
    s/#.*$//g;
    s/\"//g;
   
    next if ! $_;

    @column = split(/,/,$_);
    
    my $property_name = $column[0];

    my $lic_value = $column[1];
    my $default_value = $column[2];
    
    $h{$property_name} = { lic_value => $lic_value,
			   default_value => $default_value
			 };
  }

  return %h;
}

sub read_parameters_file {
  
  my $path = shift;
  my %h;

  open(F,$path) || die "Can't open parameters file $path";

  while(<F>){
    
    chop; 
 
    s/#.*$//g;
    s/\"//g;
   
    next if ! $_;

    @column = split(/\,/,$_);
  
    my $parameter_name = $column[0];

    my $data_type = $column[1];
    my $enum_string = $column[2];

    my @enums;
    if($enum_string){
      @enums =  split(/;/,$enum_string);
    }
    
    $h{$parameter_name} = { C => $data_type,
			   enums => [@enums]
			 };
  }

  close(F);

  return %h;
}



1;
