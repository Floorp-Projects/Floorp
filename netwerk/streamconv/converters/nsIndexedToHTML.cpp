/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIndexedToHTML.h"

#include "DateTimeFormat.h"
#include "mozilla/Encoding.h"
#include "mozilla/intl/LocaleService.h"
#include "nsNetUtil.h"
#include "netCore.h"
#include "nsStringStream.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "nsURLHelper.h"
#include "nsIPlatformCharset.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIStringBundle.h"
#include "nsITextToSubURI.h"
#include "nsXPIDLString.h"
#include <algorithm>
#include "nsIChannel.h"

using mozilla::intl::LocaleService;

NS_IMPL_ISUPPORTS(nsIndexedToHTML,
                  nsIDirIndexListener,
                  nsIStreamConverter,
                  nsIRequestObserver,
                  nsIStreamListener)

static void AppendNonAsciiToNCR(const nsAString& in, nsCString& out)
{
  nsAString::const_iterator start, end;

  in.BeginReading(start);
  in.EndReading(end);

  while (start != end) {
    if (*start < 128) {
      out.Append(*start++);
    } else {
      out.AppendLiteral("&#x");
      out.AppendInt(*start++, 16);
      out.Append(';');
    }
  }
}

nsresult
nsIndexedToHTML::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    nsresult rv;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsIndexedToHTML* _s = new nsIndexedToHTML();
    if (_s == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = _s->QueryInterface(aIID, aResult);
    return rv;
}

nsresult
nsIndexedToHTML::Init(nsIStreamListener* aListener) {
    nsresult rv = NS_OK;

    mListener = aListener;

    nsCOMPtr<nsIStringBundleService> sbs =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = sbs->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(mBundle));

    mExpectAbsLoc = false;

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
    nsCString buffer;
    nsresult rv = DoOnStartRequest(request, aContext, buffer);
    if (NS_FAILED(rv)) {
        request->Cancel(rv);
    }

    rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    // The request may have been canceled, and if that happens, we want to
    // suppress calls to OnDataAvailable.
    request->GetStatus(&rv);
    if (NS_FAILED(rv)) return rv;

    // Push our buffer to the listener.

    rv = SendToListener(request, aContext, buffer);
    return rv;
}

nsresult
nsIndexedToHTML::DoOnStartRequest(nsIRequest* request, nsISupports *aContext,
                                  nsCString& aBuffer) {
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetOriginalURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    bool isResource = false;
    rv = uri->SchemeIs("resource", &isResource);
    if (NS_FAILED(rv)) return rv;

    // We use the original URI for the title and parent link when it's a
    // resource:// url, instead of the jar:file:// url it resolves to.
    if (!isResource) {
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;
    }

    channel->SetContentType(NS_LITERAL_CSTRING("text/html"));

    mParser = do_CreateInstance("@mozilla.org/dirIndexParser;1",&rv);
    if (NS_FAILED(rv)) return rv;

    rv = mParser->SetListener(this);
    if (NS_FAILED(rv)) return rv;

    rv = mParser->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    nsAutoCString baseUri, titleUri;
    rv = uri->GetAsciiSpec(baseUri);
    if (NS_FAILED(rv)) return rv;
    titleUri = baseUri;

    nsCString parentStr;

    nsCString buffer;
    buffer.AppendLiteral("<!DOCTYPE html>\n<html>\n<head>\n");

    // XXX - should be using the 300: line from the parser.
    // We can't guarantee that that comes before any entry, so we'd have to
    // buffer, and do other painful stuff.
    // I'll deal with this when I make the changes to handle welcome messages
    // The .. stuff should also come from the lower level protocols, but that
    // would muck up the XUL display
    // - bbaetz

    bool isScheme = false;
    bool isSchemeFile = false;
    if (NS_SUCCEEDED(uri->SchemeIs("ftp", &isScheme)) && isScheme) {

        // strip out the password here, so it doesn't show in the page title
        // This is done by the 300: line generation in ftp, but we don't use
        // that - see above

        nsAutoCString pw;
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

        nsAutoCString path;
        rv = uri->GetPathQueryRef(path);
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
        file->SetFollowLinks(true);

        nsAutoCString url;
        rv = net_GetURLSpecFromFile(file, url);
        if (NS_FAILED(rv)) return rv;
        baseUri.Assign(url);

        nsCOMPtr<nsIFile> parent;
        rv = file->GetParent(getter_AddRefs(parent));

        if (parent && NS_SUCCEEDED(rv)) {
            net_GetURLSpecFromDir(parent, url);
            if (NS_FAILED(rv)) return rv;
            parentStr.Assign(url);
        }

        // Directory index will be always encoded in UTF-8 if this is file url
        buffer.AppendLiteral("<meta charset=\"UTF-8\">\n");

    } else if (NS_SUCCEEDED(uri->SchemeIs("jar", &isScheme)) && isScheme) {
        nsAutoCString path;
        rv = uri->GetPathQueryRef(path);
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
        nsAutoCString path;
        rv = uri->GetPathQueryRef(path);
        if (NS_FAILED(rv)) return rv;
        if (baseUri.Last() != '/') {
            baseUri.Append('/');
            path.Append('/');
            uri->SetPathQueryRef(path);
        }
        if (!path.EqualsLiteral("/")) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."), parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    }

    buffer.AppendLiteral("<style type=\"text/css\">\n"
                         ":root {\n"
                         "  font-family: sans-serif;\n"
                         "}\n"
                         "img {\n"
                         "  border: 0;\n"
                         "}\n"
                         "th {\n"
                         "  text-align: start;\n"
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
                         "  margin-inline-end: -.8em;\n"
                         "  text-align: end;\n"
                         "}\n"
                         "table[order=\"asc\"] > thead > tr > th::after {\n"
                         "  content: \"\\2193\"; /* DOWNWARDS ARROW (U+2193) */\n"
                         "}\n"
                         "table[order=\"desc\"] > thead > tr > th::after {\n"
                         "  content: \"\\2191\"; /* UPWARDS ARROW (U+2191) */\n"
                         "}\n"
                         "table[order][order-by=\"0\"] > thead > tr > th:first-child > a ,\n"
                         "table[order][order-by=\"1\"] > thead > tr > th:first-child + th > a ,\n"
                         "table[order][order-by=\"2\"] > thead > tr > th:first-child + th + th > a {\n"
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
                         "td {\n"
                         "  white-space: nowrap;\n"
                         "}\n"
                         "table.ellipsis {\n"
                         "  width: 100%;\n"
                         "  table-layout: fixed;\n"
                         "  border-spacing: 0;\n"
                         "}\n"
                         "table.ellipsis > tbody > tr > td {\n"
                         "  padding: 0;\n"
                         "  overflow: hidden;\n"
                         "  text-overflow: ellipsis;\n"
                         "}\n"
                         "/* name */\n"
                         "/* name */\n"
                         "th:first-child {\n"
                         "  padding-inline-end: 2em;\n"
                         "}\n"
                         "/* size */\n"
                         "th:first-child + th {\n"
                         "  padding-inline-end: 1em;\n"
                         "}\n"
                         "td:first-child + td {\n"
                         "  text-align: end;\n"
                         "  padding-inline-end: 1em;\n"
                         "}\n"
                         "/* date */\n"
                         "td:first-child + td + td {\n"
                         "  padding-inline-start: 1em;\n"
                         "  padding-inline-end: .5em;\n"
                         "}\n"
                         "/* time */\n"
                         "td:first-child + td + td + td {\n"
                         "  padding-inline-start: .5em;\n"
                         "}\n"
                         ".symlink {\n"
                         "  font-style: italic;\n"
                         "}\n"
                         ".dir ,\n"
                         ".symlink ,\n"
                         ".file {\n"
                         "  margin-inline-start: 20px;\n"
                         "}\n"
                         ".dir::before ,\n"
                         ".file > img {\n"
                         "  margin-inline-end: 4px;\n"
                         "  margin-inline-start: -20px;\n"
                         "  max-width: 16px;\n"
                         "  max-height: 16px;\n"
                         "  vertical-align: middle;\n"
                         "}\n"
                         ".dir::before {\n"
                         "  content: url(resource://gre/res/html/folder.png);\n"
                         "}\n"
                         "</style>\n"
                         "<link rel=\"stylesheet\" media=\"screen, projection\" type=\"text/css\""
                         " href=\"chrome://global/skin/dirListing/dirListing.css\">\n"
                         "<script type=\"application/javascript\">\n"
                         "'use strict';\n"
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
                         "    hiddenObjects = gRows.some(row => row.className == \"hidden-object\");\n"
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

    buffer.AppendLiteral("<link rel=\"icon\" type=\"image/png\" href=\"");
    nsCOMPtr<nsIURI> innerUri = NS_GetInnermostURI(uri);
    if (!innerUri)
        return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(innerUri));
    //XXX bug 388553: can't use skinnable icons here due to security restrictions
    if (fileURL) {
        buffer.AppendLiteral("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB"
                             "AAAAAQCAYAAAAf8%2F9hAAAAGXRFWHRTb2Z0d2FyZQBBZG9i"
                             "ZSBJbWFnZVJlYWR5ccllPAAAAjFJREFUeNqsU8uOElEQPffR"
                             "3XQ3ONASdBJCSBxHos5%2B3Bg3rvkCv8PElS78gPkO%2FATj"
                             "QoUdO2ftrJiRh6aneTb9sOpC4weMN6lcuFV16pxDIfI8x12O"
                             "YIDhcPiu2Wx%2B%2FHF5CW1Z6Jyegt%2FTNEWSJIjjGFEUIQ"
                             "xDrFYrWFSzXC4%2FdLvd95pRKpXKy%2BpRFZ7nwaWo1%2BsG"
                             "nQG2260BKJfLKJVKGI1GEEJw7ateryd0v993W63WEwjgxfn5"
                             "obGYzgCbzcaEbdsIggDj8Riu6z6iUk9SYZMSx8W0LMsM%2FS"
                             "KK75xnJlIq80anQXdbEp0OhcPJ0eiaJnGRMEyyPDsAKKUM9c"
                             "lkYoDo3SZJzzSdp0VSKYmfV1co%2Bz580kw5KDIM8RbRfEnU"
                             "f1HzxtQyMAGcaGruTKczMzEIaqhKifV6jd%2BzGQQB5llunF"
                             "%2FM52BizC2K5sYPYvZcu653tjOM9O93wnYc08gmkgg4VAxi"
                             "xfqFUJT36AYBZGd6PJkFCZnnlBxMp38gqIgLpZB0y4Nph18l"
                             "yWh5FFbrOSxbl3V4G%2BVB7T4ajYYxTyuLtO%2BCvWGgJE1M"
                             "c7JNsJEhvgw%2FQV4fo%2F24nbEsX2u1d5sVyn8sJO0ZAQiI"
                             "YnFh%2BxrfLz%2Fj29cBS%2FO14zg3i8XigW3ZkErDtmKoeM"
                             "%2BAJGRMnXeEPGKf0nCD1ydvkDzU9Jbc6OpR7WIw6L8lQ%2B"
                             "4pQ1%2FlPF0RGM9Ns91Wmptk0GfB4EJkt77vXYj%2F8m%2B8"
                             "y%2FkrwABHbz2H9V68DQAAAABJRU5ErkJggg%3D%3D");
    } else {
        buffer.AppendLiteral("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB"
                             "AAAAAQCAYAAAAf8%2F9hAAAAGXRFWHRTb2Z0d2FyZQBBZG9i"
                             "ZSBJbWFnZVJlYWR5ccllPAAAAeBJREFUeNqcU81O20AQ%2Ft"
                             "Z2AgQSYQRqL1UPVG2hAUQkxLEStz4DrXpLpD5Drz31Cajax%"
                             "2Bghhx6qHIJURBTxIwQRwopCBbZjHMcOTrzermPipsSt1Iw0"
                             "3p3ZmW%2B%2B2R0TxhgOD34wjCHZlQ0iDYz9yvEfhxMTCYhE"
                             "QDIZhkxKd2sqzX2TOD2vBQCQhpPefng1ZP2dVPlLLdpL8SEM"
                             "cxng%2Fbs0RIHhtgs4twxOh%2BHjZxvzDx%2F3GQQiDFISiR"
                             "BLFMPKTRMollzcWECrDVhtxtdRVsL9youPxGj%2FbdfFlUZh"
                             "tDyYbYqWRUdai1oQRZ5oHeHl2gNM%2B01Uqio8RlH%2Bnsaz"
                             "JzNwXcq1B%2BiXPHprlEEymeBfXs1w8XxxihfyuXqoHqpoGj"
                             "ZM04bddgG%2F9%2B8WGj87qDdsrK9m%2BoA%2BpbhQTDh2l1"
                             "%2Bi2weNbSHMZyjvNXmVbqh9Fj5Oz27uEoP%2BSTxANruJs9"
                             "L%2FT6P0ewqPx5nmiAG5f6AoCtN1PbJzuRyJAyDBzzSQYvEr"
                             "f06yYxhGXlEa8H2KVGoasjwLx3Ewk858opQWXm%2B%2Fib9E"
                             "QrBzclLLLy89xYvlpchvtixcX6uo1y%2FzsiwHrkIsgKbp%2"
                             "BYWFOWicuqppoNTnStHzPFCPQhBEBOyGAX4JMADFetubi4BS"
                             "YAAAAABJRU5ErkJggg%3D%3D");
    }
    buffer.AppendLiteral("\">\n<title>");

    // Everything needs to end in a /,
    // otherwise we end up linking to file:///foo/dirfile

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsAutoString unEscapeSpec;
    rv = mTextToSubURI->UnEscapeAndConvert(NS_LITERAL_CSTRING("UTF-8"), titleUri, unEscapeSpec);
    // unescape may fail because
    // 1. file URL may be encoded in platform charset for backward compatibility
    // 2. query part may not be encoded in UTF-8 (see bug 261929)
    // so try the platform's default if this is file url
    if (NS_FAILED(rv) && isSchemeFile) {
        nsCOMPtr<nsIPlatformCharset> platformCharset(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        nsAutoCString charset;
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, charset);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mTextToSubURI->UnEscapeAndConvert(charset, titleUri, unEscapeSpec);
    }
    if (NS_FAILED(rv)) return rv;

    nsString htmlEscSpec;
    htmlEscSpec.Adopt(nsEscapeHTML2(unEscapeSpec.get(),
                                    unEscapeSpec.Length()));

    nsAutoString title;
    const char16_t* formatTitle[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName("DirTitle",
                                       formatTitle,
                                       sizeof(formatTitle)/sizeof(char16_t*),
                                       title);
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

    if (!baseUri.Contains('"'))
    {
        // Great, the baseUri does not contain a char that
        // will prematurely close the string.  Go ahead an
        // add a base href, but only do so if we're not
        // dealing with a resource URI.
        if (!isResource) {
            buffer.AppendLiteral("<base href=\"");
            nsCString htmlEscapedUri;
            htmlEscapedUri.Adopt(nsEscapeHTML(baseUri.get()));
            buffer.Append(htmlEscapedUri);
            buffer.AppendLiteral("\" />\n");
        }
    }
    else
    {
        NS_ERROR("broken protocol handler didn't escape double-quote.");
    }

    nsCString direction(NS_LITERAL_CSTRING("ltr"));
    if (LocaleService::GetInstance()->IsAppLocaleRTL()) {
      direction.AssignLiteral("rtl");
    }

    buffer.AppendLiteral("</head>\n<body dir=\"");
    buffer.Append(direction);
    buffer.AppendLiteral("\">\n<h1>");

    const char16_t* formatHeading[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName("DirTitle",
                                       formatHeading,
                                       sizeof(formatHeading)/sizeof(char16_t*),
                                       title);
    if (NS_FAILED(rv)) return rv;

    AppendNonAsciiToNCR(title, buffer);
    buffer.AppendLiteral("</h1>\n");

    if (!parentStr.IsEmpty()) {
        nsAutoString parentText;
        rv = mBundle->GetStringFromName("DirGoUp", parentText);
        if (NS_FAILED(rv)) return rv;

        buffer.AppendLiteral("<p id=\"UI_goUp\"><a class=\"up\" href=\"");

        nsCString htmlParentStr;
        htmlParentStr.Adopt(nsEscapeHTML(parentStr.get()));
        buffer.Append(htmlParentStr);
        buffer.AppendLiteral("\">");
        AppendNonAsciiToNCR(parentText, buffer);
        buffer.AppendLiteral("</a></p>\n");
    }

    if (isSchemeFile) {
        nsAutoString showHiddenText;
        rv = mBundle->GetStringFromName("ShowHidden", showHiddenText);
        if (NS_FAILED(rv)) return rv;

        buffer.AppendLiteral("<p id=\"UI_showHidden\" style=\"display:none\"><label><input type=\"checkbox\" checked onchange=\"updateHidden()\">");
        AppendNonAsciiToNCR(showHiddenText, buffer);
        buffer.AppendLiteral("</label></p>\n");
    }

    buffer.AppendLiteral("<table>\n"
                         " <thead>\n"
                         "  <tr>\n"
                         "   <th>");

    nsAutoString columnText;
    rv = mBundle->GetStringFromName("DirColName", columnText);
    if (NS_FAILED(rv)) return rv;
    AppendNonAsciiToNCR(columnText, buffer);
    buffer.AppendLiteral("</th>\n"
                         "   <th>");

    rv = mBundle->GetStringFromName("DirColSize", columnText);
    if (NS_FAILED(rv)) return rv;
    AppendNonAsciiToNCR(columnText, buffer);
    buffer.AppendLiteral("</th>\n"
                         "   <th colspan=\"2\">");

    rv = mBundle->GetStringFromName("DirColMTime", columnText);
    if (NS_FAILED(rv)) return rv;
    AppendNonAsciiToNCR(columnText, buffer);
    buffer.AppendLiteral("</th>\n"
                         "  </tr>\n"
                         " </thead>\n");
    buffer.AppendLiteral(" <tbody>\n");

    aBuffer = buffer;
    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus) {
    if (NS_SUCCEEDED(aStatus)) {
        nsCString buffer;
        buffer.AssignLiteral("</tbody></table></body></html>\n");

        aStatus = SendToListener(request, aContext, buffer);
    }

    mParser->OnStopRequest(request, aContext, aStatus);
    mParser = nullptr;

    return mListener->OnStopRequest(request, aContext, aStatus);
}

nsresult
nsIndexedToHTML::SendToListener(nsIRequest* aRequest, nsISupports *aContext, const nsACString &aBuffer)
{
    nsCOMPtr<nsIInputStream> inputData;
    nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inputData), aBuffer);
    NS_ENSURE_SUCCESS(rv, rv);
    return mListener->OnDataAvailable(aRequest, aContext,
                                      inputData, 0, aBuffer.Length());
}

NS_IMETHODIMP
nsIndexedToHTML::OnDataAvailable(nsIRequest *aRequest,
                                 nsISupports *aCtxt,
                                 nsIInputStream* aInput,
                                 uint64_t aOffset,
                                 uint32_t aCount) {
    return mParser->OnDataAvailable(aRequest, aCtxt, aInput, aOffset, aCount);
}

NS_IMETHODIMP
nsIndexedToHTML::OnIndexAvailable(nsIRequest *aRequest,
                                  nsISupports *aCtxt,
                                  nsIDirIndex *aIndex) {
    nsresult rv;
    if (!aIndex)
        return NS_ERROR_NULL_POINTER;

    nsCString pushBuffer;
    pushBuffer.AppendLiteral("<tr");

    // We don't know the file's character set yet, so retrieve the raw bytes
    // which will be decoded by the HTML parser.
    nsXPIDLCString loc;
    aIndex->GetLocation(getter_Copies(loc));

    // Adjust the length in case unescaping shortened the string.
    loc.Truncate(nsUnescapeCount(loc.BeginWriting()));

    if (loc.IsEmpty()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    if (loc.First() == char16_t('.'))
        pushBuffer.AppendLiteral(" class=\"hidden-object\"");

    pushBuffer.AppendLiteral(">\n <td sortable-data=\"");

    // The sort key is the name of the item, prepended by either 0, 1 or 2
    // in order to group items.
    uint32_t type;
    aIndex->GetType(&type);
    switch (type) {
        case nsIDirIndex::TYPE_SYMLINK:
            pushBuffer.Append('0');
            break;
        case nsIDirIndex::TYPE_DIRECTORY:
            pushBuffer.Append('1');
            break;
        default:
            pushBuffer.Append('2');
            break;
    }
    nsCString escaped;
    escaped.Adopt(nsEscapeHTML(loc));
    pushBuffer.Append(escaped);

    pushBuffer.AppendLiteral("\"><table class=\"ellipsis\"><tbody><tr><td><a class=\"");
    switch (type) {
        case nsIDirIndex::TYPE_DIRECTORY:
            pushBuffer.AppendLiteral("dir");
            break;
        case nsIDirIndex::TYPE_SYMLINK:
            pushBuffer.AppendLiteral("symlink");
            break;
        default:
            pushBuffer.AppendLiteral("file");
            break;
    }

    pushBuffer.AppendLiteral("\" href=\"");

    // need to escape links
    nsAutoCString locEscaped;

    // Adding trailing slash helps to recognize whether the URL points to a file
    // or a directory (bug #214405).
    if ((type == nsIDirIndex::TYPE_DIRECTORY) && (loc.Last() != '/')) {
        loc.Append('/');
    }

    // now minimally re-escape the location...
    uint32_t escFlags;
    // for some protocols, we expect the location to be absolute.
    // if so, and if the location indeed appears to be a valid URI, then go
    // ahead and treat it like one.

    nsAutoCString scheme;
    if (mExpectAbsLoc &&
        NS_SUCCEEDED(net_ExtractURLScheme(loc, scheme))) {
        // escape as absolute
        escFlags = esc_Forced | esc_AlwaysCopy | esc_Minimal;
    }
    else {
        // escape as relative
        // esc_Directory is needed because directories have a trailing slash.
        // Without it, the trailing '/' will be escaped, and links from within
        // that directory will be incorrect
        escFlags = esc_Forced | esc_AlwaysCopy | esc_FileBaseName | esc_Colon | esc_Directory;
    }
    NS_EscapeURL(loc.get(), loc.Length(), escFlags, locEscaped);
    // esc_Directory does not escape the semicolons, so if a filename
    // contains semicolons we need to manually escape them.
    // This replacement should be removed in bug #473280
    locEscaped.ReplaceSubstring(";", "%3b");
    nsCString htmlEscapedURL;
    htmlEscapedURL.Adopt(nsEscapeHTML(locEscaped.get()));
    pushBuffer.Append(htmlEscapedURL);

    pushBuffer.AppendLiteral("\">");

    if (type == nsIDirIndex::TYPE_FILE || type == nsIDirIndex::TYPE_UNKNOWN) {
        pushBuffer.AppendLiteral("<img src=\"moz-icon://");
        int32_t lastDot = locEscaped.RFindChar('.');
        if (lastDot != kNotFound) {
            locEscaped.Cut(0, lastDot);
            nsCString htmlFileExt;
            htmlFileExt.Adopt(nsEscapeHTML(locEscaped.get()));
            pushBuffer.Append(htmlFileExt);
        } else {
            pushBuffer.AppendLiteral("unknown");
        }
        pushBuffer.AppendLiteral("?size=16\" alt=\"");

        nsAutoString altText;
        rv = mBundle->GetStringFromName("DirFileLabel", altText);
        if (NS_FAILED(rv)) return rv;
        AppendNonAsciiToNCR(altText, pushBuffer);
        pushBuffer.AppendLiteral("\">");
    }

    pushBuffer.Append(escaped);
    pushBuffer.AppendLiteral("</a></td></tr></tbody></table></td>\n <td");

    if (type == nsIDirIndex::TYPE_DIRECTORY || type == nsIDirIndex::TYPE_SYMLINK) {
        pushBuffer.Append('>');
    } else {
        int64_t size;
        aIndex->GetSize(&size);

        if (uint64_t(size) != UINT64_MAX) {
            pushBuffer.AppendLiteral(" sortable-data=\"");
            pushBuffer.AppendInt(size);
            pushBuffer.AppendLiteral("\">");
            nsAutoCString sizeString;
            FormatSizeString(size, sizeString);
            pushBuffer.Append(sizeString);
        } else {
            pushBuffer.Append('>');
        }
    }
    pushBuffer.AppendLiteral("</td>\n <td");

    PRTime t;
    aIndex->GetLastModified(&t);

    if (t == -1LL) {
        pushBuffer.AppendLiteral("></td>\n <td>");
    } else {
        pushBuffer.AppendLiteral(" sortable-data=\"");
        pushBuffer.AppendInt(static_cast<int64_t>(t));
        pushBuffer.AppendLiteral("\">");
        nsAutoString formatted;
        mozilla::DateTimeFormat::FormatPRTime(kDateFormatShort,
                                              kTimeFormatNone,
                                              t,
                                              formatted);
        AppendNonAsciiToNCR(formatted, pushBuffer);
        pushBuffer.AppendLiteral("</td>\n <td>");
        mozilla::DateTimeFormat::FormatPRTime(kDateFormatNone,
                                              kTimeFormatSeconds,
                                              t,
                                              formatted);
        // use NCR to show date in any doc charset
        AppendNonAsciiToNCR(formatted, pushBuffer);
    }

    pushBuffer.AppendLiteral("</td>\n</tr>");

    return SendToListener(aRequest, aCtxt, pushBuffer);
}

NS_IMETHODIMP
nsIndexedToHTML::OnInformationAvailable(nsIRequest *aRequest,
                                        nsISupports *aCtxt,
                                        const nsAString& aInfo) {
    nsAutoCString pushBuffer;
    char16_t* str = nsEscapeHTML2(PromiseFlatString(aInfo).get());
    if (!str)
        return NS_ERROR_OUT_OF_MEMORY;
    nsString escaped;
    escaped.Adopt(str);
    pushBuffer.AppendLiteral("<tr>\n <td>");
    // escaped is provided in Unicode, so write hex NCRs as necessary
    // to prevent the HTML parser from applying a character set.
    AppendNonAsciiToNCR(escaped, pushBuffer);
    pushBuffer.AppendLiteral("</td>\n <td></td>\n <td></td>\n <td></td>\n</tr>\n");

    return SendToListener(aRequest, aCtxt, pushBuffer);
}

void nsIndexedToHTML::FormatSizeString(int64_t inSize, nsCString& outSizeString)
{
    outSizeString.Truncate();
    if (inSize > int64_t(0)) {
        // round up to the nearest Kilobyte
        int64_t  upperSize = (inSize + int64_t(1023)) / int64_t(1024);
        outSizeString.AppendInt(upperSize);
        outSizeString.AppendLiteral(" KB");
    }
}

nsIndexedToHTML::nsIndexedToHTML() {
}

nsIndexedToHTML::~nsIndexedToHTML() {
}
