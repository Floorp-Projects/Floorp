'use strict';
(function() {
  var interpolationTests = [];
  var compositionTests = [];
  var cssAnimationsData = {
    sharedStyle: null,
    nextID: 0,
  };
  var expectNoInterpolation = {};
  var neutralKeyframe = {};
  function isNeutralKeyframe(keyframe) {
    return keyframe === neutralKeyframe;
  }

  // For all the CSS interpolation methods, we set the animation duration very
  // very long so that any advancement in time that does happen is irrelevant
  // in terms of the progress value. In particular, the animation duration is
  // 2e9s which is ~63 years.
  // We then set the delay to be *negative* half the duration, so we are
  // immediately at the halfway point of the animation. Finally, we using an
  // easing function that maps halfway (0.5) to whatever progress we actually
  // want.
  var cssAnimationsInterpolation = {
    name: 'CSS Animations',
    supportsProperty: function() {return true;},
    supportsValue: function() {return true;},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      var id = cssAnimationsData.nextID++;
      if (!cssAnimationsData.sharedStyle) {
        cssAnimationsData.sharedStyle = createElement(document.body, 'style');
      }
      cssAnimationsData.sharedStyle.textContent += '' +
        '@keyframes animation' + id + ' {' +
          (isNeutralKeyframe(from) ? '' : `from {${property}:${from};}`) +
          (isNeutralKeyframe(to) ? '' : `to {${property}:${to};}`) +
        '}';
      target.style.animationName = 'animation' + id;
      target.style.animationDuration = '2e9s';
      target.style.animationDelay = '-1e9s';
      target.style.animationTimingFunction = createEasing(at);
    },
  };

  var cssTransitionsInterpolation = {
    name: 'CSS Transitions',
    supportsProperty: function() {return true;},
    supportsValue: function() {return true;},
    setup: function(property, from, target) {
      target.style.setProperty(property, isNeutralKeyframe(from) ? '' : from);
    },
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, -Infinity);
    },
    interpolate: function(property, from, to, at, target) {
      // Force a style recalc on target to set the 'from' value.
      getComputedStyle(target).left;
      target.style.transitionDuration = '2e9s';
      target.style.transitionDelay = '-1e9s';
      target.style.transitionTimingFunction = createEasing(at);
      target.style.transitionProperty = property;
      target.style.setProperty(property, isNeutralKeyframe(to) ? '' : to);
    },
  };

  var cssTransitionAllInterpolation = {
    name: 'CSS Transitions with transition: all',
    // The 'all' value doesn't cover custom properties.
    supportsProperty: function(property) {return property.indexOf('--') !== 0;},
    supportsValue: function() {return true;},
    setup: function(property, from, target) {
      target.style.setProperty(property, isNeutralKeyframe(from) ? '' : from);
    },
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, -Infinity);
    },
    interpolate: function(property, from, to, at, target) {
      // Force a style recalc on target to set the 'from' value.
      getComputedStyle(target).left;
      target.style.transitionDuration = '2e9s';
      target.style.transitionDelay = '-1e9s';
      target.style.transitionTimingFunction = createEasing(at);
      target.style.transitionProperty = 'all';
      target.style.setProperty(property, isNeutralKeyframe(to) ? '' : to);
    },
  };

  var webAnimationsInterpolation = {
    name: 'Web Animations',
    supportsProperty: function(property) {return true;},
    supportsValue: function(value) {return value !== '';},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      this.interpolateComposite(property, from, 'replace', to, 'replace', at, target);
    },
    interpolateComposite: function(property, from, fromComposite, to, toComposite, at, target) {
      // Convert standard properties to camelCase.
      if (!property.startsWith('--')) {
        for (var i = property.length - 2; i > 0; --i) {
          if (property[i] === '-') {
            property = property.substring(0, i) + property[i + 1].toUpperCase() + property.substring(i + 2);
          }
        }
        if (property === 'offset')
          property = 'cssOffset';
      }
      var keyframes = [];
      if (!isNeutralKeyframe(from)) {
        keyframes.push({
          offset: 0,
          composite: fromComposite,
          [property]: from,
        });
      }
      if (!isNeutralKeyframe(to)) {
        keyframes.push({
          offset: 1,
          composite: toComposite,
          [property]: to,
        });
      }
      var animation = target.animate(keyframes, {
        fill: 'forwards',
        duration: 1,
        easing: createEasing(at),
      });
      animation.pause();
      animation.currentTime = 0.5;
    },
  };

  function expectFlip(from, to, flipAt) {
    return [-0.3, 0, 0.3, 0.5, 0.6, 1, 1.5].map(function(at) {
      return {
        at: at,
        expect: at < flipAt ? from : to
      };
    });
  }

  // Constructs a timing function which produces 'y' at x = 0.5
  function createEasing(y) {
    if (y == 0) {
      return 'steps(1, end)';
    }
    if (y == 1) {
      return 'steps(1, start)';
    }
    if (y == 0.5) {
      return 'steps(2, end)';
    }
    // Approximate using a bezier.
    var b = (8 * y - 1) / 6;
    return 'cubic-bezier(0, ' + b + ', 1, ' + b + ')';
  }

  function createElement(parent, tag, text) {
    var element = document.createElement(tag || 'div');
    element.textContent = text || '';
    parent.appendChild(element);
    return element;
  }

  function createTargetContainer(parent, className) {
    var targetContainer = createElement(parent);
    targetContainer.classList.add('container');
    var template = document.querySelector('#target-template');
    if (template) {
      targetContainer.appendChild(template.content.cloneNode(true));
    }
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.classList.add('target', className);
    target.parentElement.classList.add('parent');
    targetContainer.target = target;
    return targetContainer;
  }

  function roundNumbers(value) {
    return value.
        // Round numbers to two decimal places.
        replace(/-?\d*\.\d+(e-?\d+)?/g, function(n) {
          return (parseFloat(n).toFixed(2)).
              replace(/\.\d+/, function(m) {
                return m.replace(/0+$/, '');
              }).
              replace(/\.$/, '').
              replace(/^-0$/, '0');
        });
  }

  var anchor = document.createElement('a');
  function sanitizeUrls(value) {
    var matches = value.match(/url\("([^#][^\)]*)"\)/g);
    if (matches !== null) {
      for (var i = 0; i < matches.length; ++i) {
        var url = /url\("([^#][^\)]*)"\)/g.exec(matches[i])[1];
        anchor.href = url;
        anchor.pathname = '...' + anchor.pathname.substring(anchor.pathname.lastIndexOf('/'));
        value = value.replace(matches[i], 'url(' + anchor.href + ')');
      }
    }
    return value;
  }

  function normalizeValue(value) {
    return roundNumbers(sanitizeUrls(value)).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function stringify(text) {
    if (!text.includes("'")) {
      return `'${text}'`;
    }
    return `"${text.replace('"', '\\"')}"`;
  }

  function keyframeText(keyframe) {
    return isNeutralKeyframe(keyframe) ? 'neutral' : `[${keyframe}]`;
  }

  function keyframeCode(keyframe) {
    return isNeutralKeyframe(keyframe) ? 'neutralKeyframe' : `${stringify(keyframe)}`;
  }

  function createInterpolationTestTargets(interpolationMethod, interpolationMethodContainer, interpolationTest) {
    var property = interpolationTest.options.property;
    var from = interpolationTest.options.from;
    var to = interpolationTest.options.to;
    if ((interpolationTest.options.method && interpolationTest.options.method != interpolationMethod.name)
      || !interpolationMethod.supportsProperty(property)
      || !interpolationMethod.supportsValue(from)
      || !interpolationMethod.supportsValue(to)) {
      return;
    }
    var testText = `${interpolationMethod.name}: property <${property}> from ${keyframeText(from)} to ${keyframeText(to)}`;
    var testContainer = createElement(interpolationMethodContainer, 'div');
    createElement(testContainer);
    var expectations = interpolationTest.expectations;
    if (expectations === expectNoInterpolation) {
      expectations = interpolationMethod.nonInterpolationExpectations(from, to);
    }
    return expectations.map(function(expectation) {
      var actualTargetContainer = createTargetContainer(testContainer, 'actual');
      var expectedTargetContainer = createTargetContainer(testContainer, 'expected');
      if (!isNeutralKeyframe(expectation.expect)) {
        expectedTargetContainer.target.style.setProperty(property, expectation.expect);
      }
      var target = actualTargetContainer.target;
      interpolationMethod.setup(property, from, target);
      target.interpolate = function() {
        interpolationMethod.interpolate(property, from, to, expectation.at, target);
      };
      target.measure = function() {
        var actualValue = getComputedStyle(target).getPropertyValue(property);
        test(function() {
          assert_equals(
            normalizeValue(actualValue),
            normalizeValue(getComputedStyle(expectedTargetContainer.target).getPropertyValue(property)));
        }, `${testText} at (${expectation.at}) is [${sanitizeUrls(actualValue)}]`);
      };
      return target;
    });
  }

  function createCompositionTestTargets(compositionContainer, compositionTest) {
    var options = compositionTest.options;
    var property = options.property;
    var underlying = options.underlying;
    var from = options.addFrom || options.replaceFrom;
    var to = options.addTo || options.replaceTo;
    var fromComposite = 'addFrom' in options ? 'add' : 'replace';
    var toComposite = 'addTo' in options ? 'add' : 'replace';
    if ('addFrom' in options === 'replaceFrom' in options
      || 'addTo' in options === 'replaceTo' in options) {
      test(function() {
        assert_true('addFrom' in options !== 'replaceFrom' in options, 'addFrom xor replaceFrom must be specified');
        assert_true('addTo' in options !== 'replaceTo' in options, 'addTo xor replaceTo must be specified');
      }, `Composition tests must use addFrom xor replaceFrom, and addTo xor replaceTo`);
    }
    validateTestInputs(property, from, to, underlying);

    var testText = `Compositing: property <${property}> underlying [${underlying}] from ${fromComposite} [${from}] to ${toComposite} [${to}]`;
    var testContainer = createElement(compositionContainer, 'div');
    createElement(testContainer, 'br');
    return compositionTest.expectations.map(function(expectation) {
      var actualTargetContainer = createTargetContainer(testContainer, 'actual');
      var expectedTargetContainer = createTargetContainer(testContainer, 'expected');
      expectedTargetContainer.target.style.setProperty(property, expectation.expect);
      var target = actualTargetContainer.target;
      target.style.setProperty(property, underlying);
      target.interpolate = function() {
        webAnimationsInterpolation.interpolateComposite(property, from, fromComposite, to, toComposite, expectation.at, target);
      };
      target.measure = function() {
        var actualValue = getComputedStyle(target).getPropertyValue(property);
        test(function() {
          assert_equals(
            normalizeValue(actualValue),
            normalizeValue(getComputedStyle(expectedTargetContainer.target).getPropertyValue(property)));
        }, `${testText} at (${expectation.at}) is [${sanitizeUrls(actualValue)}]`);
      };
      return target;
    });
  }

  function validateTestInputs(property, from, to, underlying) {
    if (from && from !== neutralKeyframe && !CSS.supports(property, from)) {
        test(function() {
          assert_unreached('from value not supported');
        }, `${property} supports [${from}]`);
    }
    if (to && to !== neutralKeyframe && !CSS.supports(property, to)) {
        test(function() {
          assert_unreached('to value not supported');
        }, `${property} supports [${to}]`);
    }
    if (typeof underlying !== 'undefined' && !CSS.supports(property, underlying)) {
        test(function() {
          assert_unreached('underlying value not supported');
        }, `${property} supports [${underlying}]`);
    }
  }

  function createTestTargets(interpolationMethods, interpolationTests, compositionTests, container) {
    var targets = [];
    for (var interpolationTest of interpolationTests) {
      validateTestInputs(interpolationTest.options.property, interpolationTest.options.from, interpolationTest.options.to);
    }
    for (var interpolationMethod of interpolationMethods) {
      var interpolationMethodContainer = createElement(container);
      for (var interpolationTest of interpolationTests) {
        [].push.apply(targets, createInterpolationTestTargets(interpolationMethod, interpolationMethodContainer, interpolationTest));
      }
    }
    var compositionContainer = createElement(container);
    for (var compositionTest of compositionTests) {
      [].push.apply(targets, createCompositionTestTargets(compositionContainer, compositionTest));
    }
    return targets;
  }

  function test_no_interpolation(options) {
    test_interpolation(options, expectNoInterpolation);
  }

  function test_interpolation(options, expectations) {
    interpolationTests.push({options, expectations});
    var interpolationMethods = [
      cssTransitionsInterpolation,
      cssTransitionAllInterpolation,
      cssAnimationsInterpolation,
      webAnimationsInterpolation,
    ];
    var container = createElement(document.body);
    var targets = createTestTargets(interpolationMethods, interpolationTests, compositionTests, container);
    // Separate interpolation and measurement into different phases to avoid O(n^2) of the number of targets.
    for (var target of targets) {
      target.interpolate();
    }
    for (var target of targets) {
      target.measure();
    }
    container.remove();
    interpolationTests = [];
  }
  window.test_interpolation = test_interpolation;
  window.test_no_interpolation = test_no_interpolation;
  window.neutralKeyframe = neutralKeyframe;
})();
