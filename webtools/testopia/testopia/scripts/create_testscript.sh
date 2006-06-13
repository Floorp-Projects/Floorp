. trenv.sh

plan_id="$1"
summary="$2"

url="http://$testrunserver/tr_scripts.cgi?action=runscript&plan_id=$plan_id&summary=$summary&$login_token"

wget "$url" -O testscript.sh -o create_testscript.log

chmod 700 testscript.sh