/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is from Alexa Internet (www.alexa.com).
 *
 * The Initial Developer of the Original Code is
 * Alexa Internet.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
  Code for the Related Links Sidebar Panel
 */


var kUnknownReasonUrl		= 'about:blank';
var kMAMIUrl				= 'http://xslt.alexa.com/data?cli=17&dat=nsa';

var kNoHTTPReasonUrl		= kMAMIUrl + 'req_type=secure_intranet';
var kSkipDomainReasonUrl	= kMAMIUrl + 'req_type=blocked_list';
var kDataPrefixUrl			= kMAMIUrl;

var oNavObserver = null;

// Our observer of Navigation Messages
function NavObserver(oDisplayFrame,oContentWindow)
{
	this.m_oDisplayFrame = oDisplayFrame;
	this.m_oContentWindow = oContentWindow;
	this.m_sWindowID = ''+parseInt(Math.random()*32767);
	this.m_sLastDataUrl = 'about:blank';	// The last url that we drove our display to.
}

NavObserver.prototype.observe =
function (oSubject, sMessage, sContextUrl)
{
	try {
		if (oSubject != this.m_oContentWindow) {
			// only pay attention to our client window.
			return;
		}
		var bReferrer = (this.m_oContentWindow.document.referrer)?true:false;
		if ((sMessage == 'EndDocumentLoad') 
            && (sContextUrl != this.m_oContentWindow.location)) {
				// we were redirected...
			sContextUrl = '' + this.m_oContentWindow.location;
			bReferrer = true;
		}
		this.TrackContext(sContextUrl, bReferrer);
	} catch(ex) {
	}
}

NavObserver.prototype.GetCDT =
function (bReferrer)
{
	var sCDT = '';
	sCDT += 't=' +(bReferrer?'1':'0');
	sCDT += '&pane=nswr6';
	sCDT += '&wid='+this.m_sWindowID;

	return encodeURIComponent(sCDT);
}

NavObserver.prototype.TrackContext =
function (sContextUrl, bReferrer)
{
	if (sContextUrl != this.m_sLastContextUrl && this.m_oDisplayFrame) {
		var sDataUrl = this.TranslateContext(sContextUrl,bReferrer);
		this.m_oDisplayFrame.setAttribute('src', sDataUrl);
		this.m_sLastContextUrl = sContextUrl;
	}
}

NavObserver.prototype.TranslateContext =
function (sUrl, bReferrer)
{
	if (!sUrl || ('string' != typeof(sUrl))
        || ('' == sUrl) || sUrl == 'about:blank') {
		return kUnknownReasonUrl;
	}

    // Strip off any query strings (Don't want to be too nosy).
	var nQueryPart = sUrl.indexOf('?');
    if (nQueryPart != 1) {
        sUrl = sUrl.slice(0, nQueryPart);
    }

	// We can only get related links data on HTTP URLs
	if (0 != sUrl.indexOf("http://")) {
		return kNoHTTPReasonUrl;
	}

	// ...and non-intranet sites(those that have a '.' in the domain)
	var sUrlSuffix = sUrl.substr(7);			// strip off "http://" prefix

	var nFirstSlash = sUrlSuffix.indexOf('/');
	var nFirstDot = sUrlSuffix.indexOf('.');

	if (-1 == nFirstDot)
		return kNoHTTPReasonUrl;

	if ((nFirstSlash < nFirstDot) && (-1 != nFirstSlash))
		return kNoHTTPReasonUrl;

	// url is http, non-intranet url: see if the domain is in their blocked list.

	var nPortOffset = sUrlSuffix.indexOf(":");
	var nDomainEnd = (((nPortOffset >=0) && (nPortOffset <= nFirstSlash))
                      ? nPortOffset : nFirstSlash);

	var sDomain = sUrlSuffix;
	if (-1 != nDomainEnd) {
		sDomain = sUrlSuffix.substr(0,nDomainEnd);
	}

	if (DomainInSkipList(sDomain)) {
		return kSkipDomainReasonUrl;
	} else {
		// ok! it is a good url!
		var sFinalUrl = kDataPrefixUrl;
		sFinalUrl += 'cdt='+this.GetCDT(bReferrer);
		sFinalUrl += '&url='+sUrl;
		return sFinalUrl;
	}
}


function DomainInSkipList(sDomain)
{
	var bSkipDomainFlag = false;

	if ('/' == sDomain[sDomain.length - 1]) {
		sDomain = sDomain.substring(0, sDomain.length - 1);
	}

	try {
		var pref = Components.classes["@mozilla.org/preferences-service;1"]
							 .getService(Components.interfaces.nsIPrefBranch);

		var sDomainList = pref.getCharPref("browser.related.disabledForDomains");
		if ((sDomainList) && (sDomainList != "")) {

			var aDomains = sDomainList.split(",");

			// split on commas
			for (var x=0; x < aDomains.length; x++) {
				var sDomainCopy = sDomain;

				var sTestDomain = aDomains[x];

				if ('*' == sTestDomain[0]) { // wildcard match

                    // strip off the asterisk
					sTestDomain = sTestDomain.substring(1);	
					if (sDomainCopy.length > sTestDomain.length) {
                        var sDomainIndex = sDomain.length - sTestDomain.length;
						sDomainCopy = sDomainCopy.substring(sDomainIndex);
					}
				}

				if (0 == sDomainCopy.lastIndexOf(sTestDomain)) {

					bSkipDomainFlag = true;
					break;
				}
			}
		}
	} catch(ex) {
	}
	return(bSkipDomainFlag);
}

function Init()
{
	// Initialize the Related Links panel

	// Install our navigation observer so we can track the main client window.

	oContentWindow = window.content;
	oFrame = document.getElementById('daFrame');

	if (oContentWindow && oFrame) {
		var oObserverService = Components.classes["@mozilla.org/observer-service;1"].getService();
		oObserverService = oObserverService.QueryInterface(Components.interfaces.nsIObserverService);

		oNavObserver = new NavObserver(oFrame,oContentWindow);
		oNavObserver.TrackContext(''+oContentWindow.location);

		if (oObserverService && oNavObserver) {
			oObserverService.addObserver(oNavObserver, "StartDocumentLoad", false);
			oObserverService.addObserver(oNavObserver, "EndDocumentLoad", false);
			oObserverService.addObserver(oNavObserver, "FailDocumentLoad", false);
		} else {
			oNavObserver = null;
			dump("FAILURE to get observer service\n");
		}
	}
}

function Destruct()
{
	// remove our navigation observer.
	var oObserverService = Components.classes["@mozilla.org/observer-service;1"].getService();
	oObserverService = oObserverService.QueryInterface(Components.interfaces.nsIObserverService);
	if (oObserverService && oNavObserver) {
		oObserverService.removeObserver(oNavObserver, "StartDocumentLoad");
		oObserverService.removeObserver(oNavObserver, "EndDocumentLoad");
		oObserverService.removeObserver(oNavObserver, "FailDocumentLoad");
		oNavObserver = null;
	} else {
		dump("FAILURE to get observer service\n");
	}
}

addEventListener("load", Init, false);
addEventListener("unload", Destruct, false);
