/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for non-matrix f16 remainder expression
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { TypeF16, TypeVec } from '../../../../util/conversion.js';
import { FP } from '../../../../util/floating_point.js';
import { sparseF16Range, sparseVectorF16Range } from '../../../../util/math.js';
import { makeCaseCache } from '../case_cache.js';
import { allInputSources, run } from '../expression.js';

import { binary, compoundBinary } from './binary.js';

const remainderVectorScalarInterval = (v, s) => {
  return FP.f16.toVector(v.map(e => FP.f16.remainderInterval(e, s)));
};

const remainderScalarVectorInterval = (s, v) => {
  return FP.f16.toVector(v.map(e => FP.f16.remainderInterval(s, e)));
};

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('binary/f16_remainder', {
  scalar_const: () => {
    return FP.f16.generateScalarPairToIntervalCases(
      sparseF16Range(),
      sparseF16Range(),
      'finite',
      FP.f16.remainderInterval
    );
  },
  scalar_non_const: () => {
    return FP.f16.generateScalarPairToIntervalCases(
      sparseF16Range(),
      sparseF16Range(),
      'unfiltered',
      FP.f16.remainderInterval
    );
  },
  vec2_scalar_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(2),
      sparseF16Range(),
      'finite',
      remainderVectorScalarInterval
    );
  },
  vec2_scalar_non_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(2),
      sparseF16Range(),
      'unfiltered',
      remainderVectorScalarInterval
    );
  },
  vec3_scalar_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(3),
      sparseF16Range(),
      'finite',
      remainderVectorScalarInterval
    );
  },
  vec3_scalar_non_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(3),
      sparseF16Range(),
      'unfiltered',
      remainderVectorScalarInterval
    );
  },
  vec4_scalar_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(4),
      sparseF16Range(),
      'finite',
      remainderVectorScalarInterval
    );
  },
  vec4_scalar_non_const: () => {
    return FP.f16.generateVectorScalarToVectorCases(
      sparseVectorF16Range(4),
      sparseF16Range(),
      'unfiltered',
      remainderVectorScalarInterval
    );
  },
  scalar_vec2_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(2),
      'finite',
      remainderScalarVectorInterval
    );
  },
  scalar_vec2_non_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(2),
      'unfiltered',
      remainderScalarVectorInterval
    );
  },
  scalar_vec3_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(3),
      'finite',
      remainderScalarVectorInterval
    );
  },
  scalar_vec3_non_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(3),
      'unfiltered',
      remainderScalarVectorInterval
    );
  },
  scalar_vec4_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(4),
      'finite',
      remainderScalarVectorInterval
    );
  },
  scalar_vec4_non_const: () => {
    return FP.f16.generateScalarVectorToVectorCases(
      sparseF16Range(),
      sparseVectorF16Range(4),
      'unfiltered',
      remainderScalarVectorInterval
    );
  },
});

g.test('scalar')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x % y, where x and y are scalars
Accuracy: Derived from x - y * trunc(x/y)
`
  )
  .params(u => u.combine('inputSource', allInputSources))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'scalar_const' : 'scalar_non_const'
    );

    await run(t, binary('%'), [TypeF16, TypeF16], TypeF16, t.params, cases);
  });

g.test('vector')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x % y, where x and y are vectors
Accuracy: Derived from x - y * trunc(x/y)
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [2, 3, 4]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'scalar_const' : 'scalar_non_const'
    );

    await run(t, binary('%'), [TypeF16, TypeF16], TypeF16, t.params, cases);
  });

g.test('scalar_compound')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x %= y
Accuracy: Derived from x - y * trunc(x/y)
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'scalar_const' : 'scalar_non_const'
    );

    await run(t, compoundBinary('%='), [TypeF16, TypeF16], TypeF16, t.params, cases);
  });

g.test('vector_scalar')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x % y, where x is a vector and y is a scalar
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('dim', [2, 3, 4]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const dim = t.params.dim;
    const cases = await d.get(
      t.params.inputSource === 'const' ? `vec${dim}_scalar_const` : `vec${dim}_scalar_non_const`
    );

    await run(
      t,
      binary('%'),
      [TypeVec(dim, TypeF16), TypeF16],
      TypeVec(dim, TypeF16),
      t.params,
      cases
    );
  });

g.test('vector_scalar_compound')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x %= y, where x is a vector and y is a scalar
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('dim', [2, 3, 4]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const dim = t.params.dim;
    const cases = await d.get(
      t.params.inputSource === 'const' ? `vec${dim}_scalar_const` : `vec${dim}_scalar_non_const`
    );

    await run(
      t,
      compoundBinary('%='),
      [TypeVec(dim, TypeF16), TypeF16],
      TypeVec(dim, TypeF16),
      t.params,
      cases
    );
  });

g.test('scalar_vector')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x % y, where x is a scalar and y is a vector
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('dim', [2, 3, 4]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase('shader-f16');
  })
  .fn(async t => {
    const dim = t.params.dim;
    const cases = await d.get(
      t.params.inputSource === 'const' ? `scalar_vec${dim}_const` : `scalar_vec${dim}_non_const`
    );

    await run(
      t,
      binary('%'),
      [TypeF16, TypeVec(dim, TypeF16)],
      TypeVec(dim, TypeF16),
      t.params,
      cases
    );
  });
