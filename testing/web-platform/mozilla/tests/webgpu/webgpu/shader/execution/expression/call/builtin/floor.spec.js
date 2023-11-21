/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for the 'floor' builtin function

S is AbstractFloat, f32, f16
T is S or vecN<S>
@const fn floor(e: T ) -> T
Returns the floor of e. Component-wise when T is a vector.
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { TypeF32, TypeF16, TypeAbstractFloat } from '../../../../../util/conversion.js';
import { FP } from '../../../../../util/floating_point.js';
import { fullF32Range, fullF16Range, fullF64Range } from '../../../../../util/math.js';
import { makeCaseCache } from '../../case_cache.js';
import { allInputSources, onlyConstInputSource, run } from '../../expression.js';

import { abstractBuiltin, builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

const kSmallMagnitudeTestValues = [0.1, 0.9, 1.0, 1.1, 1.9, -0.1, -0.9, -1.0, -1.1, -1.9];

export const d = makeCaseCache('floor', {
  f32: () => {
    return FP.f32.generateScalarToIntervalCases(
      [
      ...kSmallMagnitudeTestValues,
      ...fullF32Range(),
      0x8000_0000 // https://github.com/gpuweb/cts/issues/2766
      ],
      'unfiltered',
      FP.f32.floorInterval
    );
  },
  f16: () => {
    return FP.f16.generateScalarToIntervalCases(
      [
      ...kSmallMagnitudeTestValues,
      ...fullF16Range(),
      0x8000 // https://github.com/gpuweb/cts/issues/2766
      ],
      'unfiltered',
      FP.f16.floorInterval
    );
  },
  abstract: () => {
    return FP.abstract.generateScalarToIntervalCases(
      [
      ...kSmallMagnitudeTestValues,
      ...fullF64Range(),
      0x8000_0000_0000_0000 // https://github.com/gpuweb/cts/issues/2766
      ],
      'unfiltered',
      FP.abstract.floorInterval
    );
  }
});

g.test('abstract_float').
specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions').
desc(`abstract float tests`).
params((u) =>
u.
combine('inputSource', onlyConstInputSource).
combine('vectorize', [undefined, 2, 3, 4])
).
fn(async (t) => {
  const cases = await d.get('abstract');
  await run(t, abstractBuiltin('floor'), [TypeAbstractFloat], TypeAbstractFloat, t.params, cases);
});

g.test('f32').
specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions').
desc(`f32 tests`).
params((u) =>
u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4])
).
fn(async (t) => {
  const cases = await d.get('f32');
  await run(t, builtin('floor'), [TypeF32], TypeF32, t.params, cases);
});

g.test('f16').
specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions').
desc(`f16 tests`).
params((u) =>
u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4])
).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase('shader-f16');
}).
fn(async (t) => {
  const cases = await d.get('f16');
  await run(t, builtin('floor'), [TypeF16], TypeF16, t.params, cases);
});