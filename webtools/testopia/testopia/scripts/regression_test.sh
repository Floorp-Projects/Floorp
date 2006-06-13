# Example regression test script
# It will call create_testscript.sh for a specified 
# plan_id with a generated summary and subsequently call the generated test 
# script. 

. acpm_utilities.sh
export PATH=$PATH:<PathToGeneralTestopiaScripts>
plan_id=<plan_id>
testrunname=<name_for_testrun>
create_testscript.sh $plan_id "$testrunname"
testscript.sh
