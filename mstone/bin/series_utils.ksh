#!/bin/ksh

# global configuration parameters.
# Fill in defaults for anything that is not already set

# Look for testname$test_form, first
export test_form=${test_form:-""}

# string appended to every description
export desc_conf=${desc_conf:-""}

# extra arguments common to all tests
export extra_args=${extra_args:-""}

# error limit to abort sequence
export error_limit=${error_limit:-100}

# set this to only show what it will do
export only_show_it=${only_show_it:-0}

# time to allow the server to calm down after each run
export sleep_time=${sleep_time:-5}

# This is where we store the important runs
export save_dir=${save_dir:-"results.save"}

# Basic sanity test
if [[ ! -x /usr/bin/perl || ! -f .license ]] ; then # see if setup was ever run
   echo "Critical files are missing.  Run setup."
   exit 2;
fi

find_timestamp () {		# find the timestamp string from latest run
  #OLD timestamp=`ls -d results/[0-9]*.[0-9][0-9][0-9][0-9]?(a-z) | tail -1`

  # list all directories with the timestamp pattern
  timestamp=`echo results/[0-9]*.[0-9][0-9][0-9][0-9]?([a-z])`
  # strip all but the last one
  timestamp=${timestamp##* }

  # strip the top directory name out
  timestamp=${timestamp#results/}

  # return it
  echo $timestamp
  return 0
}


# copy last mailstone run from the current directory to good results directory
save_run () {

  [[ -d $save_dir ]] || \
	mkdir $save_dir

  [[ $only_show_it -gt 0 ]] && return 0	# dont do anything important

  if [[ -n "$last_timestamp" && -d "results/$last_timestamp/" ]] ; then
	cp -pR results/$last_timestamp $save_dir/
	# index probably has lots of extra junk, but its better than nothing
	cp -pf results/index.html $save_dir/
  fi
}

# Display and run a command.  Skip if in only_show mode.
run () {
  if [[ $only_show_it -gt 0 ]] ; then
	echo  "Would run:" "$@"
	return 0
  fi
  echo "Running: " "$@"
  "$@"
}

# Sleep.  Skip if in only_show mode.
run_sleep () {
  if [[ $only_show_it -gt 0 ]] ; then
	echo  "Would sleep:" "$@"
	return 0
  fi
  echo "Sleeping: " "$@"
  sleep "$@"
}
# for readability, just use sleep
alias sleep=run_sleep


# This runs the actual mstone run and check for errors
# compress tmp files
# Usage: run_test testname description [args...]
run_test () {
  testname="$1"; shift;
  desc="$1"; shift;

  # see if a special version of this test exists
  if [[ -f conf/$testname$test_form.wld ]] ; then
	testname=$testname$test_form
  fi

  #oldtimestamp=`find_timestamp`

  if [[ $only_show_it -gt 0 ]] ; then
	echo  "Would run:" mstone $testname -b "$desc $desc_conf" $extra_args "$@"
	if [[ ! -f conf/$testname.wld ]] ; then
	  echo "Configuration Error: No such test $testname"
	fi
	return 0
  fi

  echo "\n##########################################################"
  if [[ ! -f conf/$testname.wld ]] ; then
    echo "CONFIGURATION ERROR: No such test $testname"
    exit 2
  fi

  echo  "\nRunning:" mstone $testname -b "$desc $desc_conf" $extra_args "$@"
  # We actually bypass the mstone script
  /usr/bin/perl -Ibin -- bin/mailmaster.pl -w conf/$testname.wld  -b "$desc $desc_conf" $extra_args "$@"
  stat=$?

  # BUG if another test is running at the same time, this is wrong
  timestamp="`find_timestamp`"

  # test failed to even run
  if [[ $stat -ne 0 ]]
  then
	echo "ABORT! Test failed to start"
        [[ -n "$mail_list" ]] && \
	    mail_series "DotCom Failed run: `date`" "$mail_list"
	exit 2
  fi

  # compress tmp files.  get the csv files, too.
  gzip tmp/$timestamp/* results/$timestamp/*.csv

  # stick the timestamp someplace global for a save_run
  export last_timestamp=$timestamp
  export all_timestamps="$all_timestamps $timestamp"

  # save the results
  save_run

  # see how many errors we hit
  totline=`grep 'Total:total ' results/$timestamp/results.txt`

  # strip label and first field
  errors=${totline##+([+-z])+( )+([+-9])+( )}
  # strip trailing fields
  errors=${errors%% *}

  echo ""			# space things out

  if [[ $errors -gt $error_limit ]] ; then
	echo "ABORT! Errors ($errors) exceed error limit ($error_limit)"
        [[ -n "$mail_list" ]] && \
	    mail_series "DotCom Aborted run: `date`" "$mail_list"
	exit 1
  fi
  echo "Run completed OK ($errors errors).  Timestamp $timestamp"

  sleep $sleep_time

  return 0
}

# Usage: mail_series subject "address,address,..."
mail_series () {
  subject=$1; shift
  file=/tmp/series$$.tar

  if [[ $only_show_it -gt 0 ]] ; then
	echo  "Would mail results about $subject" to "$@"
	return 0
  fi
  echo "Mailing results about $subject" to "$@"

  tar cf $file $save_dir/index.html
  for f in $all_timestamps ; do
      tar rf $file $save_dir/$f
  done
  gzip $file
  echo "$all_timestamps" | uuenview -b -30000 -s "$subject" -m "$@" $file.gz
  rm -f $file.gz
}

# parse command line arguments
while [[ -n "$1" ]]
do
  case $1 in
  # -n mode, do not execute, just show
  -n)	only_show_it=1; shift;;

  # set test form
  -f)	shift; test_form=$1; shift;;

  # set test extra description
  -d)	shift; desc_conf=$1; shift;;

  # Rest are to be passed in exactly
  --)	shift; break;;

  #default, pick up as an extra arg
  *)	extra_args="$extra_args $1"; shift;;
  esac
done
