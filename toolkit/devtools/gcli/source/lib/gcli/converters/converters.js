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

var promise = require('../util/promise');
var util = require('../util/util');
var host = require('../util/host');

// It's probably easiest to read this bottom to top

/**
 * Best guess at creating a DOM element from random data
 */
var fallbackDomConverter = {
  from: '*',
  to: 'dom',
  exec: function(data, conversionContext) {
    return conversionContext.document.createTextNode(data || '');
  }
};

/**
 * Best guess at creating a string from random data
 */
var fallbackStringConverter = {
  from: '*',
  to: 'string',
  exec: function(data, conversionContext) {
    return data == null ? '' : data.toString();
  }
};

/**
 * Convert a view object to a DOM element
 */
var viewDomConverter = {
  item: 'converter',
  from: 'view',
  to: 'dom',
  exec: function(view, conversionContext) {
    if (!view.isView) {
      view = conversionContext.createView(view);
    }
    return view.toDom(conversionContext.document);
  }
};

/**
 * Convert a view object to a string
 */
var viewStringConverter = {
  item: 'converter',
  from: 'view',
  to: 'string',
  exec: function(view, conversionContext) {
    if (!view.isView) {
      view = conversionContext.createView(view);
    }
    return view.toDom(conversionContext.document).textContent;
  }
};

/**
 * Convert a view object to a string
 */
var stringViewStringConverter = {
  item: 'converter',
  from: 'stringView',
  to: 'string',
  exec: function(view, conversionContext) {
    if (!view.isView) {
      view = conversionContext.createView(view);
    }
    return view.toDom(conversionContext.document).textContent;
  }
};

/**
 * Convert an exception to a DOM element
 */
var errorDomConverter = {
  item: 'converter',
  from: 'error',
  to: 'dom',
  exec: function(ex, conversionContext) {
    var node = util.createElement(conversionContext.document, 'p');
    node.className = 'gcli-error';
    node.textContent = ex;
    return node;
  }
};

/**
 * Convert an exception to a string
 */
var errorStringConverter = {
  item: 'converter',
  from: 'error',
  to: 'string',
  exec: function(ex, conversionContext) {
    return '' + ex;
  }
};

/**
 * Create a new converter by using 2 converters, one after the other
 */
function getChainConverter(first, second) {
  if (first.to !== second.from) {
    throw new Error('Chain convert impossible: ' + first.to + '!=' + second.from);
  }
  return {
    from: first.from,
    to: second.to,
    exec: function(data, conversionContext) {
      var intermediate = first.exec(data, conversionContext);
      return second.exec(intermediate, conversionContext);
    }
  };
}

/**
 * This is where we cache the converters that we know about
 */
var converters = {
  from: {}
};

/**
 * Add a new converter to the cache
 */
exports.addConverter = function(converter) {
  var fromMatch = converters.from[converter.from];
  if (fromMatch == null) {
    fromMatch = {};
    converters.from[converter.from] = fromMatch;
  }

  fromMatch[converter.to] = converter;
};

/**
 * Remove an existing converter from the cache
 */
exports.removeConverter = function(converter) {
  var fromMatch = converters.from[converter.from];
  if (fromMatch == null) {
    return;
  }

  if (fromMatch[converter.to] === converter) {
    fromMatch[converter.to] = null;
  }
};

/**
 * Work out the best converter that we've got, for a given conversion.
 */
function getConverter(from, to) {
  var fromMatch = converters.from[from];
  if (fromMatch == null) {
    return getFallbackConverter(from, to);
  }

  var converter = fromMatch[to];
  if (converter == null) {
    // Someone is going to love writing a graph search algorithm to work out
    // the smallest number of conversions, or perhaps the least 'lossy'
    // conversion but for now the only 2 step conversions which we are going to
    // special case are foo->view->dom and foo->stringView->string.
    if (to === 'dom') {
      converter = fromMatch.view;
      if (converter != null) {
        return getChainConverter(converter, viewDomConverter);
      }
    }

    if (to === 'string') {
      converter = fromMatch.stringView;
      if (converter != null) {
        return getChainConverter(converter, stringViewStringConverter);
      }
      converter = fromMatch.view;
      if (converter != null) {
        return getChainConverter(converter, viewStringConverter);
      }
    }

    return getFallbackConverter(from, to);
  }
  return converter;
}

/**
 * Helper for getConverter to pick the best fallback converter
 */
function getFallbackConverter(from, to) {
  console.error('No converter from ' + from + ' to ' + to + '. Using fallback');

  if (to === 'dom') {
    return fallbackDomConverter;
  }

  if (to === 'string') {
    return fallbackStringConverter;
  }

  throw new Error('No conversion possible from ' + from + ' to ' + to + '.');
}

/**
 * Convert some data from one type to another
 * @param data The object to convert
 * @param from The type of the data right now
 * @param to The type that we would like the data in
 * @param conversionContext An execution context (i.e. simplified requisition)
 * which is often required for access to a document, or createView function
 */
exports.convert = function(data, from, to, conversionContext) {
  try {
    if (from === to) {
      return promise.resolve(data);
    }

    var converter = getConverter(from, to);
    return host.exec(function() {
      return converter.exec(data, conversionContext);
    });
  }
  catch (ex) {
    var converter = getConverter('error', to);
    return host.exec(function() {
      return converter.exec(ex, conversionContext);
    });
  }
};

/**
 * Items for export
 */
exports.items = [
  viewDomConverter, viewStringConverter, stringViewStringConverter,
  errorDomConverter, errorStringConverter
];
