#!/usr/bin/env bash
set -euo pipefail
# Dependencies and exact provenance are documented in report.md/environment.json.
# No product resource is modified. Pass the fully extracted installer executable.
binary=${1:?usage: run-regression.sh /absolute/extracted/floorp /absolute/output-root}
out=${2:?output root required}
python_bin=${QA_MARIONETTE_PYTHON:-/workspace/qa-linux-22/C/web-tools/venv/bin/python}
video=${QA_VIDEO_FILE:-/workspace/qa-linux-22/C-extra/KAN-50/fixture/long.webm}
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
run_case() {
 local order=$1 close=$2 display=$3 port=$4 http_port=$5
 dbus-run-session -- "$python_bin" "$script_dir/product-regression.py" \
  --binary "$binary" --out "$out/$order-close-$close" \
  --display "$display" --port "$port" --http-port "$http_port" \
  --source-sha 9d042bc17be83c15d2ff3a25eb712f04c4d3eb45 \
  --video-file "$video" --order "$order" --close-first "$close"
}
# Two owned X servers/WM/private DBus sessions, then the other two.
run_case normal-first normal 217 28759 19059 > "$out/normal-first-close-normal-run.log" 2>&1 &
case_one=$!
run_case panel-first normal 218 28760 19060 > "$out/panel-first-close-normal-run.log" 2>&1 &
case_two=$!
first_status=0
wait "$case_one" || first_status=1
wait "$case_two" || first_status=1
if (( first_status )); then exit "$first_status"; fi
run_case normal-first panel 219 28761 19061 > "$out/normal-first-close-panel-run.log" 2>&1 &
case_three=$!
run_case panel-first panel 220 28762 19062 > "$out/panel-first-close-panel-run.log" 2>&1 &
case_four=$!
last_status=0
wait "$case_three" || last_status=1
wait "$case_four" || last_status=1
exit "$last_status"
