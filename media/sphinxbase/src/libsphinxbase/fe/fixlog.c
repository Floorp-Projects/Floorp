/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2005 Carnegie Mellon University.  All rights 
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
 * File: fixlog.c
 *
 * Description: Fast approximate fixed-point logarithms
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sphinxbase/prim_type.h"
#include "sphinxbase/fixpoint.h"

#include "fe_internal.h"

/* Table of log2(x/128)*(1<<DEFAULT_RADIX) */
/* perl -e 'for (0..128) {my $x = 1 + $_/128; $y = 1 + ($_ + 1.) / 128;
                  print "    (uint32)(", (log($x) + log($y))/2/log(2)," *(1<<DEFAULT_RADIX)),\n"}' */
static uint32 logtable[] = {
    (uint32)(0.00561362771162706*(1<<DEFAULT_RADIX)),
    (uint32)(0.0167975342258543*(1<<DEFAULT_RADIX)),
    (uint32)(0.0278954072829524*(1<<DEFAULT_RADIX)),
    (uint32)(0.0389085604479519*(1<<DEFAULT_RADIX)),
    (uint32)(0.0498382774298215*(1<<DEFAULT_RADIX)),
    (uint32)(0.060685812979481*(1<<DEFAULT_RADIX)),
    (uint32)(0.0714523937543017*(1<<DEFAULT_RADIX)),
    (uint32)(0.0821392191505851*(1<<DEFAULT_RADIX)),
    (uint32)(0.0927474621054331*(1<<DEFAULT_RADIX)),
    (uint32)(0.103278269869348*(1<<DEFAULT_RADIX)),
    (uint32)(0.113732764750838*(1<<DEFAULT_RADIX)),
    (uint32)(0.124112044834237*(1<<DEFAULT_RADIX)),
    (uint32)(0.13441718467188*(1<<DEFAULT_RADIX)),
    (uint32)(0.144649235951738*(1<<DEFAULT_RADIX)),
    (uint32)(0.154809228141536*(1<<DEFAULT_RADIX)),
    (uint32)(0.164898169110351*(1<<DEFAULT_RADIX)),
    (uint32)(0.174917045728623*(1<<DEFAULT_RADIX)),
    (uint32)(0.184866824447476*(1<<DEFAULT_RADIX)),
    (uint32)(0.194748451858191*(1<<DEFAULT_RADIX)),
    (uint32)(0.204562855232657*(1<<DEFAULT_RADIX)),
    (uint32)(0.214310943045556*(1<<DEFAULT_RADIX)),
    (uint32)(0.223993605479021*(1<<DEFAULT_RADIX)),
    (uint32)(0.23361171491048*(1<<DEFAULT_RADIX)),
    (uint32)(0.243166126384332*(1<<DEFAULT_RADIX)),
    (uint32)(0.252657678068119*(1<<DEFAULT_RADIX)),
    (uint32)(0.262087191693777*(1<<DEFAULT_RADIX)),
    (uint32)(0.271455472984569*(1<<DEFAULT_RADIX)),
    (uint32)(0.280763312068243*(1<<DEFAULT_RADIX)),
    (uint32)(0.290011483876938*(1<<DEFAULT_RADIX)),
    (uint32)(0.299200748534365*(1<<DEFAULT_RADIX)),
    (uint32)(0.308331851730729*(1<<DEFAULT_RADIX)),
    (uint32)(0.317405525085859*(1<<DEFAULT_RADIX)),
    (uint32)(0.32642248650099*(1<<DEFAULT_RADIX)),
    (uint32)(0.335383440499621*(1<<DEFAULT_RADIX)),
    (uint32)(0.344289078557851*(1<<DEFAULT_RADIX)),
    (uint32)(0.353140079424581*(1<<DEFAULT_RADIX)),
    (uint32)(0.36193710943195*(1<<DEFAULT_RADIX)),
    (uint32)(0.37068082279637*(1<<DEFAULT_RADIX)),
    (uint32)(0.379371861910488*(1<<DEFAULT_RADIX)),
    (uint32)(0.388010857626406*(1<<DEFAULT_RADIX)),
    (uint32)(0.396598429530472*(1<<DEFAULT_RADIX)),
    (uint32)(0.405135186209943*(1<<DEFAULT_RADIX)),
    (uint32)(0.4136217255118*(1<<DEFAULT_RADIX)),
    (uint32)(0.422058634793998*(1<<DEFAULT_RADIX)),
    (uint32)(0.430446491169411*(1<<DEFAULT_RADIX)),
    (uint32)(0.438785861742727*(1<<DEFAULT_RADIX)),
    (uint32)(0.447077303840529*(1<<DEFAULT_RADIX)),
    (uint32)(0.455321365234813*(1<<DEFAULT_RADIX)),
    (uint32)(0.463518584360147*(1<<DEFAULT_RADIX)),
    (uint32)(0.471669490524698*(1<<DEFAULT_RADIX)),
    (uint32)(0.479774604115327*(1<<DEFAULT_RADIX)),
    (uint32)(0.487834436796966*(1<<DEFAULT_RADIX)),
    (uint32)(0.49584949170644*(1<<DEFAULT_RADIX)),
    (uint32)(0.503820263640951*(1<<DEFAULT_RADIX)),
    (uint32)(0.511747239241369*(1<<DEFAULT_RADIX)),
    (uint32)(0.519630897170528*(1<<DEFAULT_RADIX)),
    (uint32)(0.527471708286662*(1<<DEFAULT_RADIX)),
    (uint32)(0.535270135812172*(1<<DEFAULT_RADIX)),
    (uint32)(0.543026635497834*(1<<DEFAULT_RADIX)),
    (uint32)(0.550741655782637*(1<<DEFAULT_RADIX)),
    (uint32)(0.558415637949355*(1<<DEFAULT_RADIX)),
    (uint32)(0.56604901627601*(1<<DEFAULT_RADIX)),
    (uint32)(0.573642218183348*(1<<DEFAULT_RADIX)),
    (uint32)(0.581195664378452*(1<<DEFAULT_RADIX)),
    (uint32)(0.588709768994618*(1<<DEFAULT_RADIX)),
    (uint32)(0.596184939727604*(1<<DEFAULT_RADIX)),
    (uint32)(0.603621577968369*(1<<DEFAULT_RADIX)),
    (uint32)(0.61102007893241*(1<<DEFAULT_RADIX)),
    (uint32)(0.618380831785792*(1<<DEFAULT_RADIX)),
    (uint32)(0.625704219767993*(1<<DEFAULT_RADIX)),
    (uint32)(0.632990620311629*(1<<DEFAULT_RADIX)),
    (uint32)(0.640240405159187*(1<<DEFAULT_RADIX)),
    (uint32)(0.647453940476827*(1<<DEFAULT_RADIX)),
    (uint32)(0.654631586965362*(1<<DEFAULT_RADIX)),
    (uint32)(0.661773699968486*(1<<DEFAULT_RADIX)),
    (uint32)(0.668880629578336*(1<<DEFAULT_RADIX)),
    (uint32)(0.675952720738471*(1<<DEFAULT_RADIX)),
    (uint32)(0.682990313344332*(1<<DEFAULT_RADIX)),
    (uint32)(0.689993742341272*(1<<DEFAULT_RADIX)),
    (uint32)(0.696963337820209*(1<<DEFAULT_RADIX)),
    (uint32)(0.703899425110987*(1<<DEFAULT_RADIX)),
    (uint32)(0.710802324873503*(1<<DEFAULT_RADIX)),
    (uint32)(0.717672353186654*(1<<DEFAULT_RADIX)),
    (uint32)(0.724509821635192*(1<<DEFAULT_RADIX)),
    (uint32)(0.731315037394519*(1<<DEFAULT_RADIX)),
    (uint32)(0.738088303313493*(1<<DEFAULT_RADIX)),
    (uint32)(0.744829917995304*(1<<DEFAULT_RADIX)),
    (uint32)(0.751540175876464*(1<<DEFAULT_RADIX)),
    (uint32)(0.758219367303974*(1<<DEFAULT_RADIX)),
    (uint32)(0.764867778610703*(1<<DEFAULT_RADIX)),
    (uint32)(0.77148569218905*(1<<DEFAULT_RADIX)),
    (uint32)(0.778073386562917*(1<<DEFAULT_RADIX)),
    (uint32)(0.784631136458046*(1<<DEFAULT_RADIX)),
    (uint32)(0.791159212870769*(1<<DEFAULT_RADIX)),
    (uint32)(0.797657883135205*(1<<DEFAULT_RADIX)),
    (uint32)(0.804127410988954*(1<<DEFAULT_RADIX)),
    (uint32)(0.810568056637321*(1<<DEFAULT_RADIX)),
    (uint32)(0.816980076816112*(1<<DEFAULT_RADIX)),
    (uint32)(0.823363724853051*(1<<DEFAULT_RADIX)),
    (uint32)(0.829719250727828*(1<<DEFAULT_RADIX)),
    (uint32)(0.836046901130843*(1<<DEFAULT_RADIX)),
    (uint32)(0.84234691952066*(1<<DEFAULT_RADIX)),
    (uint32)(0.848619546180216*(1<<DEFAULT_RADIX)),
    (uint32)(0.854865018271815*(1<<DEFAULT_RADIX)),
    (uint32)(0.861083569890926*(1<<DEFAULT_RADIX)),
    (uint32)(0.867275432118842*(1<<DEFAULT_RADIX)),
    (uint32)(0.873440833074202*(1<<DEFAULT_RADIX)),
    (uint32)(0.879579997963421*(1<<DEFAULT_RADIX)),
    (uint32)(0.88569314913005*(1<<DEFAULT_RADIX)),
    (uint32)(0.891780506103101*(1<<DEFAULT_RADIX)),
    (uint32)(0.897842285644346*(1<<DEFAULT_RADIX)),
    (uint32)(0.903878701794633*(1<<DEFAULT_RADIX)),
    (uint32)(0.90988996591924*(1<<DEFAULT_RADIX)),
    (uint32)(0.915876286752278*(1<<DEFAULT_RADIX)),
    (uint32)(0.921837870440188*(1<<DEFAULT_RADIX)),
    (uint32)(0.927774920584334*(1<<DEFAULT_RADIX)),
    (uint32)(0.933687638282728*(1<<DEFAULT_RADIX)),
    (uint32)(0.939576222170905*(1<<DEFAULT_RADIX)),
    (uint32)(0.945440868461959*(1<<DEFAULT_RADIX)),
    (uint32)(0.951281770985776*(1<<DEFAULT_RADIX)),
    (uint32)(0.957099121227478*(1<<DEFAULT_RADIX)),
    (uint32)(0.962893108365084*(1<<DEFAULT_RADIX)),
    (uint32)(0.968663919306429*(1<<DEFAULT_RADIX)),
    (uint32)(0.974411738725344*(1<<DEFAULT_RADIX)),
    (uint32)(0.980136749097113*(1<<DEFAULT_RADIX)),
    (uint32)(0.985839130733238*(1<<DEFAULT_RADIX)),
    (uint32)(0.991519061815512*(1<<DEFAULT_RADIX)),
    (uint32)(0.997176718429429*(1<<DEFAULT_RADIX)),
    (uint32)(1.00281227459694*(1<<DEFAULT_RADIX)),
};

int32
fixlog2(uint32 x)
{
    uint32 y;

    if (x == 0)
        return MIN_FIXLOG2;

    /* Get the exponent. */
#if ((defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) || \
      defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_7A__)) && !defined(__thumb__))
  __asm__("clz %0, %1\n": "=r"(y):"r"(x));
    x <<= y;
    y = 31 - y;
#elif defined(__ppc__)
  __asm__("cntlzw %0, %1\n": "=r"(y):"r"(x));
    x <<= y;
    y = 31 - y;
#elif __GNUC__ >= 4
    y = __builtin_clz(x);
    x <<= y;
    y = (31 - y);
#else
    for (y = 31; y > 0; --y) {
        if (x & 0x80000000)
            break;
        x <<= 1;
    }
#endif
    y <<= DEFAULT_RADIX;
    /* Do a table lookup for the MSB of the mantissa. */
    x = (x >> 24) & 0x7f;
    return y + logtable[x];
}

int
fixlog(uint32 x)
{
    int32 y;
    y = fixlog2(x);
    return FIXMUL(y, FIXLN_2);
}
