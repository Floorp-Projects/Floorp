/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["WebsiteMetadata"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher", "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");

var WebsiteMetadata = {
  /**
   * Asynchronously parse the document extract metadata. A 'Website:Metadata' event with the metadata
   * will be sent.
   */
  parseAsynchronously: function(doc) {
    Task.spawn(function() {
      let metadata = getMetadata(doc, doc.location.href, {
        image_url: metadataRules['image_url'],
        provider: metadataRules['provider']
      });

      // No metadata was extracted, so don't bother sending it.
      if (Object.keys(metadata).length === 0) {
        return;
      }

      let msg = {
        type: 'Website:Metadata',
        location: doc.location.href,
        hasImage: metadata.image_url && metadata.image_url !== "",
        metadata: JSON.stringify(metadata),
      };

      EventDispatcher.instance.sendRequest(msg);
    });
  }
};

// #################################################################################################
// # Modified version of makeUrlAbsolute() to not import url parser library (and dependencies)
// #################################################################################################

function makeUrlAbsolute(context, relative) {
	var a = context.doc.createElement('a');
    a.href = relative;
    return a.href;
}

// #################################################################################################
// # page-metadata-parser
// # https://github.com/mozilla/page-metadata-parser/
// # 61c58cbd0f0bf2153df832a388a79c66b288b98c
// #################################################################################################

function buildRuleset(name, rules, processors) {
  const reversedRules = Array.from(rules).reverse();
  const builtRuleset = ruleset(...reversedRules.map(([query, handler], order) => rule(
    dom(query),
    node => [{
      score: order,
      flavor: name,
      notes: handler(node),
    }]
  )));

  return (doc, context) => {
    const kb = builtRuleset.score(doc);
    const maxNode = kb.max(name);

    if (maxNode) {
      let value = maxNode.flavors.get(name);

      if (processors) {
        processors.forEach(processor => {
          value = processor(value, context);
        });
      }

      if (value) {
        if (value.trim) {
          return value.trim();
        }
        return value;
      }
    }
  };
}

const metadataRules = {
  description: {
    rules: [
      ['meta[property="og:description"]', node => node.element.getAttribute('content')],
      ['meta[name="description"]', node => node.element.getAttribute('content')],
    ],
  },

  icon_url: {
    rules: [
      ['link[rel="apple-touch-icon"]', node => node.element.getAttribute('href')],
      ['link[rel="apple-touch-icon-precomposed"]', node => node.element.getAttribute('href')],
      ['link[rel="icon"]', node => node.element.getAttribute('href')],
      ['link[rel="fluid-icon"]', node => node.element.getAttribute('href')],
      ['link[rel="shortcut icon"]', node => node.element.getAttribute('href')],
      ['link[rel="Shortcut Icon"]', node => node.element.getAttribute('href')],
      ['link[rel="mask-icon"]', node => node.element.getAttribute('href')],
    ],
    processors: [
      (icon_url, context) => makeUrlAbsolute(context, icon_url)
    ]
  },

  image_url: {
    rules: [
      ['meta[property="og:image:secure_url"]', node => node.element.getAttribute('content')],
      ['meta[property="og:image:url"]', node => node.element.getAttribute('content')],
      ['meta[property="og:image"]', node => node.element.getAttribute('content')],
      ['meta[property="twitter:image"]', node => node.element.getAttribute('content')],
      ['meta[name="thumbnail"]', node => node.element.getAttribute('content')],
    ],
    processors: [
      (image_url, context) => makeUrlAbsolute(context, image_url)
    ],
  },

  keywords: {
    rules: [
      ['meta[name="keywords"]', node => node.element.getAttribute('content')],
    ],
    processors: [
      (keywords) => keywords.split(',').map((keyword) => keyword.trim()),
    ]
  },

  title: {
    rules: [
      ['meta[property="og:title"]', node => node.element.getAttribute('content')],
      ['meta[property="twitter:title"]', node => node.element.getAttribute('content')],
      ['meta[name="hdl"]', node => node.element.getAttribute('content')],
      ['title', node => node.element.text],
    ],
  },

  type: {
    rules: [
      ['meta[property="og:type"]', node => node.element.getAttribute('content')],
    ],
  },

  url: {
    rules: [
      ['meta[property="og:url"]', node => node.element.getAttribute('content')],
      ['link[rel="canonical"]', node => node.element.getAttribute('href')],
    ],
  },

  provider: {
    rules: [
      ['meta[property="og:site_name"]', node => node.element.getAttribute('content')]
    ]
  },
};

function getMetadata(doc, url, rules) {
  const metadata = {};
  const context = {url,doc};
  const ruleSet = rules || metadataRules;

  Object.keys(ruleSet).map(metadataKey => {
    const metadataRule = ruleSet[metadataKey];

    if(Array.isArray(metadataRule.rules)) {
      const builtRule = buildRuleset(metadataKey, metadataRule.rules, metadataRule.processors);
      metadata[metadataKey] = builtRule(doc, context);
    } else {
      metadata[metadataKey] = getMetadata(doc, url, metadataRule);
    }
  });

  return metadata;
}

// #################################################################################################
// # Fathom dependencies resolved
// #################################################################################################

// const {forEach} = require('wu');
function forEach(fn, obj) {
    for (let x of obj) {
        fn(x);
    }
}

function best(iterable, by, isBetter) {
    let bestSoFar, bestKeySoFar;
    let isFirst = true;
    forEach(
        function (item) {
            const key = by(item);
            if (isBetter(key, bestKeySoFar) || isFirst) {
                bestSoFar = item;
                bestKeySoFar = key;
                isFirst = false;
            }
        },
        iterable);
    if (isFirst) {
        throw new Error('Tried to call best() on empty iterable');
    }
    return bestSoFar;
}

// const {max} = require('./utils');
function max(iterable, by = identity) {
    return best(iterable, by, (a, b) => a > b);
}

// #################################################################################################
// # Fathom
// # https://github.com/mozilla/fathom
// # cac59e470816f17fc1efd4a34437b585e3e451cd
// #################################################################################################

// Get a key of a map, first setting it to a default value if it's missing.
function getDefault(map, key, defaultMaker) {
    if (map.has(key)) {
        return map.get(key);
    }
    const defaultValue = defaultMaker();
    map.set(key, defaultValue);
    return defaultValue;
}


// Construct a filtration network of rules.
function ruleset(...rules) {
    const rulesByInputFlavor = new Map();  // [someInputFlavor: [rule, ...]]

    // File each rule under its input flavor:
    forEach(rule => getDefault(rulesByInputFlavor, rule.source.inputFlavor, () => []).push(rule),
            rules);

    return {
        // Iterate over a DOM tree or subtree, building up a knowledgebase, a
        // data structure holding scores and annotations for interesting
        // elements. Return the knowledgebase.
        //
        // This is the "rank" portion of the rank-and-yank algorithm.
        score: function (tree) {
            const kb = knowledgebase();

            // Introduce the whole DOM into the KB as flavor 'dom' to get
            // things started:
            const nonterminals = [[{tree}, 'dom']];  // [[node, flavor], [node, flavor], ...]

            // While there are new facts, run the applicable rules over them to
            // generate even newer facts. Repeat until everything's fully
            // digested. Rules run in no particular guaranteed order.
            while (nonterminals.length) {
                const [inNode, inFlavor] = nonterminals.pop();
                for (let rule of getDefault(rulesByInputFlavor, inFlavor, () => [])) {
                    const outFacts = resultsOf(rule, inNode, inFlavor, kb);
                    for (let fact of outFacts) {
                        const outNode = kb.nodeForElement(fact.element);

                        // No matter whether or not this flavor has been
                        // emitted before for this node, we multiply the score.
                        // We want to be able to add rules that refine the
                        // scoring of a node, without having to rewire the path
                        // of flavors that winds through the ruleset.
                        //
                        // 1 score per Node is plenty. That simplifies our
                        // data, our rankers, our flavor system (since we don't
                        // need to represent score axes), and our engine. If
                        // somebody wants more score axes, they can fake it
                        // themselves with notes, thus paying only for what
                        // they eat. (We can even provide functions that help
                        // with that.) Most rulesets will probably be concerned
                        // with scoring only 1 thing at a time anyway. So,
                        // rankers return a score multiplier + 0 or more new
                        // flavors with optional notes. Facts can never be
                        // deleted from the KB by rankers (or order would start
                        // to matter); after all, they're *facts*.
                        outNode.score *= fact.score;

                        // Add a new annotation to a node--but only if there
                        // wasn't already one of the given flavor already
                        // there; otherwise there's no point.
                        //
                        // You might argue that we might want to modify an
                        // existing note here, but that would be a bad
                        // idea. Notes of a given flavor should be
                        // considered immutable once laid down. Otherwise, the
                        // order of execution of same-flavored rules could
                        // matter, hurting pluggability. Emit a new flavor and
                        // a new note if you want to do that.
                        //
                        // Also, choosing not to add a new fact to nonterminals
                        // when we're not adding a new flavor saves the work of
                        // running the rules against it, which would be
                        // entirely redundant and perform no new work (unless
                        // the rankers were nondeterministic, but don't do
                        // that).
                        if (!outNode.flavors.has(fact.flavor)) {
                            outNode.flavors.set(fact.flavor, fact.notes);
                            kb.indexNodeByFlavor(outNode, fact.flavor);  // TODO: better encapsulation rather than indexing explicitly
                            nonterminals.push([outNode, fact.flavor]);
                        }
                    }
                }
            }
            return kb;
        }
    };
}


// Construct a container for storing and querying facts, where a fact has a
// flavor (used to dispatch further rules upon), a corresponding DOM element, a
// score, and some other arbitrary notes opaque to fathom.
function knowledgebase() {
    const nodesByFlavor = new Map();  // Map{'texty' -> [NodeA],
                                      //     'spiffy' -> [NodeA, NodeB]}
                                      // NodeA = {element: <someElement>,
                                      //
                                      //          // Global nodewide score. Add
                                      //          // custom ones with notes if
                                      //          // you want.
                                      //          score: 8,
                                      //
                                      //          // Flavors is a map of flavor names to notes:
                                      //          flavors: Map{'texty' -> {ownText: 'blah',
                                      //                                   someOtherNote: 'foo',
                                      //                                   someCustomScore: 10},
                                      //                       // This is an empty note:
                                      //                       'fluffy' -> undefined}}
    const nodesByElement = new Map();

    return {
        // Return the "node" (our own data structure that we control) that
        // corresponds to a given DOM element, creating one if necessary.
        nodeForElement: function (element) {
            return getDefault(nodesByElement,
                              element,
                              () => ({element,
                                      score: 1,
                                      flavors: new Map()}));
        },

        // Return the highest-scored node of the given flavor, undefined if
        // there is none.
        max: function (flavor) {
            const nodes = nodesByFlavor.get(flavor);
            return nodes === undefined ? undefined : max(nodes, node => node.score);
        },

        // Let the KB know that a new flavor has been added to an element.
        indexNodeByFlavor: function (node, flavor) {
            getDefault(nodesByFlavor, flavor, () => []).push(node);
        },

        nodesOfFlavor: function (flavor) {
            return getDefault(nodesByFlavor, flavor, () => []);
        }
    };
}


// Apply a rule (as returned by a call to rule()) to a fact, and return the
// new facts that result.
function resultsOf(rule, node, flavor, kb) {
    // If more types of rule pop up someday, do fancier dispatching here.
    return rule.source.flavor === 'flavor' ? resultsOfFlavorRule(rule, node, flavor) : resultsOfDomRule(rule, node, kb);
}


// Pull the DOM tree off the special property of the root "dom" fact, and query
// against it.
function *resultsOfDomRule(rule, specialDomNode, kb) {
    // Use the special "tree" property of the special starting node:
    const matches = specialDomNode.tree.querySelectorAll(rule.source.selector);

    for (let i = 0; i < matches.length; i++) {  // matches is a NodeList, which doesn't conform to iterator protocol
        const element = matches[i];
        const newFacts = explicitFacts(rule.ranker(kb.nodeForElement(element)));
        for (let fact of newFacts) {
            if (fact.element === undefined) {
                fact.element = element;
            }
            if (fact.flavor === undefined) {
                throw new Error('Rankers of dom() rules must return a flavor in each fact. Otherwise, there is no way for that fact to be used later.');
            }
            yield fact;
        }
    }
}


function *resultsOfFlavorRule(rule, node, flavor) {
    const newFacts = explicitFacts(rule.ranker(node));

    for (let fact of newFacts) {
        // If the ranker didn't specify a different element, assume it's
        // talking about the one we passed in:
        if (fact.element === undefined) {
            fact.element = node.element;
        }
        if (fact.flavor === undefined) {
            fact.flavor = flavor;
        }
        yield fact;
    }
}


// Take the possibly abbreviated output of a ranker function, and make it
// explicitly an iterable with a defined score.
//
// Rankers can return undefined, which means "no facts", a single fact, or an
// array of facts.
function *explicitFacts(rankerResult) {
    const array = (rankerResult === undefined) ? [] : (Array.isArray(rankerResult) ? rankerResult : [rankerResult]);
    for (let fact of array) {
        if (fact.score === undefined) {
            fact.score = 1;
        }
        yield fact;
    }
}


// TODO: For the moment, a lot of responsibility is on the rankers to return a
// pretty big data structure of up to 4 properties. This is a bit verbose for
// an arrow function (as I hope we can use most of the time) and the usual case
// will probably be returning just a score multiplier. Make that case more
// concise.

// TODO: It is likely that rankers should receive the notes of their input type
// as a 2nd arg, for brevity.


// Return a condition that uses a DOM selector to find its matches from the
// original DOM tree.
//
// For consistency, Nodes will still be delivered to the transformers, but
// they'll have empty flavors and score = 1.
//
// Condition constructors like dom() and flavor() build stupid, introspectable
// objects that the query engine can read. They don't actually do the query
// themselves. That way, the query planner can be smarter than them, figuring
// out which indices to use based on all of them. (We'll probably keep a heap
// by each dimension's score and a hash by flavor name, for starters.) Someday,
// fancy things like this may be possible: rule(and(tag('p'), klass('snork')),
// ...)
function dom(selector) {
    return {
        flavor: 'dom',
        inputFlavor: 'dom',
        selector
    };
}


// Return a condition that discriminates on nodes of the knowledgebase by flavor.
function flavor(inputFlavor) {
    return {
        flavor: 'flavor',
        inputFlavor
    };
}


function rule(source, ranker) {
    return {
        source,
        ranker
    };
}
