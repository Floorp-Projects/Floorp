/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var util = require('./util');
var l10n = require('./l10n');
var spell = require('./spell');
var filesystem = require('./filesystem');
var Status = require('../types/types').Status;

/*
 * An implementation of the functions that call the filesystem, designed to
 * support the file type.
 */

/**
 * Helper for the parse() function from the file type.
 * See gcli/util/filesystem.js for details
 */
exports.parse = function(context, typed, options) {
  return filesystem.stat(typed).then(function(stats) {
    // The 'save-as' case - the path should not exist but does
    if (options.existing === 'no' && stats.exists) {
      return {
        value: undefined,
        status: Status.INCOMPLETE,
        message: l10n.lookupFormat('fileErrExists', [ typed ]),
        predictor: undefined // No predictions that we can give here
      };
    }

    if (stats.exists) {
      // The path exists - check it's the correct file type ...
      if (options.filetype === 'file' && !stats.isFile) {
        return {
          value: undefined,
          status: Status.INCOMPLETE,
          message: l10n.lookupFormat('fileErrIsNotFile', [ typed ]),
          predictor: getPredictor(typed, options)
        };
      }

      if (options.filetype === 'directory' && !stats.isDir) {
        return {
          value: undefined,
          status: Status.INCOMPLETE,
          message: l10n.lookupFormat('fileErrIsNotDirectory', [ typed ]),
          predictor: getPredictor(typed, options)
        };
      }

      // ... and that it matches any 'match' RegExp
      if (options.matches != null && !options.matches.test(typed)) {
        return {
          value: undefined,
          status: Status.INCOMPLETE,
          message: l10n.lookupFormat('fileErrDoesntMatch',
                                     [ typed, options.source ]),
          predictor: getPredictor(typed, options)
        };
      }
    }
    else {
      if (options.existing === 'yes') {
        // We wanted something that exists, but it doesn't. But we don't know
        // if the path so far is an ERROR or just INCOMPLETE
        var parentName = filesystem.dirname(typed);
        return filesystem.stat(parentName).then(function(stats) {
          return {
            value: undefined,
            status: stats.isDir ? Status.INCOMPLETE : Status.ERROR,
            message: l10n.lookupFormat('fileErrNotExists', [ typed ]),
            predictor: getPredictor(typed, options)
          };
        });
      }
    }

    // We found no problems
    return {
      value: typed,
      status: Status.VALID,
      message: undefined,
      predictor: getPredictor(typed, options)
    };
  });
};

var RANK_OPTIONS = { noSort: true, prefixZero: true };

/**
 * We want to be able to turn predictions off in Firefox
 */
exports.supportsPredictions = true;

/**
 * Get a function which creates predictions of files that match the given
 * path
 */
function getPredictor(typed, options) {
  if (!exports.supportsPredictions) {
    return undefined;
  }

  return function() {
    var allowFile = (options.filetype !== 'directory');
    var parts = filesystem.split(typed);

    var absolute = (typed.indexOf('/') === 0);
    var roots;
    if (absolute) {
      roots = [ { name: '/', dist: 0, original: '/' } ];
    }
    else {
      roots = dirHistory.getCommonDirectories().map(function(root) {
        return { name: root, dist: 0, original: root };
      });
    }

    // Add each part of the typed pathname onto each of the roots in turn,
    // Finding options from each of those paths, and using these options as
    // our roots for the next part
    var partsAdded = util.promiseEach(parts, function(part, index) {

      var partsSoFar = filesystem.join.apply(filesystem, parts.slice(0, index + 1));

      // We allow this file matches in this pass if we're allowed files at all
      // (i.e this isn't 'cd') and if this is the last part of the path
      var allowFileForPart = (allowFile && index >= parts.length - 1);

      var rootsPromise = util.promiseEach(roots, function(root) {

        // Extend each roots to a list of all the files in each of the roots
        var matchFile = allowFileForPart ? options.matches : null;
        var promise = filesystem.ls(root.name, matchFile);

        var onSuccess = function(entries) {
          // Unless this is the final part filter out the non-directories
          if (!allowFileForPart) {
            entries = entries.filter(function(entry) {
              return entry.isDir;
            });
          }
          var entryMap = {};
          entries.forEach(function(entry) {
            entryMap[entry.pathname] = entry;
          });
          return entryMap;
        };

        var onError = function(err) {
          // We expect errors due to the path not being a directory, not being
          // accessible, or removed since the call to 'readdir'
          return {};
        };

        promise = promise.then(onSuccess, onError);

        // We want to compare all the directory entries with the original root
        // plus the partsSoFar
        var compare = filesystem.join(root.original, partsSoFar);

        return promise.then(function(entryMap) {

          var ranks = spell.rank(compare, Object.keys(entryMap), RANK_OPTIONS);
          // penalize each path by the distance of it's parent
          ranks.forEach(function(rank) {
            rank.original = root.original;
            rank.stats = entryMap[rank.name];
          });
          return ranks;
        });
      });

      return rootsPromise.then(function(data) {
        // data is an array of arrays of ranking objects. Squash down.
        data = data.reduce(function(prev, curr) {
          return prev.concat(curr);
        }, []);

        data.sort(function(r1, r2) {
          return r1.dist - r2.dist;
        });

        // Trim, but by how many?
        // If this is the last run through, we want to present the user with
        // a sensible set of predictions. Otherwise we want to trim the tree
        // to a reasonable set of matches, so we're happy with 1
        // We look through x +/- 3 roots, and find the one with the biggest
        // distance delta, and cut below that
        // x=5 for the last time through, and x=8 otherwise
        var isLast = index >= parts.length - 1;
        var start = isLast ? 1 : 5;
        var end = isLast ? 7 : 10;

        var maxDeltaAt = start;
        var maxDelta = data[start].dist - data[start - 1].dist;

        for (var i = start + 1; i < end; i++) {
          var delta = data[i].dist - data[i - 1].dist;
          if (delta >= maxDelta) {
            maxDelta = delta;
            maxDeltaAt = i;
          }
        }

        // Update the list of roots for the next time round
        roots = data.slice(0, maxDeltaAt);
      });
    });

    return partsAdded.then(function() {
      var predictions = roots.map(function(root) {
        var isFile = root.stats && root.stats.isFile;
        var isDir = root.stats && root.stats.isDir;

        var name = root.name;
        if (isDir && name.charAt(name.length) !== filesystem.sep) {
          name += filesystem.sep;
        }

        return {
          name: name,
          incomplete: !(allowFile && isFile),
          isFile: isFile,  // Added for describe, below
          dist: root.dist, // TODO: Remove - added for debug in describe
        };
      });

      return util.promiseEach(predictions, function(prediction) {
        if (!prediction.isFile) {
          prediction.description = '(' + prediction.dist + ')';
          prediction.dist = undefined;
          prediction.isFile = undefined;
          return prediction;
        }

        return filesystem.describe(prediction.name).then(function(description) {
          prediction.description = description;
          prediction.dist = undefined;
          prediction.isFile = undefined;
          return prediction;
        });
      });
    });
  };
}

// =============================================================================

/*
 * The idea is that we maintain a list of 'directories that the user is
 * interested in'. We store directories in a most-frequently-used cache
 * of some description.
 * But for now we're just using / and ~/
 */
var dirHistory = {
  getCommonDirectories: function() {
    return [
      filesystem.sep,  // i.e. the root directory
      filesystem.home  // i.e. the users home directory
    ];
  },
  addCommonDirectory: function(ignore) {
    // Not implemented yet
  }
};
