. trenv.sh

test_case_log_id="$1"

url="http://$testrunserver/tr_caselogform.cgi?action=change_status&case_action=pass&id=$test_case_log_id&$login_token"

wget "$url" -O passed_run_case_$test_case_log_id.html -o passed_run_case_$test_case_log_id.log