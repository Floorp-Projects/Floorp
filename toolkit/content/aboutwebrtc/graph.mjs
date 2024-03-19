/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function compStyle(property) {
  return getComputedStyle(window.document.body).getPropertyValue(property);
}

function toHumanReadable(num, fpDecimals) {
  const prefixes = [..." kMGTPEYZYRQ"];
  const inner = (curr, remainingPrefixes) => {
    return Math.abs(curr >= 1000)
      ? inner(curr / 1000, remainingPrefixes.slice(1, -1))
      : [curr.toFixed(fpDecimals), remainingPrefixes[0].trimEnd()];
  };
  return inner(num, prefixes);
}

class GraphImpl {
  constructor(width, height) {
    this.width = width;
    this.height = height;
  }

  // The returns the earliest time to graph
  startTime = dataSet => (dataSet.earliest() || { time: 0 }).time;

  // Returns the latest time to graph
  stopTime = dataSet => (dataSet.latest() || { time: 0 }).time;

  // The default background color
  bgColor = () => compStyle("--in-content-page-background");
  // The color to use for value graph lines
  valueLineColor = () => "grey";
  // The color to use for average graph lines and text
  averageLineColor = () => "green";
  // The color to use for the max value
  maxColor = () => "grey";
  // The color to use for the min value
  minColor = () => "grey";
  // Title color
  titleColor = () => compStyle("--in-content-page-color");
  // The color to use for a data point at a time.
  // The destination x coordinate and graph width are also provided.
  datumColor = () => "red";

  // Returns an SVG element that needs to be inserted into the DOM for display
  drawSparseValues = (dataSet, title, config) => {
    const { width, height } = this;
    // Clear the canvas
    const bgColor = this.bgColor();
    const mkSvgElem = type =>
      document.createElementNS("http://www.w3.org/2000/svg", type);
    const svgText = (x, y, text, color, subclass) => {
      const txt = mkSvgElem("text");
      txt.setAttribute("x", x);
      txt.setAttribute("y", y);
      txt.setAttribute("stroke", bgColor);
      txt.setAttribute("fill", color);
      txt.setAttribute("paint-order", "stroke");
      txt.textContent = text;
      txt.classList.add(["graph-text", ...[subclass]].join("-"));
      return txt;
    };
    const svg = mkSvgElem("svg");
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    svg.setAttribute("version", "1.1");
    svg.setAttribute("width", width);
    svg.setAttribute("height", height);
    svg.classList.add("svg-graph");
    const rect = mkSvgElem("rect");
    rect.setAttribute("fill", bgColor);
    rect.setAttribute("width", width);
    rect.setAttribute("height", height);
    svg.appendChild(rect);

    if (config.toRate) {
      dataSet = dataSet.toRateDataSet();
    }

    const startTime = this.startTime(dataSet);
    const stopTime = this.stopTime(dataSet);
    let timeFilter = ({ time }) => time >= startTime && time <= stopTime;

    let avgDataSet = { dataPoints: [] };
    if (!config.noAvg) {
      avgDataSet = dataSet.toRollingAverageDataSet(config.avgPoints);
    }

    let filtered = dataSet.filter(timeFilter);
    if (filtered.dataPoints == []) {
      return svg;
    }

    let range = filtered.dataRange();
    if (range === undefined) {
      return svg;
    }
    let { min: rangeMin, max: rangeMax } = range;

    // Adjust the _display_ range to lift flat lines towards the center
    if (rangeMin == rangeMax) {
      rangeMin = rangeMin - 1;
      rangeMax = rangeMax + 1;
    }
    const yFactor = (height - 26) / (1 + rangeMax - rangeMin);
    const yPos = ({ value }) =>
      this.height - 1 - (value - rangeMin) * yFactor - 13;
    const xFactor = width / (1 + stopTime - startTime);
    const xPos = ({ time }) => (time - startTime) * xFactor;

    const toPathStr = dataPoints =>
      [...dataPoints]
        .map(
          (datum, index) => `${index ? "L" : "M"}${xPos(datum)} ${yPos(datum)}`
        )
        .join(" ");
    const valuePath = mkSvgElem("path");
    valuePath.setAttribute("d", toPathStr(filtered.dataPoints));
    valuePath.setAttribute("stroke", this.valueLineColor());
    valuePath.setAttribute("fill", "none");
    svg.appendChild(valuePath);

    const avgPath = mkSvgElem("path");
    avgPath.setAttribute("d", toPathStr(avgDataSet.dataPoints));
    avgPath.setAttribute("stroke", this.averageLineColor());
    avgPath.setAttribute("fill", "none");
    svg.appendChild(avgPath);
    const fixed = num => num.toFixed(config.fixedPointDecimals);
    const formatValue = value =>
      config.toHuman
        ? toHumanReadable(value, config.fixedPointDecimals).join("")
        : fixed(value);

    // Draw rolling average text
    avgDataSet.dataPoints.slice(-1).forEach(({ value }) => {
      svg.appendChild(
        svgText(
          5,
          height - 4,
          `AVG: ${formatValue(value)}`,
          this.averageLineColor(),
          "avg"
        )
      );
    });

    // Draw title text
    if (title) {
      svg.appendChild(
        svgText(
          5,
          12,
          `${title}${config.toRate ? "/s" : ""}`,
          this.titleColor(this),
          "title"
        )
      );
    }

    // Draw max value text
    const maxText = svgText(
      width - 5,
      12,
      `Max: ${formatValue(range.max)}`,
      this.maxColor(range.max),
      "max"
    );
    maxText.setAttribute("text-anchor", "end");
    svg.appendChild(maxText);

    // Draw min value text
    const minText = svgText(
      width - 5,
      height - 4,
      `Min: ${formatValue(range.min)}`,
      this.minColor(range.min),
      "min"
    );
    minText.setAttribute("text-anchor", "end");
    svg.appendChild(minText);
    return svg;
  };
}

export { GraphImpl };
