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
 * The Original Code is new-graph code.
 *
 * The Initial Developer of the Original Code is
 *    Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
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

function BonsaiService() {
}

BonsaiService.prototype = {
    // this could cache stuff, so that we only have to request the bookend data
    // if we want a wider range, but it's probably not worth it for now.
    //
    // The callback is called with an object argument which contains:
    // {
    //   times: [ t1, t2, t3, .. ],
    //   who:   [ w1, w2, w3, .. ],
    //   log:   [ l1, l2, l3, .. ],
    //   files: [ [ r11, f11, r12, f12, r13, f13, .. ], [ r21, f21, r22, f22, r23, f23, .. ], .. ]
    // }
    //
    // r = revision number, as a string, e.g. "1.15"
    // f = file, e.g. "mozilla/widget/foo.cpp"
    //
    // arg1 = callback, arg2 = null
    // arg1 = includeFiles, arg2 = callback
    requestCheckinsBetween: function (startDate, endDate, arg1, arg2) {
        var includeFiles = arg1;
        var callback = arg2;

        if (arg2 == null) {
            callback = arg1;
            includeFiles = null;
        }

        var queryargs = {
            treeid: "default",
            module: "SeaMonkeyAll",
            branch: "HEAD",
            mindate: startDate,
            maxdate: endDate
        };

        if (!includeFiles)
            queryargs.xml_nofiles = "1";

        log ("bonsai request: ", queryString(queryargs));

        doSimpleXMLHttpRequest (bonsaicgi, queryargs)
            .addCallbacks(
                function (obj) {
                    var result = { times: [], who: [], comment: [], files: null };
                    if (includeFiles)
                        result.files = [];

                    // strip out the xml declaration
                    var s = obj.responseText.replace(/<\?xml version="1.0"\?>/, "");
                    var bq = new XML(s);
                    for (var i = 0; i < bq.ci.length(); i++) {
                        var ci = bq.ci[i];
                        result.times.push(ci.@date);
                        result.who.push(ci.@who);
                        result.comment.push(ci.log.text().toString());
                        if (includeFiles) {
                            var files = [];
                            for (var j = 0; j < ci.files.f.length(); j++) {
                                var f = ci.files.f[j];
                                files.push(f.@rev);
                                files.push(f.text().toString());
                            }
                            result.files.push(files);
                        }
                    }

                    callback.call (window, result);
                },
                function () { alert ("Error talking to bonsai"); });
    },
};

