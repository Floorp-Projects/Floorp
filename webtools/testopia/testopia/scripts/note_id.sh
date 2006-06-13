. trenv.sh

test_case_log_id="$1"
notes="$2"

url="http://$testrunserver/tr_caselogform.cgi?action=submit_notes&id=$test_case_log_id&notes=$notes&$login_token"

wget "$url" -O noted_run_case_$test_case_log_id.html -o noted_run_case_$test_case_log_id.log