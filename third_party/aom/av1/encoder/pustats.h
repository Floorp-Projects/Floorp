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

#ifndef AV1_ENCODER_PUSTATS_H_
#define AV1_ENCODER_PUSTATS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/ml.h"

#define NUM_FEATURES 11
#define NUM_HIDDEN_LAYERS 2
#define HIDDEN_LAYERS_0_NODES 12
#define HIDDEN_LAYERS_1_NODES 10
#define LOGITS_NODES 1

static const float
    av1_pustats_rate_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      21.5067f,  22.6709f,  0.0049f,   0.9288f,  -0.0100f,  0.0060f,   -0.0071f,
      -0.0085f,  0.0348f,   -0.1273f,  10.1154f, 6.3405f,   7.8589f,   -0.0652f,
      -4.6352f,  0.0445f,   -3.2748f,  0.1025f,  -0.0385f,  -0.4505f,  1.1320f,
      3.2634f,   23.2420f,  -7.9056f,  0.0522f,  -18.1555f, 0.0977f,   0.1155f,
      -0.0138f,  0.0267f,   -0.3992f,  0.2735f,  22.8063f,  35.1043f,  3.8140f,
      -0.0295f,  0.0771f,   -0.6938f,  0.0302f,  -0.0266f,  0.0989f,   -0.0794f,
      0.2981f,   33.3333f,  -24.1150f, 1.4986f,  -0.0975f,  -15.3938f, -0.0858f,
      -0.0845f,  -0.0869f,  -0.0858f,  0.3542f,  0.0155f,   -18.2629f, 9.6688f,
      -11.9643f, -0.2904f,  -5.3026f,  -0.1011f, -0.1202f,  0.0127f,   -0.0269f,
      0.3434f,   0.0595f,   16.6800f,  41.4730f, 6.9269f,   -0.0512f,  -1.4540f,
      0.0468f,   0.0077f,   0.0983f,   0.1265f,  -0.5234f,  0.9477f,   36.6470f,
      -0.4838f,  -0.2269f,  -0.1143f,  -0.3907f, -0.5005f,  -0.0179f,  -0.1057f,
      0.1233f,   -0.4412f,  -0.0474f,  0.1140f,  -21.6813f, -0.9077f,  -0.0078f,
      -3.3306f,  0.0417f,   0.0412f,   0.0427f,  0.0418f,   -0.1699f,  0.0072f,
      -22.3335f, 16.1203f,  -10.1220f, -0.0019f, 0.0005f,   -0.0054f,  -0.0155f,
      -0.0302f,  -0.0379f,  0.1276f,   0.1568f,  21.6175f,  12.2919f,  11.0327f,
      -0.2000f,  -8.6691f,  -0.5593f,  -0.5952f, -0.4203f,  -0.4857f,  -1.1239f,
      3.1404f,   -13.1098f, -5.9165f,  22.2060f, -0.0312f,  -3.9642f,  -0.0344f,
      -0.0656f,  -0.0273f,  -0.0465f,  0.1412f,  -6.1974f,  9.3661f,
    };

static const float av1_pustats_rate_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      -14.3065f, 2.059f,   -62.9916f, -50.1209f, 57.643f,  -59.3737f,
      -30.4737f, -0.1112f, 72.5427f,  55.402f,   24.9523f, 18.5834f,
    };

static const float
    av1_pustats_rate_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      0.3883f,  -0.2784f, -0.2850f, 0.4894f,  -2.2450f, 0.4511f,  -0.1969f,
      -0.0077f, -1.4924f, 0.1138f,  -2.9848f, 1.0211f,  -0.1712f, -0.1952f,
      -0.4774f, 0.0761f,  -0.3186f, -0.1002f, 0.8663f,  0.5026f,  1.1920f,
      0.9337f,  0.3911f,  -0.3841f, -0.0037f, 0.7295f,  -0.3183f, 0.1829f,
      -1.3670f, -0.1046f, 0.6629f,  0.0619f,  -0.1551f, 0.8174f,  2.1521f,
      -1.3323f, -0.0527f, -0.5772f, 0.2001f,  -0.6270f, -1.0625f, 0.3342f,
      0.6676f,  0.4605f,  -2.0049f, 0.7781f,  0.0713f,  -0.0824f, -0.4529f,
      0.1757f,  -0.1338f, -0.2319f, -0.2864f, 0.1248f,  0.3887f,  -0.1676f,
      1.8422f,  0.6435f,  1.2123f,  -0.5667f, -0.2423f, -0.0314f, 0.2411f,
      -0.5013f, 0.0422f,  0.2559f,  0.4435f,  -0.1223f, 1.5167f,  0.3939f,
      1.0898f,  0.0795f,  -0.9251f, -0.0813f, -0.5929f, -0.0741f, 4.0687f,
      -0.4368f, -0.0984f, 0.0837f,  3.6169f,  0.0662f,  -0.1679f, -0.8090f,
      -0.2610f, -0.5791f, 0.0642f,  -0.2979f, -0.9036f, 0.2898f,  0.3265f,
      0.4660f,  -1.6358f, -0.0347f, 0.1087f,  0.0353f,  0.5687f,  -0.5242f,
      -0.4895f, 0.7693f,  -1.3829f, -0.2244f, -0.2880f, 0.0575f,  2.0563f,
      -0.2322f, -1.1597f, 1.6125f,  -0.0925f, 1.3540f,  0.1432f,  0.3993f,
      -0.0303f, -1.1438f, -1.7323f, -0.4329f, 2.9443f,  -0.5724f, 0.0122f,
      -1.0829f,
    };

static const float av1_pustats_rate_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      -10.3717f, 37.304f,  -36.7221f, -52.7572f, 44.0877f,
      41.1631f,  36.3299f, -48.6087f, -4.5189f,  13.0611f,
    };

static const float
    av1_pustats_rate_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      0.8362f, 1.0615f, -1.5178f, -1.2959f, 1.3233f,
      1.4909f, 1.3554f, -0.8626f, -0.618f,  -0.9458f,
    };

static const float av1_pustats_rate_logits_bias[LOGITS_NODES] = {
  30.6878f,
};

static const NN_CONFIG av1_pustats_rate_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_rate_hiddenlayer_0_kernel,
      av1_pustats_rate_hiddenlayer_1_kernel,
      av1_pustats_rate_logits_kernel,
  },
  {
      av1_pustats_rate_hiddenlayer_0_bias,
      av1_pustats_rate_hiddenlayer_1_bias,
      av1_pustats_rate_logits_bias,
  },
};

static const float
    av1_pustats_dist_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      0.7770f,   1.0881f,  0.0177f,  0.4939f,  -0.2541f, -0.2672f, -0.1705f,
      -0.1940f,  -0.6395f, 1.2928f,  3.6240f,  2.4445f,  1.6790f,  0.0265f,
      0.1897f,   0.1776f,  0.0422f,  0.0197f,  -0.0466f, 0.0462f,  -1.0827f,
      2.0231f,   1.8044f,  2.7022f,  0.0064f,  0.2255f,  -0.0552f, -0.1010f,
      -0.0581f,  -0.0781f, 0.2614f,  -3.4085f, 1.7478f,  0.1155f,  -0.1458f,
      -0.0031f,  -0.1797f, -0.4378f, -0.0539f, 0.0607f,  -0.1347f, -0.3142f,
      -0.2014f,  -0.4484f, -0.2808f, 1.5913f,  0.0046f,  -0.0610f, -0.6479f,
      -0.7278f,  -0.5592f, -0.6695f, -0.8120f, 2.9056f,  -1.1501f, 9.3618f,
      4.2486f,   0.0011f,  -0.1499f, -0.0834f, 0.1282f,  0.0409f,  0.1670f,
      -0.1398f,  -0.4661f, 13.7700f, 8.2061f,  -0.0685f, 0.0061f,  -0.2951f,
      0.0169f,   0.0520f,  0.0040f,  0.0374f,  0.0467f,  -0.0107f, 14.2664f,
      -2.2489f,  -0.2516f, -0.0061f, -0.9921f, 0.1223f,  0.1212f,  0.1199f,
      0.1185f,   -0.4867f, 0.0325f,  -5.0757f, -8.7853f, 1.0450f,  0.0169f,
      0.5462f,   0.0051f,  0.1330f,  0.0143f,  0.1429f,  -0.0258f, 0.2769f,
      -12.8839f, 22.3093f, 1.2761f,  0.0037f,  -1.2459f, -0.0466f, 0.0003f,
      -0.0464f,  -0.0067f, 0.2361f,  0.0355f,  23.3833f, 10.9218f, 2.6811f,
      0.0222f,   -1.1055f, 0.1825f,  0.0575f,  0.0114f,  -0.1259f, 0.3148f,
      -2.0047f,  11.9559f, 5.7375f,  0.8802f,  0.0042f,  -0.2469f, -0.1040f,
      -1.5679f,  0.1969f,  -0.0184f, 0.0157f,  0.6688f,  3.4492f,
    };

static const float av1_pustats_dist_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      4.5051f,  -4.5858f, 1.4693f, 0.f,      3.7968f, -3.6292f,
      -7.3112f, 10.9743f, 8.027f,  -2.2692f, -8.748f, -1.3689f,
    };

static const float
    av1_pustats_dist_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.0182f, -0.0925f, -0.0311f, -0.2962f, 0.1177f,  -0.0027f, -0.2136f,
      -1.2094f, 0.0935f,  -0.1403f, -0.1477f, -0.0752f, 0.1519f,  -0.4726f,
      -0.3521f, 0.4199f,  -0.0168f, -0.2927f, -0.2510f, 0.0706f,  -0.2920f,
      0.2046f,  -0.0400f, -0.2114f, 0.4240f,  -0.7070f, 0.4964f,  0.4471f,
      0.3841f,  -0.0918f, -0.6140f, 0.6056f,  -0.1123f, 0.3944f,  -0.0178f,
      -1.7702f, -0.4434f, 0.0560f,  0.1565f,  -0.0793f, -0.0041f, 0.0052f,
      -0.1843f, 0.2400f,  -0.0605f, 0.3196f,  -0.0286f, -0.0002f, -0.0595f,
      -0.0493f, -0.2636f, -0.3994f, -0.1871f, -0.3298f, -0.0788f, -1.0685f,
      0.1900f,  -0.5549f, -0.1350f, -0.0153f, -0.1195f, -0.5874f, 1.0468f,
      0.0212f,  -0.2306f, -0.2677f, -0.3000f, -1.0702f, -0.1725f, -0.0656f,
      -0.0226f, 0.0616f,  -0.3453f, 0.0810f,  0.4838f,  -0.3780f, -1.4486f,
      0.7777f,  -0.0459f, -0.6568f, 0.0589f,  -1.0286f, -0.6001f, 0.0826f,
      0.4794f,  -0.0586f, -0.1759f, 0.3811f,  -0.1313f, 0.3829f,  -0.0968f,
      -2.0445f, -0.3566f, -0.1491f, -0.0745f, -0.0202f, 0.0839f,  0.0470f,
      -0.2432f, 0.3013f,  -0.0743f, -0.3479f, 0.0749f,  -5.2490f, 0.0209f,
      -0.1653f, -0.0826f, -0.0535f, 0.3225f,  -0.3786f, -0.0104f, 0.3091f,
      0.3652f,  0.1757f,  -0.3252f, -1.1022f, -0.0574f, -0.4473f, 0.3469f,
      -0.5539f,
    };

static const float av1_pustats_dist_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      11.9337f, -0.3681f, -6.1324f,  12.674f,  9.0956f,
      4.6069f,  -4.4158f, -12.4848f, 10.8473f, 5.7633f,
    };

static const float
    av1_pustats_dist_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      0.3245f,  0.2979f,  -0.157f,  -0.1441f, 0.1413f,
      -0.7496f, -0.1737f, -0.5322f, 0.0748f,  0.2518f,
    };

static const float av1_pustats_dist_logits_bias[LOGITS_NODES] = {
  4.6065f,
};

static const NN_CONFIG av1_pustats_dist_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_dist_hiddenlayer_0_kernel,
      av1_pustats_dist_hiddenlayer_1_kernel,
      av1_pustats_dist_logits_kernel,
  },
  {
      av1_pustats_dist_hiddenlayer_0_bias,
      av1_pustats_dist_hiddenlayer_1_bias,
      av1_pustats_dist_logits_bias,
  },
};

#undef NUM_FEATURES
#undef NUM_HIDDEN_LAYERS
#undef HIDDEN_LAYERS_0_NODES
#undef HIDDEN_LAYERS_1_NODES
#undef LOGITS_NODES

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_PUSTATS_H_
