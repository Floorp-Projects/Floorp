This folder contains a set of scripts that Sytze <testrunner@asset-control.com>
uses to perform the administration of his automatic regression tests in Test 
Runner.

The scripts work on Test Runner 0.3. They need some customization to make them
work in your machine, Everybody is welcomed to continue improving this 
awesome functionality, including documentation and installation.

Many thanks to Sytze.

These are his notes:

*	GeneralScripts, the scripts that are used on the test machine: 

	*	create_testscript.sh ; this will call for the action added to 
	tr_scripts.cgi and store the created test script as "testscript.sh". 
	
	*	pass_id.sh; this will pass the specified run_case 
	
	*	fail_id.sh; this will fail the specified run_case 
	
	*	note_id.sh; this will put a note to the specified run_case 
	
	*	trenv.sh; this contains the name of the server, the username and 
	password for the user that runs the test.
	

*	RegressionTest; this contains an example regression test script: 

	*	regression_test.sh: It will call create_testscript.sh for a specified 
	plan_id with a generated summary and subsequently call the generated test 
	script. 
				
There are some things that should be added/changed: 

*	I did have wget available, but the version that we have installed does not 
support post. This implies that I can currently only put a single line as note. 
I have asked our system administrator to install curl, so that I can use a post 
method to store the log of a testscript as notes for the run case. 

*	Actually only those thest cases that are marked as "automated" should be 
put in the generated test script.

*	I currently have mutliple unix users on different machines that perform the 
regression tests. At the moment I create a new run for general user testopia, 
perhaps I should change that to have a single run with multiple users, but then 
I need to have an additional cgi-script to get the script for a specific user 
(need the run_cases) and I then also need to know the run_id for the plan. So 
this seems to be a better solution. In my summary I now generated the name of 
the installation.
