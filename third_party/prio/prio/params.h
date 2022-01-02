/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __PARAMS_H__
#define __PARAMS_H__

// A prime modulus p.
static const char Modulus[] = "8000000000000000080001";

// A generator g of a subgroup of Z*_p.
static const char Generator[] = "2597c14f48d5b65ed8dcca";

// The generator g generates a subgroup of
// order 2^Generator2Order in Z*_p.
static const int Generator2Order = 19;

#endif /* __PARAMS_H__ */
