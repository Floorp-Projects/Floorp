# Name: custom-script-test
# Owner: Perftest Team
# Description: Runs a sample custom script test.
# Options: {"default": {"perfherder": true, "perfherder_metrics": [{ "name": "Registration", "unit": "ms" }]}} #noqa

echo Running...

# Binary will be the package name on android, and the path to the
# browser on desktop. It's already verified if it exists.
echo Binary: ${BROWSER_BINARY}

# perfMetrics must be enclosed in a single-quote string,
# with all open/closing curly-braces doubled
echo 'perfMetrics: [{{"name": "metric1", "shouldAlert": false, "lowerIsBetter": false, "unit": "speed", "values": [1, 2, 3, 4]}}]'
