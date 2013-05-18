/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* nsStrictTransportSecurityService.cpp, you shouldn't be #including it.     */
/*****************************************************************************/

#include "mozilla/StandardInteger.h"
const PRTime gPreloadListExpirationTime = INT64_C(1379758102034000);

class nsSTSPreload
{
  public:
    const char *mHost;
    const bool mIncludeSubdomains;
};

static const nsSTSPreload kSTSPreloadList[] = {
  { "alpha.irccloud.com", false },
  { "api.intercom.io", false },
  { "app.recurly.com", false },
  { "arivo.com.br", true },
  { "bassh.net", true },
  { "blog.torproject.org", false },
  { "bugzilla.mozilla.org", true },
  { "business.medbank.com.mt", true },
  { "check.torproject.org", false },
  { "cloudsecurityalliance.org", true },
  { "controlcenter.gigahost.dk", true },
  { "crate.io", true },
  { "crm.onlime.ch", false },
  { "crypto.cat", false },
  { "csawctf.poly.edu", true },
  { "developer.mydigipass.com", false },
  { "dist.torproject.org", false },
  { "ebanking.indovinabank.com.vn", false },
  { "entropia.de", false },
  { "espra.com", true },
  { "factor.cc", false },
  { "gocardless.com", true },
  { "grc.com", false },
  { "howrandom.org", true },
  { "id.mayfirst.org", false },
  { "intercom.io", false },
  { "itriskltd.com", true },
  { "keyerror.com", true },
  { "lastpass.com", false },
  { "lockify.com", true },
  { "logentries.com", false },
  { "login.persona.org", true },
  { "login.sapo.pt", true },
  { "luneta.nearbuysystems.com", false },
  { "makeyourlaws.org", false },
  { "mattmccutchen.net", true },
  { "mega.co.nz", false },
  { "members.mayfirst.org", false },
  { "members.nearlyfreespeech.net", false },
  { "my.onlime.ch", false },
  { "neg9.org", false },
  { "passwd.io", true },
  { "pay.gigahost.dk", true },
  { "paymill.com", true },
  { "paymill.de", true },
  { "piratenlogin.de", true },
  { "pixi.me", true },
  { "riseup.net", true },
  { "roundcube.mayfirst.org", false },
  { "sandbox.mydigipass.com", false },
  { "serverdensity.io", true },
  { "silentcircle.com", false },
  { "simple.com", false },
  { "stocktrade.de", false },
  { "stripe.com", true },
  { "support.mayfirst.org", false },
  { "surfeasy.com", false },
  { "therapynotes.com", false },
  { "twitter.com", false },
  { "ubertt.org", true },
  { "webmail.gigahost.dk", false },
  { "webmail.mayfirst.org", false },
  { "webmail.onlime.ch", false },
  { "wiz.biz", true },
  { "writeapp.me", false },
  { "www.apollo-auto.com", true },
  { "www.braintreepayments.com", false },
  { "www.cueup.com", false },
  { "www.entropia.de", false },
  { "www.gov.uk", false },
  { "www.grc.com", false },
  { "www.intercom.io", false },
  { "www.irccloud.com", false },
  { "www.lookout.com", false },
  { "www.mydigipass.com", false },
  { "www.noisebridge.net", false },
  { "www.simple.com", false },
  { "www.surfeasy.com", false },
  { "www.therapynotes.com", false },
  { "www.torproject.org", false },
  { "www.twitter.com", false },
  { "zoo24.de", true },
};
