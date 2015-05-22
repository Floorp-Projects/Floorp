/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2006 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*********************************************************************
 *
 * File: fe_warp_inverse_linear.c
 * 
 * Description: 
 * 	Warp the frequency axis according to an inverse_linear function, i.e.:
 *
 *		w' = w / a
 *	
 *********************************************************************/

/* static char rcsid[] = "@(#)$Id: fe_warp_inverse_linear.c,v 1.3 2006/02/23 19:40:11 eht Exp $"; */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning (disable: 4996)
#endif

#include "sphinxbase/strfuncs.h"
#include "sphinxbase/err.h"

#include "fe_warp.h"
#include "fe_warp_inverse_linear.h"

#define N_PARAM		1
#define YES             1
#define NO              0

/*
 * params[0] : a
 */
static float params[N_PARAM] = { 1.0f };
static int32 is_neutral = YES;
static char p_str[256] = "";
static float nyquist_frequency = 0.0f;


const char *
fe_warp_inverse_linear_doc()
{
    return "inverse_linear :== < w' = x / a >";
}

uint32
fe_warp_inverse_linear_id()
{
    return FE_WARP_ID_INVERSE_LINEAR;
}

uint32
fe_warp_inverse_linear_n_param()
{
    return N_PARAM;
}

void
fe_warp_inverse_linear_set_parameters(char const *param_str, float sampling_rate)
{
    char *tok;
    char *seps = " \t";
    char temp_param_str[256];
    int param_index = 0;

    nyquist_frequency = sampling_rate / 2;
    if (param_str == NULL) {
        is_neutral = YES;
        return;
    }
    /* The new parameters are the same as the current ones, so do nothing. */
    if (strcmp(param_str, p_str) == 0) {
        return;
    }
    is_neutral = NO;
    strcpy(temp_param_str, param_str);
    memset(params, 0, N_PARAM * sizeof(float));
    strcpy(p_str, param_str);
    /* FIXME: strtok() is not re-entrant... */
    tok = strtok(temp_param_str, seps);
    while (tok != NULL) {
        params[param_index++] = (float) atof_c(tok);
        tok = strtok(NULL, seps);
        if (param_index >= N_PARAM) {
            break;
        }
    }
    if (tok != NULL) {
        E_INFO
            ("Inverse linear warping takes only one argument, %s ignored.\n",
             tok);
    }
    if (params[0] == 0) {
        is_neutral = YES;
        E_INFO
            ("Inverse linear warping cannot have slope zero, warping not applied.\n");
    }
}

float
fe_warp_inverse_linear_warped_to_unwarped(float nonlinear)
{
    if (is_neutral) {
        return nonlinear;
    }
    else {
        /* linear = nonlinear * a */
        float temp = nonlinear * params[0];
        if (temp > nyquist_frequency) {
            E_WARN
                ("Warp factor %g results in frequency (%.1f) higher than Nyquist (%.1f)\n",
                 params[0], temp, nyquist_frequency);
        }
        return temp;
    }
}

float
fe_warp_inverse_linear_unwarped_to_warped(float linear)
{
    if (is_neutral) {
        return linear;
    }
    else {
        /* nonlinear = a / linear */
        float temp = linear / params[0];
        return temp;
    }
}

void
fe_warp_inverse_linear_print(const char *label)
{
    uint32 i;

    for (i = 0; i < N_PARAM; i++) {
        printf("%s[%04u]: %6.3f ", label, i, params[i]);
    }
    printf("\n");
}
