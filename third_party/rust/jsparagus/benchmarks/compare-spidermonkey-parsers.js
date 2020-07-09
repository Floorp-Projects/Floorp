// This script runs multipe parsers from a single engine.
"use strict";

// Directory where to find the list of JavaScript sources to be used for
// benchmarking.
var dir = ".";
if (scriptArgs && scriptArgs[0]) {
    // If an argument is given on the command line, assume this is the path to
    // one of real-js-samples directory containing thousabds of JS files,
    // otherwise this is assumed to be the current working directory.
    dir = scriptArgs[0]; //  "~/real-js-samples/20190416"
}

// Execution mode of the parser, either "script" or "module".
var mode = "script";

// Number of times each JavaScript source is used for benchmarking.
var runs_per_script = 10;

// First parser
var name_1 = "SpiderMonkey parser";
function parse_1(path) {
    var start = performance.now();
    parse(path, { module: mode == "module", smoosh: false });
    return performance.now() - start;
}

// Second parser
var name_2 = "SmooshMonkey parser";
function parse_2(path) {
    var start = performance.now();
    parse(path, { module: mode == "module", smoosh: true });
    return performance.now() - start;
}

// For a given `parse` function, execute it with the content of each file in
// `dir`. This process is repeated `N` times and the results are added to the
// `result` argument using the `prefix` key for the filenames.
function for_all_files(parse, N = 1, prefix = "", result = {}) {
    var path = "", content = "";
    var t = 0;
    var list = os.file.listDir(dir);
    for (var file of list) {
        try {
            path = os.path.join(dir, file);
            content = os.file.readRelativeToScript(path);
            try {
                t = 0;
                for (var n = 0; n < N; n++)
                    t += parse(content);
                result[prefix + path] = { time: t / N, bytes: content.length };
            } catch (e) {
                // ignore all errors for now.
                result[prefix + path] = { time: null, bytes: content.length };
            }
        } catch (e) {
            // ignore all read errors.
        }
    }
    return result;
}

// Compare the results of 2 parser runs and compute the speed ratio between the
// 2 parsers. Results from both parsers are assuming to be comparing the same
// things if they have the same property name.
//
// The aggregated results is returned as an object, which reports the total time
// for each parser, the quantity of bytes parsed and skipped and an array of
// speed ratios for each file tested.
function compare(res1, res2) {
    var result = {
        time1: 0,
        time2: 0,
        parsed_files: 0,
        parsed_bytes: 0,
        skipped_files: 0,
        skipped_bytes: 0,
        ratios_2over1: [],
    };
    for (var path of Object.keys(res1)) {
        if (!(path in res1 && path in res2)) {
            continue;
        }
        var p1 = res1[path];
        var p2 = res2[path];
        if (p1.time !== null && p2.time !== null) {
            result.time1 += p1.time;
            result.time2 += p2.time;
            result.parsed_files += 1;
            result.parsed_bytes += p1.bytes;
            result.ratios_2over1.push(p2.time / p1.time);
        } else {
            result.skipped_files += 1;
            result.skipped_bytes += p1.bytes;
        }
    }
    return result;
}

// Given a `table` of speed ratios, display a distribution chart of speed
// ratios. This is useful to check if the data is noisy, bimodal, and to easily
// eye-ball characteristics of the distribution.
function spread(table, min, max, step) {
    // var chars = ["\xa0", "\u2591", "\u2592", "\u2593", "\u2588"];
    var chars = ["\xa0", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"];
    var s = ["\xa0", "\xa0", "" + min, "\xa0", "\xa0"];
    var ending = ["\xa0", "\xa0", "" + max, "\xa0", "\xa0"];
    var ranges = [];
    var vmax = table.length / 10;
    for (var i = min; i < max; i += step) {
        ranges.push(0);
    }
    for (var x of table) {
        if (x < min || max < x) continue;
        var idx = ((x - min) / step)|0;
        ranges[idx] += 1;
    }
    var max_index = chars.length * s.length;
    var ratio = max_index / vmax;
    for (i = 0; i < s.length; i++)
        s[i] += "\xa0\u2595";
    for (var v of ranges) {
        var d = Math.min((v * ratio)|0, max_index - 1);
        var offset = max_index;
        for (i = 0; i < s.length; i++) {
            offset -= chars.length;
            var c = Math.max(0, Math.min(d - offset, chars.length - 1));
            s[i] += chars[c];
        }
    }
    for (i = 0; i < s.length; i++)
        s[i] += "\u258f\xa0" + ending[i];
    var res = "";
    for (i = 0; i < s.length; i++)
        res += "\n" + s[i];
    return res;
}

// NOTE: We have multiple strategies depending whether we want to check the
// throughput of the parser assuming the parser is cold/hot in memory, the data is
// cold/hot in the cache, and the adaptive CPU throttle is low/high.
//
// Ideally we should be comparing comparable things, but due to the adaptive
// behavior of CPU and Disk, we can only approximate it while keeping results
// comparable to what users might see.

// Compare Hot-parsers on cold data.
function strategy_1() {
    var res1 = for_all_files(parse_1, runs_per_script);
    var res2 = for_all_files(parse_2, runs_per_script);
    var result = compare(res1, res2);
    print(name_1, "\t", result.time1, "ms\t", 1e6 * result.time1 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time1), 'bytes/ns\t');
    print(name_2, "\t", result.time2, "ms\t", 1e6 * result.time2 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time2), 'bytes/ns\t');
    print("Total parsed  (scripts:", result.parsed_files, ", bytes:", result.parsed_bytes, ")");
    print("Total skipped (scripts:", result.skipped_files, ", bytes:", result.skipped_bytes, ")");
    print(name_2, "/", name_1, ":", result.time2 / result.time1);
    print(name_2, "/", name_1, ":", spread(result.ratios_2over1, 0, 3, 0.05));
}

// Compare Hot-parsers on cold data, and swap parse order.
function strategy_2() {
    var res2 = for_all_files(parse_2, runs_per_script);
    var res1 = for_all_files(parse_1, runs_per_script);
    var result = compare(res1, res2);
    print(name_1, "\t", result.time1, "ms\t", 1e6 * result.time1 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time1), 'bytes/ns\t');
    print(name_2, "\t", result.time2, "ms\t", 1e6 * result.time2 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time2), 'bytes/ns\t');
    print("Total parsed  (scripts:", result.parsed_files, ", bytes:", result.parsed_bytes, ")");
    print("Total skipped (scripts:", result.skipped_files, ", bytes:", result.skipped_bytes, ")");
    print(name_2, "/", name_1, ":", result.time2 / result.time1);
    print(name_2, "/", name_1, ":", spread(result.ratios_2over1, 0, 3, 0.05));
}

// Interleaves N hot-parser results. (if N=1, then strategy_3 is identical to strategy_1)
//
// At the moment, this is assumed to be the best approach which might mimic how
// a helper-thread would behave if it was saturated with content to be parsed.
function strategy_3() {
    var res1 = {};
    var res2 = {};
    var N = runs_per_script;
    for (var n = 0; n < N; n++) {
        for_all_files(parse_1, 1, "" + n, res1);
        for_all_files(parse_2, 1, "" + n, res2);
    }
    var result = compare(res1, res2);
    print(name_1, "\t", result.time1, "ms\t", 1e6 * result.time1 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time1), 'bytes/ns\t');
    print(name_2, "\t", result.time2, "ms\t", 1e6 * result.time2 / result.parsed_bytes, 'ns/byte\t', result.parsed_bytes / (1e6 * result.time2), 'bytes/ns\t');
    print("Total parsed  (scripts:", result.parsed_files, ", bytes:", result.parsed_bytes, ")");
    print("Total skipped (scripts:", result.skipped_files, ", bytes:", result.skipped_bytes, ")");
    print(name_2, "/", name_1, ":", result.time2 / result.time1);
    print(name_2, "/", name_1, ":", spread(result.ratios_2over1, 0, 5, 0.05));
}

// Compare cold parsers, with alternatetively cold/hot data.
//
// By swapping parser order of execution after each file, we expect that the
// previous parser execution would be enough to evict the other from the L2
// cache, and as such cause the other parser to hit cold instruction cache where
// the instruction have to be reloaded.
//
// At the moment, this is assumed to be the best approach which might mimic how
// parsers are effectively used on the main thread.
function strategy_0() {
    var path = "", content = "";
    var t_1= 0, t_2 = 0, time_1 = 0, time_2 = 0;
    var count = 0, count_bytes = 0, skipped = 0, skipped_bytes = 0;
    var parse1_first = false;
    var list = os.file.listDir(dir);
    var ratios_2over1 = [];
    var parse1_first = true;
    for (var file of list) {
        path = os.path.join(dir, file);
        content = "";
        try {
            // print(Math.round(100 * f / list.length), file);
            content = os.file.readRelativeToScript(path);
            parse1_first = !parse1_first; // Math.random() > 0.5;
            for (var i = 0; i < runs_per_script; i++) {
                // Randomize the order in which parsers are executed as they are
                // executed in the same process and the parsed content might be
                // faster to load for the second parser as it is already in memory.
                if (parse1_first) {
                    t_1 = parse_1(content);
                    t_2 = parse_2(content);
                } else {
                    t_2 = parse_2(content);
                    t_1 = parse_1(content);
                }
                time_1 += t_1;
                time_2 += t_2;
                ratios_2over1.push(t_2 / t_1);
            }
            count++;
            count_bytes += content.length;
        } catch (e) {
            // ignore all errors for now.
            skipped++;
            skipped_bytes += content.length;
        }
    }

    var total_bytes = count_bytes * runs_per_script;
    print(name_1, "\t", time_1, "ms\t", 1e6 * time_1 / total_bytes, 'ns/byte\t', total_bytes / (1e6 * time_1), 'bytes/ns\t');
    print(name_2, "\t", time_2, "ms\t", 1e6 * time_2 / total_bytes, 'ns/byte\t', total_bytes / (1e6 * time_2), 'bytes/ns\t');
    print("Total parsed  (scripts:", count * runs_per_script, ", bytes:", total_bytes, ")");
    print("Total skipped (scripts:", skipped * runs_per_script, ", bytes:", skipped_bytes, ")");
    print(name_2, "/", name_1, ":", time_2 / time_1);
    print(name_2, "/", name_1, ":", spread(ratios_2over1, 0, 5, 0.05));
}

print("Main thread comparison:")
strategy_0();
print("")
print("Off-thread comparison:")
strategy_3();
