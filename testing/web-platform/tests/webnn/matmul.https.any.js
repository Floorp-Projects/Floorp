// META: title=test WebNN API matmul operation
// META: global=window,dedicatedworker
// META: script=./resources/utils.js
// META: timeout=long

'use strict';

// https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-matmul

const buildMatmul= (operationName, builder, resources) => {
  // MLOperand matmul(MLOperand a, MLOperand b);
  const namedOutputOperand = {};
  const [inputOperandA, inputOperandB] = createMultiInputOperands(builder, resources);
  namedOutputOperand[resources.expected.name] = builder[operationName](inputOperandA, inputOperandB);
  return namedOutputOperand;
};

testWebNNOperation('matmul', '/webnn/resources/test_data/matmul.json', buildMatmul);