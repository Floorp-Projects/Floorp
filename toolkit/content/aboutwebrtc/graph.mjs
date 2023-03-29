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
  constructor(canvas, width, height) {
    this.canvas = canvas;
    this.width = width;
    this.height = height;
    this.drawCtx = canvas.getContext("2d");
  }

  // The returns the earliest time to graph
  startTime = dataSet => (dataSet.earliest() || { time: 0 }).time;

  // Returns the latest time to graph
  stopTime = dataSet => (dataSet.latest() || { time: 0 }).time;

  /** @type {HTMLCanvasElement} */
  canvas = {};
  /** @type {CanvasRenderingContext2D} */
  drawCtx = {};
  // The default background color
  bgColor = () => compStyle("--in-content-page-background");
  // The color to use for value graph lines
  valueLineColor = () => "grey";
  // The color to use for average graph lines and text
  averageLineColor = () => "green";
  // The color to use for the max value
  maxColor = ({ time, value }) => "grey";
  // The color to use for the min value
  minColor = ({ time, value }) => "grey";
  // Title color
  titleColor = title => compStyle("--in-content-page-color");
  // The color to use for a data point at a time.
  // The destination x coordinate and graph width are also provided.
  datumColor = ({ time, value, x, width }) => "red";

  drawSparseValues = (dataSet, title, config) => {
    if (config.toRate) {
      dataSet = dataSet.toRateDataSet();
    }
    const { drawCtx: ctx, width, height } = this;
    // Clear the canvas
    const bgColor = this.bgColor();
    ctx.fillStyle = bgColor;
    ctx.clearRect(0, 0, width, height);
    ctx.fillRect(0, 0, width, height);

    const startTime = this.startTime(dataSet);
    const stopTime = this.stopTime(dataSet);
    let timeFilter = ({ time }) => time >= startTime && time <= stopTime;

    let avgDataSet = { dataPoints: [] };
    if (!config.noAvg) {
      avgDataSet = dataSet.toRollingAverageDataSet(config.avgPoints);
    }

    let filtered = dataSet.filter(timeFilter);
    if (filtered.dataPoints == []) {
      return;
    }

    let range = filtered.dataRange();
    if (range === undefined) {
      return;
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
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.strokeStyle = this.valueLineColor();
    [...filtered.dataPoints].forEach((datum, index) => {
      if (!index) {
        ctx.moveTo(xPos(datum), yPos(datum));
      } else {
        ctx.lineTo(xPos(datum), yPos(datum));
      }
      // Draw data point
      ctx.fillStyle = this.datumColor({ ...datum, width: this.width });
      ctx.fillRect(xPos(datum), yPos(datum), 1, 1);
    });
    ctx.stroke();

    // Rolling average
    ctx.lineWidth = 2;
    ctx.strokeStyle = this.averageLineColor();
    ctx.beginPath();
    const rollingAverage = avgDataSet.dataPoints;
    [...rollingAverage].forEach((datum, index) => {
      if (!index) {
        ctx.moveTo(xPos(datum), yPos(datum));
      } else {
        ctx.lineTo(xPos(datum), yPos(datum));
      }
    });
    const fixed = num => num.toFixed(config.fixedPointDecimals);
    const formatValue = value =>
      config.toHuman
        ? toHumanReadable(value, config.fixedPointDecimals).join("")
        : fixed(value);
    rollingAverage.slice(-1).forEach(({ value }) => {
      ctx.stroke();
      ctx.fillStyle = this.averageLineColor();
      ctx.font = "12px Arial";
      ctx.textAlign = "left";
      ctx.fillText(formatValue(value), 5, this.height - 4);
    });

    // Draw the title
    if (title) {
      ctx.fillStyle = this.titleColor(this);
      ctx.font = "12px Arial";
      ctx.textAlign = "left";
      ctx.fillText(`${title}${config.toRate ? "/s" : ""}`, 5, 12);
    }
    ctx.font = "12px Arial";
    ctx.fillStyle = this.maxColor(range.max);
    ctx.textAlign = "right";
    ctx.fillText(formatValue(range.max), this.width - 5, 12);
    ctx.fillStyle = this.minColor(range.min);
    ctx.fillText(formatValue(range.min), this.width - 5, this.height - 4);
  };
}

export { GraphImpl };
