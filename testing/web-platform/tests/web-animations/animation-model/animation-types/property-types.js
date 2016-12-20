const discreteType = {
  testInterpolation: function(property, setup, options) {
    options.forEach(function(keyframes) {
      var [ from, to ] = keyframes;
      test(function(t) {
        var idlName = propertyToIDL(property);
        var target = createTestElement(t, setup);
        var animation = target.animate({ [idlName]: [from, to] },
                                       { duration: 1000, fill: 'both' });
        testAnimationSamples(animation, idlName,
                             [{ time: 0,    expected: from.toLowerCase() },
                              { time: 499,  expected: from.toLowerCase() },
                              { time: 500,  expected: to.toLowerCase() },
                              { time: 1000, expected: to.toLowerCase() }]);
      }, property + ' uses discrete animation when animating between "'
         + from + '" and "' + to + '" with linear easing');

      test(function(t) {
        // Easing: http://cubic-bezier.com/#.68,0,1,.01
        // With this curve, we don't reach the 50% point until about 95% of
        // the time has expired.
        var idlName = propertyToIDL(property);
        var keyframes = {};
        var target = createTestElement(t, setup);
        var animation = target.animate({ [idlName]: [from, to] },
                                       { duration: 1000, fill: 'both',
                                         easing: 'cubic-bezier(0.68,0,1,0.01)' });
        testAnimationSamples(animation, idlName,
                             [{ time: 0,    expected: from.toLowerCase() },
                              { time: 940,  expected: from.toLowerCase() },
                              { time: 960,  expected: to.toLowerCase() }]);
      }, property + ' uses discrete animation when animating between "'
         + from + '" and "' + to + '" with effect easing');

      test(function(t) {
        // Easing: http://cubic-bezier.com/#.68,0,1,.01
        // With this curve, we don't reach the 50% point until about 95% of
        // the time has expired.
        var idlName = propertyToIDL(property);
        var target = createTestElement(t, setup);
        var animation = target.animate({ [idlName]: [from, to],
                                         easing: 'cubic-bezier(0.68,0,1,0.01)' },
                                       { duration: 1000, fill: 'both' });
        testAnimationSamples(animation, idlName,
                             [{ time: 0,    expected: from.toLowerCase() },
                              { time: 940,  expected: from.toLowerCase() },
                              { time: 960,  expected: to.toLowerCase() }]);
      }, property + ' uses discrete animation when animating between "'
         + from + '" and "' + to + '" with keyframe easing');
    });
  },
};

const lengthType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['10px', '50px'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10px' },
                            { time: 500,  expected: '30px' },
                            { time: 1000, expected: '50px' }]);
    }, property + ' supports animating as a length');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['1rem', '5rem'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10px' },
                            { time: 500,  expected: '30px' },
                            { time: 1000, expected: '50px' }]);
    }, property + ' supports animating as a length of rem');
  },
};

const percentageType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['10%', '50%'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10%' },
                            { time: 500,  expected: '30%' },
                            { time: 1000, expected: '50%' }]);
    }, property + ' supports animating as a percentage');
  },
};

const integerType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: [-2, 2] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '-2' },
                            { time: 500,  expected: '0' },
                            { time: 1000, expected: '2' }]);
    }, property + ' supports animating as an integer');
  },
};

const lengthPercentageOrCalcType = {
  testInterpolation: function(property, setup) {
    lengthType.testInterpolation(property, setup);
    percentageType.testInterpolation(property, setup);

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['10px', '20%'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10px' },
                            { time: 500,  expected: 'calc(5px + 10%)' },
                            { time: 1000, expected: '20%' }]);
    }, property + ' supports animating as combination units "px" and "%"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['10%', '2em'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10%' },
                            { time: 500,  expected: 'calc(10px + 5%)' },
                            { time: 1000, expected: '20px' }]);
    }, property + ' supports animating as combination units "%" and "em"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['1em', '2rem'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10px' },
                            { time: 500,  expected: '15px' },
                            { time: 1000, expected: '20px' }]);
    }, property + ' supports animating as combination units "em" and "rem"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['10px', 'calc(1em + 20%)'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '10px' },
                            { time: 500,  expected: 'calc(10px + 10%)' },
                            { time: 1000, expected: 'calc(10px + 20%)' }]);
    }, property + ' supports animating as combination units "px" and "calc"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate(
        { [idlName]: ['calc(10px + 10%)', 'calc(1em + 1rem + 20%)'] },
        { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,
                              expected: 'calc(10px + 10%)' },
                            { time: 500,
                              expected: 'calc(15px + 15%)' },
                            { time: 1000,
                              expected: 'calc(20px + 20%)' }]);
    }, property + ' supports animating as a calc');
  },
};

const positiveNumberType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: [1.1, 1.5] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: '1.1' },
                            { time: 500,  expected: '1.3' },
                            { time: 1000, expected: '1.5' }]);
    }, property + ' supports animating as a positive number');
  },
};

const visibilityType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['visible', 'hidden'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: 'visible' },
                            { time: 999,  expected: 'visible' },
                            { time: 1000, expected: 'hidden' }]);
    }, property + ' uses visibility animation when animating '
       + 'from "visible" to "hidden"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['hidden', 'visible'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: 'hidden' },
                            { time: 1,    expected: 'visible' },
                            { time: 1000, expected: 'visible' }]);
    }, property + ' uses visibility animation when animating '
     + 'from "hidden" to "visible"');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['hidden', 'collapse'] },
                                     { duration: 1000, fill: 'both' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: 'hidden' },
                            { time: 499,  expected: 'hidden' },
                            { time: 500,  expected: 'collapse' },
                            { time: 1000, expected: 'collapse' }]);
    }, property + ' uses visibility animation when animating '
     + 'from "hidden" to "collapse"');

    test(function(t) {
      // Easing: http://cubic-bezier.com/#.68,-.55,.26,1.55
      // With this curve, the value is less than 0 till about 34%
      // also more than 1 since about 63%
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation =
        target.animate({ [idlName]: ['visible', 'hidden'] },
                       { duration: 1000, fill: 'both',
                         easing: 'cubic-bezier(0.68, -0.55, 0.26, 1.55)' });
      testAnimationSamples(animation, idlName,
                           [{ time: 0,    expected: 'visible' },
                            { time: 1,    expected: 'visible' },
                            { time: 330,  expected: 'visible' },
                            { time: 340,  expected: 'visible' },
                            { time: 620,  expected: 'visible' },
                            { time: 630,  expected: 'hidden' },
                            { time: 1000, expected: 'hidden' }]);
    }, property + ' uses visibility animation when animating '
     + 'from "visible" to "hidden" with easeInOutBack easing');
  },
};

const colorType = {
  testInterpolation: function(property, setup) {
    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['rgb(255, 0, 0)',
                                                   'rgb(0, 0, 255)'] },
                                     1000);
      testAnimationSamples(animation, idlName,
                           [{ time: 500,  expected: 'rgb(128, 0, 128)' }]);
    }, property + ' supports animating as color of rgb()');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['#ff0000', '#0000ff'] },
                                     1000);
      testAnimationSamples(animation, idlName,
                           [{ time: 500,  expected: 'rgb(128, 0, 128)' }]);
    }, property + ' supports animating as color of #RGB');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['hsl(0,   100%, 50%)',
                                                   'hsl(240, 100%, 50%)'] },
                                     1000);
      testAnimationSamples(animation, idlName,
                           [{ time: 500,  expected: 'rgb(128, 0, 128)' }]);
    }, property + ' supports animating as color of hsl()');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['#ff000066', '#0000ffcc'] },
                                     1000);
                                             // R: 255 * (0.4 * 0.5) / 0.6 = 85
                                             // G: 255 * (0.8 * 0.5) / 0.6 = 170
      testAnimationSamples(animation, idlName,
                           [{ time: 500,  expected: 'rgba(85, 0, 170, 0.6)' }]);
    }, property + ' supports animating as color of #RGBa');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['rgba(255, 0, 0, 0.4)',
                                                   'rgba(0, 0, 255, 0.8)'] },
                                     1000);
      testAnimationSamples(animation, idlName,      // Same as above.
                           [{ time: 500,  expected: 'rgba(85, 0, 170, 0.6)' }]);
    }, property + ' supports animating as color of rgba()');

    test(function(t) {
      var idlName = propertyToIDL(property);
      var target = createTestElement(t, setup);
      var animation = target.animate({ [idlName]: ['hsla(0,   100%, 50%, 0.4)',
                                                   'hsla(240, 100%, 50%, 0.8)'] },
                                     1000);
      testAnimationSamples(animation, idlName,      // Same as above.
                           [{ time: 500,  expected: 'rgba(85, 0, 170, 0.6)' }]);
    }, property + ' supports animating as color of hsla()');
  },
};

const types = {
  color: colorType,
  discrete: discreteType,
  integer: integerType,
  length: lengthType,
  percentage: percentageType,
  lengthPercentageOrCalc: lengthPercentageOrCalcType,
  positiveNumber: positiveNumberType,
  visibility: visibilityType,
};

