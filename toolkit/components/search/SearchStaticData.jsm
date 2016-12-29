/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module contains additional data about default search engines that is the
 * same across all languages.  This information is defined outside of the actual
 * search engine definition files, so that localizers don't need to update them
 * when a change is made.
 *
 * This separate module is also easily overridable, in case a hotfix is needed.
 * No high-level processing logic is applied here.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SearchStaticData",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// To update this list of known alternate domains, just cut-and-paste from
// https://www.google.com/supported_domains
const gGoogleDomainsSource = ".google.com .google.ad .google.ae .google.com.af .google.com.ag .google.com.ai .google.al .google.am .google.co.ao .google.com.ar .google.as .google.at .google.com.au .google.az .google.ba .google.com.bd .google.be .google.bf .google.bg .google.com.bh .google.bi .google.bj .google.com.bn .google.com.bo .google.com.br .google.bs .google.bt .google.co.bw .google.by .google.com.bz .google.ca .google.cd .google.cf .google.cg .google.ch .google.ci .google.co.ck .google.cl .google.cm .google.cn .google.com.co .google.co.cr .google.com.cu .google.cv .google.com.cy .google.cz .google.de .google.dj .google.dk .google.dm .google.com.do .google.dz .google.com.ec .google.ee .google.com.eg .google.es .google.com.et .google.fi .google.com.fj .google.fm .google.fr .google.ga .google.ge .google.gg .google.com.gh .google.com.gi .google.gl .google.gm .google.gp .google.gr .google.com.gt .google.gy .google.com.hk .google.hn .google.hr .google.ht .google.hu .google.co.id .google.ie .google.co.il .google.im .google.co.in .google.iq .google.is .google.it .google.je .google.com.jm .google.jo .google.co.jp .google.co.ke .google.com.kh .google.ki .google.kg .google.co.kr .google.com.kw .google.kz .google.la .google.com.lb .google.li .google.lk .google.co.ls .google.lt .google.lu .google.lv .google.com.ly .google.co.ma .google.md .google.me .google.mg .google.mk .google.ml .google.com.mm .google.mn .google.ms .google.com.mt .google.mu .google.mv .google.mw .google.com.mx .google.com.my .google.co.mz .google.com.na .google.com.nf .google.com.ng .google.com.ni .google.ne .google.nl .google.no .google.com.np .google.nr .google.nu .google.co.nz .google.com.om .google.com.pa .google.com.pe .google.com.pg .google.com.ph .google.com.pk .google.pl .google.pn .google.com.pr .google.ps .google.pt .google.com.py .google.com.qa .google.ro .google.ru .google.rw .google.com.sa .google.com.sb .google.sc .google.se .google.com.sg .google.sh .google.si .google.sk .google.com.sl .google.sn .google.so .google.sm .google.sr .google.st .google.com.sv .google.td .google.tg .google.co.th .google.com.tj .google.tk .google.tl .google.tm .google.tn .google.to .google.com.tr .google.tt .google.com.tw .google.co.tz .google.com.ua .google.co.ug .google.co.uk .google.com.uy .google.co.uz .google.com.vc .google.co.ve .google.vg .google.co.vi .google.com.vn .google.vu .google.ws .google.rs .google.co.za .google.co.zm .google.co.zw .google.cat";
const gGoogleDomains = gGoogleDomainsSource.split(" ").map(d => "www" + d);

this.SearchStaticData = {
  /**
   * Returns a list of alternate domains for a given search engine domain.
   *
   * @param aDomain
   *        Lowercase host name to look up. For example, if this argument is
   *        "www.google.com" or "www.google.co.uk", the function returns the
   *        full list of supported Google domains.
   *
   * @return Array containing one entry for each alternate host name, or empty
   *         array if none is known.  The returned array should not be modified.
   */
  getAlternateDomains(aDomain) {
    return gGoogleDomains.indexOf(aDomain) == -1 ? [] : gGoogleDomains;
  },
};
