/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

importScripts("ClusterLib.js", "ColorConversion.js");

// Offsets in the ImageData pixel array to reach pixel colors
const PIXEL_RED = 0;
const PIXEL_GREEN = 1;
const PIXEL_BLUE = 2;
const PIXEL_ALPHA = 3;

// Number of components in one ImageData pixel (RGBA)
const NUM_COMPONENTS = 4;

// Shift a color represented as a 24 bit integer by N bits to get a component
const RED_SHIFT = 16;
const GREEN_SHIFT = 8;

// Only run the N most frequent unique colors through the clustering algorithm
// Images with more than this many unique colors will have reduced accuracy.
const MAX_COLORS_TO_MERGE = 500;

// Each cluster of colors has a mean color in the Lab color space.
// If the euclidean distance between the means of two clusters is greater
// than or equal to this threshold, they won't be merged.
const MERGE_THRESHOLD = 12;

// The highest the distance handicap can be for large clusters
const MAX_SIZE_HANDICAP = 5;
// If the handicap is below this number, it is cut off to zero
const SIZE_HANDICAP_CUTOFF = 2;

// If potential background colors deviate from the mean background color by
// this threshold or greater, finding a background color will fail
const BACKGROUND_THRESHOLD = 10;

// Alpha component of colors must be larger than this in order to make it into
// the clustering algorithm or be considered a background color (0 - 255).
const MIN_ALPHA = 25;

// The euclidean distance in the Lab color space under which merged colors
// are weighted lower for being similar to the background color
const BACKGROUND_WEIGHT_THRESHOLD = 15;

// The range in which color chroma differences will affect desirability.
// Colors with chroma outside of the range take on the desirability of
// their nearest extremes. Should be roughly 0 - 150.
const CHROMA_WEIGHT_UPPER = 90;
const CHROMA_WEIGHT_LOWER = 1;
const CHROMA_WEIGHT_MIDDLE = (CHROMA_WEIGHT_UPPER + CHROMA_WEIGHT_LOWER) / 2;

/**
 * When we receive a message from the outside world, find the representative
 * colors of the given image.  The colors will be posted back to the caller
 * through the "colors" property on the event data object as an array of
 * integers.  Colors of lower indices are more representative.
 * This array can be empty if this worker can't find a color.
 *
 * @param event
 *        A MessageEvent whose data should have the following properties:
 *          imageData - A DOM ImageData instance to analyze
 *          maxColors - The maximum number of representative colors to find,
 *                      defaults to 1 if not provided
 */
onmessage = function(event) {
  let imageData = event.data.imageData;
  let pixels = imageData.data;
  let width = imageData.width;
  let height = imageData.height;
  let maxColors = event.data.maxColors;
  if (typeof(maxColors) != "number") {
    maxColors = 1;
  }

  let allColors = getColors(pixels, width, height);

  // Only merge top colors by frequency for speed.
  let mergedColors = mergeColors(allColors.slice(0, MAX_COLORS_TO_MERGE),
                                 width * height, MERGE_THRESHOLD);

  let backgroundColor = getBackgroundColor(pixels, width, height);

  mergedColors = mergedColors.map(function(cluster) {
    // metadata holds a bunch of information about the color represented by
    // this cluster
    let metadata = cluster.item;

    // the basis of color desirability is how much of the image the color is
    // responsible for, but we'll need to weigh this number differently
    // depending on other factors
    metadata.desirability = metadata.ratio;
    let weight = 1;

    // if the color is close to the background color, we don't want it
    if (backgroundColor != null) {
      let backgroundDistance = labEuclidean(metadata.mean, backgroundColor);
      if (backgroundDistance < BACKGROUND_WEIGHT_THRESHOLD) {
        weight = backgroundDistance / BACKGROUND_WEIGHT_THRESHOLD;
      }
    }

    // prefer more interesting colors, but don't knock low chroma colors
    // completely out of the running (lower bound), and we don't really care
    // if a color is slightly more intense than another on the higher end
    let chroma = labChroma(metadata.mean);
    if (chroma < CHROMA_WEIGHT_LOWER) {
      chroma = CHROMA_WEIGHT_LOWER;
    } else if (chroma > CHROMA_WEIGHT_UPPER) {
      chroma = CHROMA_WEIGHT_UPPER;
    }
    weight *= chroma / CHROMA_WEIGHT_MIDDLE;

    metadata.desirability *= weight;
    return metadata;
  });

  // only send back the most desirable colors
  mergedColors.sort(function(a, b) {
    return b.desirability != a.desirability ? b.desirability - a.desirability : b.color - a.color;
  });
  mergedColors = mergedColors.map(function(metadata) {
    return metadata.color;
  }).slice(0, maxColors);
  postMessage({ colors: mergedColors });
};

/**
 * Given the pixel data and dimensions of an image, return an array of objects
 * associating each unique color and its frequency in the image, sorted
 * descending by frequency.  Sufficiently transparent colors are ignored.
 *
 * @param pixels
 *        Pixel data array for the image to get colors from (ImageData.data).
 * @param width
 *        Width of the image, in # of pixels.
 * @param height
 *        Height of the image, in # of pixels.
 *
 * @return An array of objects with color and freq properties, sorted
 *         descending by freq
 */
function getColors(pixels, width, height) {
  let colorFrequency = {};
  for (let x = 0; x < width; x++) {
    for (let y = 0; y < height; y++) {
      let offset = (x * NUM_COMPONENTS) + (y * NUM_COMPONENTS * width);

      if (pixels[offset + PIXEL_ALPHA] < MIN_ALPHA) {
        continue;
      }

      let color = pixels[offset + PIXEL_RED] << RED_SHIFT
                | pixels[offset + PIXEL_GREEN] << GREEN_SHIFT
                | pixels[offset + PIXEL_BLUE];

      if (color in colorFrequency) {
        colorFrequency[color]++;
      } else {
        colorFrequency[color] = 1;
      }
    }
  }

  let colors = [];
  for (var color in colorFrequency) {
    colors.push({ color: +color, freq: colorFrequency[+color] });
  }
  colors.sort(descendingFreqSort);
  return colors;
}

/**
 * Given an array of objects from getColors, the number of pixels in the
 * image, and a merge threshold, run the clustering algorithm on the colors
 * and return the set of merged clusters.
 *
 * @param colorFrequencies
 *        An array of objects from getColors to cluster
 * @param numPixels
 *        The number of pixels in the image
 * @param threshold
 *        The maximum distance between two clusters for which those clusters
 *        can be merged.
 *
 * @return An array of merged clusters
 *
 * @see clusterlib.hcluster
 * @see getColors
 */
function mergeColors(colorFrequencies, numPixels, threshold) {
  let items = colorFrequencies.map(function(colorFrequency) {
    let color = colorFrequency.color;
    let freq = colorFrequency.freq;
    return {
      mean: rgb2lab(color >> RED_SHIFT, color >> GREEN_SHIFT & 0xff,
                    color & 0xff),
      // the canonical color of the cluster
      // (one w/ highest freq or closest to mean)
      color: color,
      colors: [color],
      highFreq: freq,
      highRatio: freq / numPixels,
      // the individual color w/ the highest frequency in this cluster
      highColor: color,
      // ratio of image taken up by colors in this cluster
      ratio: freq / numPixels,
      freq: freq,
    };
  });

  let merged = clusterlib.hcluster(items, distance, merge, threshold);
  return merged;
}

function descendingFreqSort(a, b) {
  return b.freq != a.freq ? b.freq - a.freq : b.color - a.color;
}

/**
 * Given two items for a pair of clusters (as created in mergeColors above),
 * determine the distance between them so we know if we should merge or not.
 * Uses the euclidean distance between their mean colors in the lab color
 * space, weighted so larger items are harder to merge.
 *
 * @param item1
 *        The first item to compare
 * @param item2
 *        The second item to compare
 *
 * @return The distance between the two items
 */
function distance(item1, item2) {
  // don't cluster large blocks of color unless they're really similar
  let minRatio = Math.min(item1.ratio, item2.ratio);
  let dist = labEuclidean(item1.mean, item2.mean);
  let handicap = Math.min(MAX_SIZE_HANDICAP, dist * minRatio);
  if (handicap <= SIZE_HANDICAP_CUTOFF) {
    handicap = 0;
  }
  return dist + handicap;
}

/**
 * Find the euclidean distance between two colors in the Lab color space.
 *
 * @param color1
 *        The first color to compare
 * @param color2
 *        The second color to compare
 *
 * @return The euclidean distance between the two colors
 */
function labEuclidean(color1, color2) {
  return Math.sqrt(
      Math.pow(color2.lightness - color1.lightness, 2)
    + Math.pow(color2.a - color1.a, 2)
    + Math.pow(color2.b - color1.b, 2));
}

/**
 * Given items from two clusters we know are appropriate for merging,
 * merge them together into a third item such that its metadata describes both
 * input items.  The "color" property is set to the color in the new item that
 * is closest to its mean color.
 *
 * @param item1
 *        The first item to merge
 * @param item2
 *        The second item to merge
 *
 * @return An item that represents the merging of the given items
 */
function merge(item1, item2) {
  let lab1 = item1.mean;
  let lab2 = item2.mean;

  /* algorithm tweak point - weighting the mean of the cluster */
  let num1 = item1.freq;
  let num2 = item2.freq;

  let total = num1 + num2;

  let mean = {
    lightness: (lab1.lightness * num1 + lab2.lightness * num2) / total,
    a: (lab1.a * num1 + lab2.a * num2) / total,
    b: (lab1.b * num1 + lab2.b * num2) / total
  };

  let colors = item1.colors.concat(item2.colors);

  // get the canonical color of the new cluster
  let color;
  let avgFreq = colors.length / (item1.freq + item2.freq);
  if ((item1.highFreq > item2.highFreq) && (item1.highFreq > avgFreq * 2)) {
    color = item1.highColor;
  } else if (item2.highFreq > avgFreq * 2) {
    color = item2.highColor;
  } else {
    // if there's no stand-out color
    let minDist = Infinity, closest = 0;
    for (let i = 0; i < colors.length; i++) {
      let color = colors[i];
      let lab = rgb2lab(color >> RED_SHIFT, color >> GREEN_SHIFT & 0xff,
                        color & 0xff);
      let dist = labEuclidean(lab, mean);
      if (dist < minDist) {
        minDist = dist;
        closest = i;
      }
    }
    color = colors[closest];
  }

  const higherItem = item1.highFreq > item2.highFreq ? item1 : item2;

  return {
    mean: mean,
    color: color,
    highFreq: higherItem.highFreq,
    highColor: higherItem.highColor,
    highRatio: higherItem.highRatio,
    ratio: item1.ratio + item2.ratio,
    freq: item1.freq + item2.freq,
    colors: colors,
  };
}

/**
 * Find the background color of the given image.
 *
 * @param pixels
 *        The pixel data for the image (an array of component integers)
 * @param width
 *        The width of the image
 * @param height
 *        The height of the image
 *
 * @return The background color of the image as a Lab object, or null if we
 *         can't determine the background color
 */
function getBackgroundColor(pixels, width, height) {
  // we'll assume that if the four corners are roughly the same color,
  // then that's the background color
  let coordinates = [[0, 0], [width - 1, 0], [width - 1, height - 1],
                     [0, height - 1]];

  // find the corner colors in LAB
  let cornerColors = [];
  for (let i = 0; i < coordinates.length; i++) {
    let offset = (coordinates[i][0] * NUM_COMPONENTS)
               + (coordinates[i][1] * NUM_COMPONENTS * width);
    if (pixels[offset + PIXEL_ALPHA] < MIN_ALPHA) {
      // we can't make very accurate judgements below this opacity
      continue;
    }
    cornerColors.push(rgb2lab(pixels[offset + PIXEL_RED],
                              pixels[offset + PIXEL_GREEN],
                              pixels[offset + PIXEL_BLUE]));
  }

  // we want at least two points at acceptable alpha levels
  if (cornerColors.length <= 1) {
    return null;
  }

  // find the average color among the corners
  let averageColor = { lightness: 0, a: 0, b: 0 };
  cornerColors.forEach(function(color) {
    for (let i in color) {
      averageColor[i] += color[i];
    }
  });
  for (let i in averageColor) {
    averageColor[i] /= cornerColors.length;
  }

  // if we have fewer points due to low alpha, they need to be closer together
  let threshold = BACKGROUND_THRESHOLD
                * (cornerColors.length / coordinates.length);

  // if any of the corner colors deviate enough from the average, they aren't
  // similar enough to be considered the background color
  for (let cornerColor of cornerColors) {
    if (labEuclidean(cornerColor, averageColor) > threshold) {
      return null;
    }
  }
  return averageColor;
}
