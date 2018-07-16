/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "av1/encoder/ml.h"

void av1_nn_predict(const float *features, const NN_CONFIG *nn_config,
                    float *output) {
  int num_input_nodes = nn_config->num_inputs;
  int buf_index = 0;
  float buf[2][NN_MAX_NODES_PER_LAYER];
  const float *input_nodes = features;

  // Propagate hidden layers.
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= NN_MAX_HIDDEN_LAYERS);
  for (int layer = 0; layer < num_layers; ++layer) {
    const float *weights = nn_config->weights[layer];
    const float *bias = nn_config->bias[layer];
    float *output_nodes = buf[buf_index];
    const int num_output_nodes = nn_config->num_hidden_nodes[layer];
    assert(num_output_nodes < NN_MAX_NODES_PER_LAYER);
    for (int node = 0; node < num_output_nodes; ++node) {
      float val = 0.0f;
      for (int i = 0; i < num_input_nodes; ++i)
        val += weights[i] * input_nodes[i];
      val += bias[node];
      // ReLU as activation function.
      val = val > 0.0f ? val : 0.0f;  // Could use AOMMAX().
      output_nodes[node] = val;
      weights += num_input_nodes;
    }
    num_input_nodes = num_output_nodes;
    input_nodes = output_nodes;
    buf_index = 1 - buf_index;
  }

  // Final output layer.
  const float *weights = nn_config->weights[num_layers];
  for (int node = 0; node < nn_config->num_outputs; ++node) {
    const float *bias = nn_config->bias[num_layers];
    float val = 0.0f;
    for (int i = 0; i < num_input_nodes; ++i)
      val += weights[i] * input_nodes[i];
    output[node] = val + bias[node];
    weights += num_input_nodes;
  }
}
