. trenv.sh

test_case_log_id="$1"

url="http://$testrunserver/tr_caselogform.cgi?form_action=fail_bug_id&mark_failed=1&id=$test_case_log_id&$login_token"

wget "$url" -O failed_run_case_$test_case_log_id.html -o failed_run_case_$test_case_log_id.log