# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from statistics import mean


class MissingResultsError(Exception):
    pass


def format_metrics(
    name,
    type,
    value,
    replicates=None,
    unit="ms",
    lower_is_better=True,
    alert_threshold=2,
    **kw
):
    res = {
        "alertThreshold": alert_threshold,
        "type": type,
        "unit": unit,
        "lowerIsBetter": lower_is_better,
        "name": name,
        "value": value,
    }
    res.update(kw)
    return res


def process(results, log, app="firefox", cold=True):
    """Takes a list of results and processes them.

    Assumes that the data being given is coming from browsertime. Each result in
    the list is treated as a new subtest in the suite. In other words, if you
    return 3 browsertime.json files, each of them will be its own subtest and
    they will not be combined together.

    :param results list: A list containing the data to process, each entry
        must be a single subtest. The entries are not combined.

    :return dict: A perfherder-formatted data blob.
    """
    allresults = []
    for c, result in enumerate(results):
        log("Results {}: parsing results from browsertime json".format(c))
        allresults.append(parse(result, log, app=app, cold=cold))

    # Create a subtest entry per result entry
    suites = []
    perfherder = {
        "suites": suites,
        "framework": {"name": "browsertime"},
        "application": {"name": app},
    }

    for res in allresults:
        res = res[0]
        measurements = res["measurements"]
        subtests = []

        values = [measurements[key][0] for key in measurements]
        suite = format_metrics(
            "btime-testing",
            "perftest-script",
            mean(values),
            extraOptions=[],
            subtests=subtests,
        )

        for measure in measurements:
            vals = measurements[measure]
            subtests.append(
                format_metrics(measure, "perftest-script", mean(vals), replicates=vals)
            )

        suites.append(suite)

    print(perfherder)
    return perfherder


def parse(results, log, app, cold):
    # bt to raptor names
    measure = ["fnbpaint", "fcp", "dcf", "loadtime"]
    conversion = (
        ("fnbpaint", "firstPaint"),
        ("fcp", "timeToContentfulPaint"),
        ("dcf", "timeToDomContentFlushed"),
        ("loadtime", "loadEventEnd"),
    )

    chrome_raptor_conversion = {
        "timeToContentfulPaint": ["paintTiming", "first-contentful-paint"]
    }

    def _get_raptor_val(mdict, mname, retval=False):
        if type(mname) != list:
            if mname in mdict:
                return mdict[mname]
            return retval
        target = mname[-1]
        tmpdict = mdict
        for name in mname[:-1]:
            tmpdict = tmpdict.get(name, {})
        if target in tmpdict:
            return tmpdict[target]

        return retval

    res = []

    # Do some preliminary results validation. When running cold page-load, the results will
    # be all in one entry already, as browsertime groups all cold page-load iterations in
    # one results entry with all replicates within. When running warm page-load, there will
    # be one results entry for every warm page-load iteration; with one single replicate
    # inside each.

    # XXX added this because it was not defined
    page_cycles = 1

    if cold:
        if len(results) == 0:
            raise MissingResultsError("Missing results for all cold browser cycles.")
    else:
        if len(results) != int(page_cycles):
            raise MissingResultsError("Missing results for at least 1 warm page-cycle.")

    # now parse out the values
    for raw_result in results:
        if not raw_result["browserScripts"]:
            raise MissingResultsError("Browsertime cycle produced no measurements.")

        if raw_result["browserScripts"][0].get("timings") is None:
            raise MissingResultsError("Browsertime cycle is missing all timings")

        # Desktop chrome doesn't have `browser` scripts data available for now
        bt_browser = raw_result["browserScripts"][0].get("browser", None)
        bt_ver = raw_result["info"]["browsertime"]["version"]
        bt_url = (raw_result["info"]["url"],)
        bt_result = {
            "bt_ver": bt_ver,
            "browser": bt_browser,
            "url": bt_url,
            "measurements": {},
            "statistics": {},
        }

        custom_types = raw_result["browserScripts"][0].get("custom")
        if custom_types:
            for custom_type in custom_types:
                bt_result["measurements"].update(
                    {k: [v] for k, v in custom_types[custom_type].items()}
                )
        else:
            # extracting values from browserScripts and statistics
            for bt, raptor in conversion:
                if measure is not None and bt not in measure:
                    continue
                # chrome we just measure fcp and loadtime; skip fnbpaint and dcf
                if app and "chrome" in app.lower() and bt in ("fnbpaint", "dcf"):
                    continue
                # fennec doesn't support 'fcp'
                if app and "fennec" in app.lower() and bt == "fcp":
                    continue

                # chrome currently uses different names (and locations) for some metrics
                if raptor in chrome_raptor_conversion and _get_raptor_val(
                    raw_result["browserScripts"][0]["timings"],
                    chrome_raptor_conversion[raptor],
                ):
                    raptor = chrome_raptor_conversion[raptor]

                # XXX looping several times in the list, could do better
                for cycle in raw_result["browserScripts"]:
                    if bt not in bt_result["measurements"]:
                        bt_result["measurements"][bt] = []
                    val = _get_raptor_val(cycle["timings"], raptor)
                    if not val:
                        raise MissingResultsError(
                            "Browsertime cycle missing {} measurement".format(raptor)
                        )
                    bt_result["measurements"][bt].append(val)

                # let's add the browsertime statistics; we'll use those for overall values
                # instead of calculating our own based on the replicates
                bt_result["statistics"][bt] = _get_raptor_val(
                    raw_result["statistics"]["timings"], raptor, retval={}
                )

        res.append(bt_result)

    return res
