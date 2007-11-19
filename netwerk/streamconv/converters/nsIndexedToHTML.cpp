/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
 *   Christopher A. Aillon <christopher@aillon.com>
 *   Dão Gottwald <dao@design-noir.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIndexedToHTML.h"
#include "nsNetUtil.h"
#include "netCore.h"
#include "nsStringStream.h"
#include "nsIFileURL.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "prtime.h"
#include "nsDateTimeFormatCID.h"
#include "nsURLHelper.h"
#include "nsCRT.h"
#include "nsIPlatformCharset.h"

NS_IMPL_ISUPPORTS4(nsIndexedToHTML,
                   nsIDirIndexListener,
                   nsIStreamConverter,
                   nsIRequestObserver,
                   nsIStreamListener)

static void AppendNonAsciiToNCR(const nsAString& in, nsAFlatString& out)
{
  nsAString::const_iterator start, end;

  in.BeginReading(start);
  in.EndReading(end);

  while (start != end) {
    if (*start < 128) {
      out.Append(*start++);
    } else {
      out.AppendLiteral("&#x");
      nsAutoString hex;
      hex.AppendInt(*start++, 16);
      out.Append(hex);
      out.Append((PRUnichar)';');
    }
  }
}

NS_METHOD
nsIndexedToHTML::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    nsresult rv;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    
    nsIndexedToHTML* _s = new nsIndexedToHTML();
    if (_s == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    rv = _s->QueryInterface(aIID, aResult);
    return rv;
}

nsresult
nsIndexedToHTML::Init(nsIStreamListener* aListener) {
    nsresult rv = NS_OK;

    mListener = aListener;

    mDateTime = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);

    nsCOMPtr<nsIStringBundleService> sbs =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = sbs->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(mBundle));

    mExpectAbsLoc = PR_FALSE;

    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::Convert(nsIInputStream* aFromStream,
                         const char* aFromType,
                         const char* aToType,
                         nsISupports* aCtxt,
                         nsIInputStream** res) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIndexedToHTML::AsyncConvertData(const char *aFromType,
                                  const char *aToType,
                                  nsIStreamListener *aListener,
                                  nsISupports *aCtxt) {
    return Init(aListener);
}

NS_IMETHODIMP
nsIndexedToHTML::OnStartRequest(nsIRequest* request, nsISupports *aContext) {
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    channel->SetContentType(NS_LITERAL_CSTRING("text/html"));

    mParser = do_CreateInstance("@mozilla.org/dirIndexParser;1",&rv);
    if (NS_FAILED(rv)) return rv;

    rv = mParser->SetListener(this);
    if (NS_FAILED(rv)) return rv;
    
    rv = mParser->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString baseUri, titleUri;
    rv = uri->GetAsciiSpec(baseUri);
    if (NS_FAILED(rv)) return rv;
    titleUri = baseUri;

    nsCString parentStr;

    // XXX - should be using the 300: line from the parser.
    // We can't guarantee that that comes before any entry, so we'd have to
    // buffer, and do other painful stuff.
    // I'll deal with this when I make the changes to handle welcome messages
    // The .. stuff should also come from the lower level protocols, but that
    // would muck up the XUL display
    // - bbaetz

    PRBool isScheme = PR_FALSE;
    PRBool isSchemeFile = PR_FALSE;
    PRBool isSchemeGopher = PR_FALSE;
    if (NS_SUCCEEDED(uri->SchemeIs("ftp", &isScheme)) && isScheme) {

        // strip out the password here, so it doesn't show in the page title
        // This is done by the 300: line generation in ftp, but we don't use
        // that - see above
        
        nsCAutoString pw;
        rv = uri->GetPassword(pw);
        if (NS_FAILED(rv)) return rv;
        if (!pw.IsEmpty()) {
             nsCOMPtr<nsIURI> newUri;
             rv = uri->Clone(getter_AddRefs(newUri));
             if (NS_FAILED(rv)) return rv;
             rv = newUri->SetPassword(EmptyCString());
             if (NS_FAILED(rv)) return rv;
             rv = newUri->GetAsciiSpec(titleUri);
             if (NS_FAILED(rv)) return rv;
        }

        nsCAutoString path;
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;

        if (!path.EqualsLiteral("//") && !path.LowerCaseEqualsLiteral("/%2f")) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."),parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    } else if (NS_SUCCEEDED(uri->SchemeIs("file", &isSchemeFile)) && isSchemeFile) {
        nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
        nsCOMPtr<nsIFile> file;
        rv = fileUrl->GetFile(getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file, &rv);
        if (NS_FAILED(rv)) return rv;
        lfile->SetFollowLinks(PR_TRUE);
        
        nsCAutoString url;
        rv = net_GetURLSpecFromFile(file, url);
        if (NS_FAILED(rv)) return rv;
        baseUri.Assign(url);
        
        nsCOMPtr<nsIFile> parent;
        rv = file->GetParent(getter_AddRefs(parent));
        
        if (parent && NS_SUCCEEDED(rv)) {
            net_GetURLSpecFromFile(parent, url);
            if (NS_FAILED(rv)) return rv;
            parentStr.Assign(url);
        }

        // Directory index will be always encoded in UTF-8 if this is file url
        rv = mParser->SetEncoding("UTF-8");
        NS_ENSURE_SUCCESS(rv, rv);

    } else if (NS_SUCCEEDED(uri->SchemeIs("gopher", &isSchemeGopher)) && isSchemeGopher) {
        mExpectAbsLoc = PR_TRUE;
    } else if (NS_SUCCEEDED(uri->SchemeIs("jar", &isScheme)) && isScheme) {
        nsCAutoString path;
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;

        // a top-level jar directory URL is of the form jar:foo.zip!/
        // path will be of the form foo.zip!/, and its last two characters
        // will be "!/"
        //XXX this won't work correctly when the name of the directory being
        //XXX displayed ends with "!", but then again, jar: URIs don't deal
        //XXX particularly well with such directories anyway
        if (!StringEndsWith(path, NS_LITERAL_CSTRING("!/"))) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."), parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        // default behavior for other protocols is to assume the channel's
        // URL references a directory ending in '/' -- fixup if necessary.
        nsCAutoString path;
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        if (baseUri.Last() != '/') {
            baseUri.Append('/');
            path.Append('/');
            uri->SetPath(path);
        }
        if (!path.EqualsLiteral("/")) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."), parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    }

    nsString buffer;
    buffer.AppendLiteral("<!DOCTYPE html>\n"
                         "<html>\n<head>\n"
                         "<meta http-equiv=\"content-type\" content=\"text/html; charset=");
    
    // Get the encoding from the parser
    // XXX - this won't work for any encoding set via a 301: line in the
    // format - this output stuff would need to move to OnDataAvailable
    // for that.

    nsXPIDLCString encoding;
    rv = mParser->GetEncoding(getter_Copies(encoding));
    if (NS_FAILED(rv)) return rv;

    AppendASCIItoUTF16(encoding, buffer);
    buffer.AppendLiteral("\">\n"
                         "<style type=\"text/css\">\n"
                         ":root {\n"
                         "  font-family: sans-serif;\n"
                         "}\n"
                         "img {\n"
                         "  border: 0;\n"
                         "}\n"
                         "th {\n"
                         "  text-align: left;\n"
                         "  white-space: nowrap;\n"
                         "}\n"
                         "th > a {\n"
                         "  color: inherit;\n"
                         "}\n"
                         "table[order] > thead > tr > th {\n"
                         "  cursor: pointer;\n"
                         "}\n"
                         "table[order] > thead > tr > th::after {\n"
                         "  display: none;\n"
                         "  width: .8em;\n"
                         "  margin-right: -.8em;\n"
                         "  text-align: right;\n"
                         "}\n"
                         "table[order=\"asc\"] > thead > tr > th::after {\n"
                         "  content: \"\\2193\"; /* DOWNWARDS ARROW (U+2193) */\n"
                         "}\n"
                         "table[order=\"desc\"] > thead > tr > th::after {\n"
                         "  content: \"\\2191\"; /* UPWARDS ARROW (U+2191) */\n"
                         "}\n"
                         "table[order][order-by=\"0\"] > thead > tr > th:first-child ,\n"
                         "table[order][order-by=\"1\"] > thead > tr > th:first-child + th ,\n"
                         "table[order][order-by=\"2\"] > thead > tr > th:first-child + th + th {\n"
                         "  text-decoration: underline;\n"
                         "}\n"
                         "table[order][order-by=\"0\"] > thead > tr > th:first-child::after ,\n"
                         "table[order][order-by=\"1\"] > thead > tr > th:first-child + th::after ,\n"
                         "table[order][order-by=\"2\"] > thead > tr > th:first-child + th + th::after {\n"
                         "  display: inline-block;\n"
                         "}\n"
                         "table.remove-hidden > tbody > tr.hidden-object {\n"
                         "  display: none;\n"
                         "}\n"
                         "td > a {\n"
                         "  display: block;\n"
                         "}\n"
                         "/* size */\n"
                         "td:first-child + td {\n"
                         "  text-align: right;\n"
                         "  padding-right: 2em;\n"
                         "  white-space: nowrap;\n"
                         "}\n"
                         "/* date */\n"
                         "td:first-child + td + td {\n"
                         "  padding-right: 1em;\n"
                         "  white-space: nowrap;\n"
                         "}\n"
                         "/* time */\n"
                         "td:last-child {\n"
                         "  white-space: nowrap;\n"
                         "}\n"
                         "@-moz-document url-prefix(gopher://) {\n"
                         "  td {\n"
                         "    white-space: pre !important;\n"
                         "    font-family: monospace;\n"
                         "  }\n"
                         "}\n"
                         ".symlink {\n"
                         "  font-style: italic;\n"
                         "}\n"
                         ".dir ,\n"
                         ".symlink ,\n"
                         ".file {\n"
                         "  padding: 0 .5em;\n"
                         "  margin-left: 20px;\n"
                         "}\n"
                         ".dir::before ,\n"
                         ".file > img {\n"
                         "  margin-right: 4px;\n"
                         "  margin-left: -20px;\n"
                         "  vertical-align: middle;\n"
                         "}\n"
                         ".dir::before {\n"
                         "  content: url(resource://gre/res/html/folder.png);\n"
                         "}\n"
                         "</style>\n"
                         "<link rel=\"stylesheet\" media=\"screen, projection\" type=\"text/css\""
                         " href=\"chrome://global/skin/dirListing/dirListing.css\">\n");

    if (!isSchemeGopher) {
        buffer.AppendLiteral("<script type=\"application/javascript\">\n"
                             "var gTable, gOrderBy, gTBody, gRows, gUI_showHidden;\n"
                             "document.addEventListener(\"DOMContentLoaded\", function() {\n"
                             "  gTable = document.getElementsByTagName(\"table\")[0];\n"
                             "  gTBody = gTable.tBodies[0];\n"
                             "  if (gTBody.rows.length < 2)\n"
                             "    return;\n"
                             "  gUI_showHidden = document.getElementById(\"UI_showHidden\");\n"
                             "  var headCells = gTable.tHead.rows[0].cells,\n"
                             "      hiddenObjects = false;\n"
                             "  function rowAction(i) {\n"
                             "    return function(event) {\n"
                             "      event.preventDefault();\n"
                             "      orderBy(i);\n"
                             "    }\n"
                             "  }\n"
                             "  for (var i = headCells.length - 1; i >= 0; i--) {\n"
                             "    var anchor = document.createElement(\"a\");\n"
                             "    anchor.href = \"\";\n"
                             "    anchor.appendChild(headCells[i].firstChild);\n"
                             "    headCells[i].appendChild(anchor);\n"
                             "    headCells[i].addEventListener(\"click\", rowAction(i), true);\n"
                             "  }\n"
                             "  if (gUI_showHidden) {\n"
                             "    gRows = Array.slice(gTBody.rows);\n"
                             "    hiddenObjects = gRows.some(function (row) row.className == \"hidden-object\");\n"
                             "  }\n"
                             "  gTable.setAttribute(\"order\", \"\");\n"
                             "  if (hiddenObjects) {\n"
                             "    gUI_showHidden.style.display = \"block\";\n"
                             "    updateHidden();\n"
                             "  }\n"
                             "}, \"false\");\n"
                             "function compareRows(rowA, rowB) {\n"
                             "  var a = rowA.cells[gOrderBy].getAttribute(\"sortable-data\") || \"\";\n"
                             "  var b = rowB.cells[gOrderBy].getAttribute(\"sortable-data\") || \"\";\n"
                             "  var intA = +a;\n"
                             "  var intB = +b;\n"
                             "  if (a == intA && b == intB) {\n"
                             "    a = intA;\n"
                             "    b = intB;\n"
                             "  } else {\n"
                             "    a = a.toLowerCase();\n"
                             "    b = b.toLowerCase();\n"
                             "  }\n"
                             "  if (a < b)\n"
                             "    return -1;\n"
                             "  if (a > b)\n"
                             "    return 1;\n"
                             "  return 0;\n"
                             "}\n"
                             "function orderBy(column) {\n"
                             "  if (!gRows)\n"
                             "    gRows = Array.slice(gTBody.rows);\n"
                             "  var order;\n"
                             "  if (gOrderBy == column) {\n"
                             "    order = gTable.getAttribute(\"order\") == \"asc\" ? \"desc\" : \"asc\";\n"
                             "  } else {\n"
                             "    order = \"asc\";\n"
                             "    gOrderBy = column;\n"
                             "    gTable.setAttribute(\"order-by\", column);\n"
                             "    gRows.sort(compareRows);\n"
                             "  }\n"
                             "  gTable.removeChild(gTBody);\n"
                             "  gTable.setAttribute(\"order\", order);\n"
                             "  if (order == \"asc\")\n"
                             "    for (var i = 0; i < gRows.length; i++)\n"
                             "      gTBody.appendChild(gRows[i]);\n"
                             "  else\n"
                             "    for (var i = gRows.length - 1; i >= 0; i--)\n"
                             "      gTBody.appendChild(gRows[i]);\n"
                             "  gTable.appendChild(gTBody);\n"
                             "}\n"
                             "function updateHidden() {\n"
                             "  gTable.className = gUI_showHidden.getElementsByTagName(\"input\")[0].checked ?\n"
                             "                     \"\" :\n"
                             "                     \"remove-hidden\";\n"
                             "}\n"
                             "</script>\n");
    }
    buffer.AppendLiteral("<link rel=\"icon\" type=\"image/png\" href=\"");
    nsCOMPtr<nsIURI> innerUri = NS_GetInnermostURI(uri);
    if (!innerUri)
        return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(innerUri));
    //XXX bug 388553: can't use skinnable icons here due to security restrictions
    if (fileURL) {
        //buffer.AppendLiteral("chrome://global/skin/dirListing/local.png");
        buffer.AppendLiteral("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB"
                             "AAAAAQCAYAAAAf8/9hAAAABGd BTUEAANbY1E9YMgAAAfRJR"
                             "EFUeNrFUk1IVFEU/t7MfU2pMRiGLVxYhGCgEoIbyZVGtFJXt"
                             "hIDQ7BlgtvQpQs1NWgTA5YbscjZZLQNZkRpnEBFRR0sf9FG5"
                             "zne9969z/PuvAHHEN114Xv33nfP+c453znA/15a9C1qhPS3c"
                             "S4cYaPQtgHCmmlhvqkHo2QjLyP4WN44+azgTk3Ow9Rg/URyK"
                             "y7hIOiRwrawTMSLHe/wJkvMHAdF/uB9wOzPIXj8cqQZvrLzA"
                             "RsGnhdP0P6ekHJJmGGgIcB8kKaZa2p9/jffay9Q+QDNdOokG"
                             "CoD109a9EnzKyhmwotzneAjCEUgrBOAp5GIRZD6Sy8sCEEVC"
                             "pGBdHd17wArrMXUkLaaTu1id30RHgHHn1gUvhsVePjU1Sd5Q"
                             "Qp+wLFI1zxsLU3jS1/jBuOU+fHhAZI7e6hq6aVwUTK0LyghA"
                             "Meh0rVqzIZ7EE+gW2WwGQ+htPYVmJ4ggt+5TlKJRJGFapxEC"
                             "ZZnx5yVXzPfh7/iB9N1qtlewK17r0n5cdVsUEng1CXBM47Zx"
                             "Ypw4q9D5FP70bc5ULo4ZjfzgbuPuqDxeaJ/kjHUPZxbUhj4G"
                             "R7G5s5hKDyDVfp1xKisWGS0tSqge9PmwVX/7D2L7X2Euj+g3"
                             "5sDrrmJEW4TAq5Mlw+DGmHDa5WVdWBXdD5LosQ5BeW364651"
                             "CZwAAAAAElFTkSuQmCC");
    } else {
        //buffer.AppendLiteral("chrome://global/skin/dirListing/remote.png");
        buffer.AppendLiteral("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB"
                             "AAAAAQCAYAAAAf8/9hAAAABGdBTUEAALGPC/xhBQAAAohJRE"
                             "FUeNqFU0tPE2EUPTMtr5G2DBCNYrVKxAXgKwgLNlYCiWGhG3"
                             "eICxNZuXBl/AEQggsNO4Jxb0gwokYxGmNgIRuMSRcGbMAoVV"
                             "NgBqSveX3e+01LwEicyZ37Te/cc859VMF/rrm5uWvkOm3bhu"
                             "M4YM9mWZaRyWTeKXslJhKJi5FIpL1QKMQqKjQUKAmeB8FBIa"
                             "DrNZiefmkE90i+Qq6DGPVwOIyl5RXJ7nkCnvAISMAlMALXgz"
                             "MzM1fpY42smixEFjEMQ+eEVCq1U7L06+sGAXno7rkkfwum0+"
                             "lMPN7VZZobkl3wLVilkMYvxIlaPYJnU0/R3HJWsnOMFIBLqO"
                             "LkOw/n6ahAoa4o/kN6tt+bmxi5eV4yfpz/ANd1cbD3sq+gxM"
                             "of6rV1UFUVCplaND4zgPB8xpZTbfIs/B5A5frpnQBUBAKBXa"
                             "YWPV/cPGZMh5bwyHwgyyopqODxKKqCQDC4zS49l6Cqfm8I4P"
                             "uhqYHlRWrmVppVjDEAR3VuCJfAjMcP7ENsv4aj9ZWI1lVKMF"
                             "8Br4GN7ubryFsFCchTYQWeWqyTLbnw78UKqAFYngOXS/EsOa"
                             "lSCfmysiCGb5yR47FtB9msz1BeXkZlCLl9qkKJtBuu5yLvWH"
                             "KMUoGu6yvJ5KJGi9PI4zFNkwCycvPW1tbkyNLHpgfyjo2G2h"
                             "PgZW6sb8attxcGclEqfafM0dHRjqamprYqrbo1FKrBi+dPkM"
                             "vlJil0OnlkciRadxKthzvx/ssbpMxfOPejt//v/0K8oSEaqw"
                             "6FSIGLvr5+jI+PxYeGhu5S7F7PfVNsWTnMfp7FwmBWeYVP2A"
                             "Wwurr6bWLicay0/1SGoWna11L89e2fSvvwhlgYzG0r/wN/4X"
                             "tnm2x4eQAAAABJRU5ErkJggg==");
    }
    buffer.AppendLiteral("\">\n<title>");

    // Anything but a gopher url needs to end in a /,
    // otherwise we end up linking to file:///foo/dirfile

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsXPIDLString unEscapeSpec;
    rv = mTextToSubURI->UnEscapeAndConvert(encoding, titleUri.get(),
                                           getter_Copies(unEscapeSpec));
    // unescape may fail because
    // 1. file URL may be encoded in platform charset for backward compatibility
    // 2. query part may not be encoded in UTF-8 (see bug 261929)
    // so try the platform's default if this is file url
    if (NS_FAILED(rv) && isSchemeFile) {
        nsCOMPtr<nsIPlatformCharset> platformCharset(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCAutoString charset;
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, charset);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mTextToSubURI->UnEscapeAndConvert(charset.get(), titleUri.get(),
                                               getter_Copies(unEscapeSpec));
    }
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString htmlEscSpec;
    htmlEscSpec.Adopt(nsEscapeHTML2(unEscapeSpec.get(),
                                    unEscapeSpec.Length()));

    nsXPIDLString title;
    const PRUnichar* formatTitle[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatTitle,
                                       sizeof(formatTitle)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;

    // we want to convert string bundle to NCR
    // to ensure they're shown in any charsets
    AppendNonAsciiToNCR(title, buffer);

    buffer.AppendLiteral("</title>\n");    

    // If there is a quote character in the baseUri, then
    // lets not add a base URL.  The reason for this is that
    // if we stick baseUri containing a quote into a quoted
    // string, the quote character will prematurely close
    // the base href string.  This is a fall-back check;
    // that's why it is OK to not use a base rather than
    // trying to play nice and escaping the quotes.  See bug
    // 358128.

    if (baseUri.FindChar('"') == kNotFound)
    {
        // Great, the baseUri does not contain a char that
        // will prematurely close the string.  Go ahead an
        // add a base href.
        buffer.AppendLiteral("<base href=\"");
        AppendASCIItoUTF16(baseUri, buffer);
        buffer.AppendLiteral("\">\n");
    }
    else
    {
        NS_ERROR("broken protocol handler didn't escape double-quote.");
    }

    buffer.AppendLiteral("</head>\n<body>\n<h1>");
    
    const PRUnichar* formatHeading[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatHeading,
                                       sizeof(formatHeading)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;
    
    AppendNonAsciiToNCR(title, buffer);
    buffer.AppendLiteral("</h1>\n");

    if (!parentStr.IsEmpty()) {
        nsXPIDLString parentText;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirGoUp").get(),
                                        getter_Copies(parentText));
        if (NS_FAILED(rv)) return rv;

        buffer.AppendLiteral("<p id=\"UI_goUp\"><a class=\"up\" href=\"");
        AppendASCIItoUTF16(parentStr, buffer);
        buffer.AppendLiteral("\">");
        AppendNonAsciiToNCR(parentText, buffer);
        buffer.AppendLiteral("</a></p>\n");
    }

    if (isSchemeFile) {
        nsXPIDLString showHiddenText;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("ShowHidden").get(),
                                        getter_Copies(showHiddenText));
        if (NS_FAILED(rv)) return rv;

        buffer.AppendLiteral("<p id=\"UI_showHidden\" style=\"display:none\"><label><input type=\"checkbox\" checked onchange=\"updateHidden()\">");
        AppendNonAsciiToNCR(showHiddenText, buffer);
        buffer.AppendLiteral("</label></p>\n");
    }

    buffer.AppendLiteral("<table>\n");

    if (!isSchemeGopher) {
        nsXPIDLString columnText;

        buffer.AppendLiteral(" <thead>\n"
                             "  <tr>\n"
                             "   <th>");

        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirColName").get(),
                                        getter_Copies(columnText));
        if (NS_FAILED(rv)) return rv;
        AppendNonAsciiToNCR(columnText, buffer);
        buffer.AppendLiteral("</th>\n"
                             "   <th>");

        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirColSize").get(),
                                        getter_Copies(columnText));
        if (NS_FAILED(rv)) return rv;
        AppendNonAsciiToNCR(columnText, buffer);
        buffer.AppendLiteral("</th>\n"
                             "   <th colspan=\"2\">");

        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirColMTime").get(),
                                        getter_Copies(columnText));
        if (NS_FAILED(rv)) return rv;
        AppendNonAsciiToNCR(columnText, buffer);
        buffer.AppendLiteral("</th>\n"
                             "  </tr>\n"
                             " </thead>\n");
    }
    buffer.AppendLiteral(" <tbody>\n");

    // Push buffer to the listener now, so the initial HTML will not
    // be parsed in OnDataAvailable().

    rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    // The request may have been canceled, and if that happens, we want to
    // suppress calls to OnDataAvailable.
    request->GetStatus(&rv);
    if (NS_FAILED(rv)) return rv;

    rv = FormatInputStream(request, aContext, buffer);
    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus) {
    if (NS_SUCCEEDED(aStatus)) {
        nsString buffer;
        buffer.AssignLiteral("</tbody></table></body></html>\n");

        aStatus = FormatInputStream(request, aContext, buffer);
    }

    mParser->OnStopRequest(request, aContext, aStatus);
    mParser = 0;
    
    return mListener->OnStopRequest(request, aContext, aStatus);
}

nsresult
nsIndexedToHTML::FormatInputStream(nsIRequest* aRequest, nsISupports *aContext, const nsAString &aBuffer) 
{
    nsresult rv = NS_OK;

    // set up unicode encoder
    if (!mUnicodeEncoder) {
      nsXPIDLCString encoding;
      rv = mParser->GetEncoding(getter_Copies(encoding));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICharsetConverterManager> charsetConverterManager;
        charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
        rv = charsetConverterManager->GetUnicodeEncoder(encoding.get(), 
                                                          getter_AddRefs(mUnicodeEncoder));
        if (NS_SUCCEEDED(rv))
            rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, 
                                                       nsnull, (PRUnichar)'?');
      }
    }

    // convert the data with unicode encoder
    char *buffer = nsnull;
    PRInt32 dstLength;
    if (NS_SUCCEEDED(rv)) {
      PRInt32 unicharLength = aBuffer.Length();
      rv = mUnicodeEncoder->GetMaxLength(PromiseFlatString(aBuffer).get(), 
                                         unicharLength, &dstLength);
      if (NS_SUCCEEDED(rv)) {
        buffer = (char *) nsMemory::Alloc(dstLength);
        NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

        rv = mUnicodeEncoder->Convert(PromiseFlatString(aBuffer).get(), &unicharLength, 
                                      buffer, &dstLength);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 finLen = 0;
          rv = mUnicodeEncoder->Finish(buffer + dstLength, &finLen);
          if (NS_SUCCEEDED(rv))
            dstLength += finLen;
        }
      }
    }

    // if conversion error then fallback to UTF-8
    if (NS_FAILED(rv)) {
      rv = NS_OK;
      if (buffer) {
        nsMemory::Free(buffer);
        buffer = nsnull;
      }
    }

    nsCOMPtr<nsIInputStream> inputData;
    if (buffer) {
      rv = NS_NewCStringInputStream(getter_AddRefs(inputData), Substring(buffer, buffer + dstLength));
      nsMemory::Free(buffer);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mListener->OnDataAvailable(aRequest, aContext,
                                      inputData, 0, dstLength);
    }
    else {
      NS_ConvertUTF16toUTF8 utf8Buffer(aBuffer);
      rv = NS_NewCStringInputStream(getter_AddRefs(inputData), utf8Buffer);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mListener->OnDataAvailable(aRequest, aContext,
                                      inputData, 0, utf8Buffer.Length());
    }
    return (rv);
}

NS_IMETHODIMP
nsIndexedToHTML::OnDataAvailable(nsIRequest *aRequest,
                                 nsISupports *aCtxt,
                                 nsIInputStream* aInput,
                                 PRUint32 aOffset,
                                 PRUint32 aCount) {
    return mParser->OnDataAvailable(aRequest, aCtxt, aInput, aOffset, aCount);
}

NS_IMETHODIMP
nsIndexedToHTML::OnIndexAvailable(nsIRequest *aRequest,
                                  nsISupports *aCtxt,
                                  nsIDirIndex *aIndex) {
    nsresult rv;
    if (!aIndex)
        return NS_ERROR_NULL_POINTER;

    nsString pushBuffer;
    pushBuffer.AppendLiteral("<tr");

    nsXPIDLString description;
    aIndex->GetDescription(getter_Copies(description));
    if (description.First() == PRUnichar('.'))
        pushBuffer.AppendLiteral(" class=\"hidden-object\"");

    pushBuffer.AppendLiteral(">\n <td sortable-data=\"");

    // The sort key is the name of the item, prepended by either 0, 1 or 2
    // in order to group items.
    PRUint32 type;
    aIndex->GetType(&type);
    switch (type) {
        case nsIDirIndex::TYPE_SYMLINK:
            pushBuffer.AppendInt(0);
            break;
        case nsIDirIndex::TYPE_DIRECTORY:
            pushBuffer.AppendInt(1);
            break;
        case nsIDirIndex::TYPE_FILE:
        case nsIDirIndex::TYPE_UNKNOWN:
            pushBuffer.AppendInt(2);
            break;
    }
    PRUnichar* escaped = nsEscapeHTML2(description.get(), description.Length());
    pushBuffer.Append(escaped);

    pushBuffer.AppendLiteral("\"><a class=\"");
    switch (type) {
        case nsIDirIndex::TYPE_DIRECTORY:
            pushBuffer.AppendLiteral("dir");
            break;
        case nsIDirIndex::TYPE_SYMLINK:
            pushBuffer.AppendLiteral("symlink");
            break;
        case nsIDirIndex::TYPE_FILE:
        case nsIDirIndex::TYPE_UNKNOWN:
            pushBuffer.AppendLiteral("file");
            break;
    }
    pushBuffer.AppendLiteral("\"");

    // Truncate long names to not stretch the table
    //XXX this should be left to the stylesheet (bug 391471)
    nsString escapedShort;
    if (description.Length() > 71) {
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
        nsCOMPtr<nsIURI> uri;
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        // No need to do this for Gopher, as the table has only one column in that case
        PRBool isSchemeGopher = PR_FALSE;
        if (!(NS_SUCCEEDED(uri->SchemeIs("gopher", &isSchemeGopher)) && isSchemeGopher)) {
            //XXX this potentially truncates after a combining char (bug 391472)
            nsXPIDLString descriptionAffix;
            descriptionAffix.Assign(description);
            descriptionAffix.Cut(0, descriptionAffix.Length() - 25);
            if (NS_IS_LOW_SURROGATE(descriptionAffix.First()))
                descriptionAffix.Cut(0, 1);
            description.Truncate(PR_MIN(71, description.Length() - 28));
            if (NS_IS_HIGH_SURROGATE(description.Last()))
                description.Truncate(description.Length() - 1);

            escapedShort.Adopt(nsEscapeHTML2(description.get(), description.Length()));
            // add HORIZONTAL ELLIPSIS (U+2026)
            escapedShort.AppendLiteral("&#8230;");
            nsString tmp;
            tmp.Adopt(nsEscapeHTML2(descriptionAffix.get(), descriptionAffix.Length()));
            escapedShort.Append(tmp);

            pushBuffer.AppendLiteral(" title=\"");
            pushBuffer.Append(escaped);
            pushBuffer.AppendLiteral("\"");
        }
    }
    if (escapedShort.IsEmpty())
        escapedShort.Assign(escaped);
    nsMemory::Free(escaped);

    pushBuffer.AppendLiteral(" href=\"");
    nsXPIDLCString loc;
    aIndex->GetLocation(getter_Copies(loc));

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsXPIDLCString encoding;
    rv = mParser->GetEncoding(getter_Copies(encoding));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString unEscapeSpec;
    rv = mTextToSubURI->UnEscapeAndConvert(encoding, loc,
                                           getter_Copies(unEscapeSpec));
    if (NS_FAILED(rv)) return rv;

    // need to escape links
    nsCAutoString escapeBuf;

    NS_ConvertUTF16toUTF8 utf8UnEscapeSpec(unEscapeSpec);

    // now minimally re-escape the location...
    PRUint32 escFlags;
    // for some protocols, like gopher, we expect the location to be absolute.
    // if so, and if the location indeed appears to be a valid URI, then go
    // ahead and treat it like one.
    if (mExpectAbsLoc &&
        NS_SUCCEEDED(net_ExtractURLScheme(utf8UnEscapeSpec, nsnull, nsnull, nsnull))) {
        // escape as absolute 
        escFlags = esc_Forced | esc_OnlyASCII | esc_AlwaysCopy | esc_Minimal;
    }
    else {
        // escape as relative
        // esc_Directory is needed for protocols which allow the same name for
        // both a directory and a file and distinguish between the two by a
        // trailing '/' -- without it, the trailing '/' will be escaped, and
        // links from within that directory will be incorrect
        escFlags = esc_Forced | esc_OnlyASCII | esc_AlwaysCopy | esc_FileBaseName | esc_Colon | esc_Directory;
    }
    NS_EscapeURL(utf8UnEscapeSpec.get(), utf8UnEscapeSpec.Length(), escFlags, escapeBuf);

    AppendUTF8toUTF16(escapeBuf, pushBuffer);

    pushBuffer.AppendLiteral("\">");

    if (type == nsIDirIndex::TYPE_FILE || type == nsIDirIndex::TYPE_UNKNOWN) {
        pushBuffer.AppendLiteral("<img src=\"moz-icon://");
        PRInt32 lastDot = escapeBuf.RFindChar('.');
        if (lastDot != kNotFound) {
            escapeBuf.Cut(0, lastDot);
            AppendUTF8toUTF16(escapeBuf, pushBuffer);
        } else {
            pushBuffer.AppendLiteral("unknown");
        }
        pushBuffer.AppendLiteral("?size=16\" alt=\"");

        nsXPIDLString altText;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirFileLabel").get(),
                                        getter_Copies(altText));
        if (NS_FAILED(rv)) return rv;
        AppendNonAsciiToNCR(altText, pushBuffer);
        pushBuffer.AppendLiteral("\">");
    }

    pushBuffer.Append(escapedShort);
    pushBuffer.AppendLiteral("</a></td>\n <td");

    if (type == nsIDirIndex::TYPE_DIRECTORY || type == nsIDirIndex::TYPE_SYMLINK) {
        pushBuffer.AppendLiteral(">");
    } else {
        PRInt64 size;
        aIndex->GetSize(&size);

        if (PRUint64(size) != LL_MAXUINT) {
            pushBuffer.AppendLiteral(" sortable-data=\"");
            pushBuffer.AppendInt(size);
            pushBuffer.AppendLiteral("\">");
            nsAutoString  sizeString;
            FormatSizeString(size, sizeString);
            pushBuffer.Append(sizeString);
        } else {
            pushBuffer.AppendLiteral(">");
        }
    }
    pushBuffer.AppendLiteral("</td>\n <td");

    PRTime t;
    aIndex->GetLastModified(&t);

    if (t == -1) {
        pushBuffer.AppendLiteral("></td>\n <td>");
    } else {
        pushBuffer.AppendLiteral(" sortable-data=\"");
        pushBuffer.AppendInt(t);
        pushBuffer.AppendLiteral("\">");
        nsAutoString formatted;
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatShort,
                                kTimeFormatNone,
                                t,
                                formatted);
        AppendNonAsciiToNCR(formatted, pushBuffer);
        pushBuffer.AppendLiteral("</td>\n <td>");
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatNone,
                                kTimeFormatSeconds,
                                t,
                                formatted);
        // use NCR to show date in any doc charset
        AppendNonAsciiToNCR(formatted, pushBuffer);
    }

    pushBuffer.AppendLiteral("</td>\n</tr>");

    return FormatInputStream(aRequest, aCtxt, pushBuffer);
}

NS_IMETHODIMP
nsIndexedToHTML::OnInformationAvailable(nsIRequest *aRequest,
                                        nsISupports *aCtxt,
                                        const nsAString& aInfo) {
    nsAutoString pushBuffer;
    PRUnichar* escaped = nsEscapeHTML2(PromiseFlatString(aInfo).get());
    if (!escaped)
        return NS_ERROR_OUT_OF_MEMORY;
    pushBuffer.AppendLiteral("<tr>\n <td>");
    pushBuffer.Append(escaped);
    nsMemory::Free(escaped);
    pushBuffer.AppendLiteral("</td>\n <td></td>\n <td></td>\n <td></td>\n</tr>\n");
    
    return FormatInputStream(aRequest, aCtxt, pushBuffer);
}

void nsIndexedToHTML::FormatSizeString(PRInt64 inSize, nsString& outSizeString)
{
    outSizeString.Truncate();
    if (inSize > PRInt64(0)) {
        // round up to the nearest Kilobyte
        PRInt64  upperSize = (inSize + PRInt64(1023)) / PRInt64(1024);
        outSizeString.AppendInt(upperSize);
        outSizeString.AppendLiteral(" KB");
    }
}

nsIndexedToHTML::nsIndexedToHTML() {
}

nsIndexedToHTML::~nsIndexedToHTML() {
}
