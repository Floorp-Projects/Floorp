/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  const benchmark_url = "https://grandprixbench.netlify.app/";
  const iterations = `${context.options.browsertime.grandprix_iterations}`;
  const tests = [
    "Vanilla-HTML-TodoMVC",
    "Vanilla-TodoMVC",
    "Preact-TodoMVC",
    "Preact-TodoMVC-Modern",
    "React-TodoMVC",
    //"Elm-TodoMVC",              // removed from suite
    //"Rust-Yew-TodoMVC",         // removed from suite
    //"Hydration-Preact",         // removed from suite
    //"Scroll-Windowing-React",   // removed from suite
    "Monaco-Editor",
    "Monaco-Syntax-Highlight",
    "React-Stockcharts",
    "React-Stockcharts-SVG",
    "Leaflet-Fractal",
    //"SVG-UI",                   // removed from suite
    "Proxx-Tables",
    "Proxx-Tables-Lit",
    "Proxx-Tables-Canvas",
  ];

  // Build test url
  let url = benchmark_url + "?suites=";
  tests.forEach(test => {
    url = url + test + ",";
  });
  url = url + "&iterations=" + iterations;

  console.log("Using url=" + url);
  await commands.measure.start(url);

  await commands.wait.byTime("3000");
  await commands.click.byXpath("/html/body/main/div/div[1]/div/div/button");

  let finished = 0;
  do {
    await commands.wait.byTime("30000");
    finished = await commands.js.run('return typeof results != "undefined"');
  } while (!finished);

  if (context.options.browser === "firefox") {
    let buildId = await commands.js.runPrivileged(
      "return Services.appinfo.appBuildID;"
    );
    console.log(buildId);
  }

  let output = {};
  output.score = {};
  output.score.total = await commands.js.run("return results.Score");
  output.iterations = await commands.js.run("return results.iterations");

  let subtests = {};
  for (let i = 0; i < output.iterations.length; i++) {
    for (const [key, value] of Object.entries(output.iterations[i])) {
      if (!subtests.hasOwnProperty(key)) {
        subtests[key] = [];
      }
      subtests[key].push(value.total);
    }
  }

  console.log("score, ", output.score.total.mean);
  for (const [key, value] of Object.entries(subtests)) {
    const average = value.reduce((a, b) => a + b, 0) / value.length;
    output.score[key] = average;
    console.log(key, ",", average);
  }

  await commands.measure.add("grandprix-s3", output);
};
