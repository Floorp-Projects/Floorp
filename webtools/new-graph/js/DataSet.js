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

function TimeDataSet(data) {
    this.data = data;
}

TimeDataSet.prototype = {
    data: null,

    indicesForTimeRange: function (startTime, endTime) {
        var startIndex = -1;
        var endIndex = -1;

        if (this.data[0] > endTime ||
            this.data[this.data.length-2] < startTime)
            return null;

        for (var i = 0; i < this.data.length/2; i++) {
            if (startIndex == -1 && this.data[i*2] >= startTime) {
                startIndex = i;
            } else if (startIndex != -1 && this.data[i*2] > endTime) {
                endIndex = i;
                return [startIndex, endIndex];
            }
        }

        endIndex = (this.data.length/2) - 1;
        return [startIndex, endIndex];
    },
};

function TimeValueDataSet(data, color) {
    this.data = data;

    this.firstTime = data[0];
    if (data.length > 2)
        this.lastTime = data[data.length-2];
    else
        this.lastTime = data[0];

    if (color) {
        this.color = color;
    } else {
        this.color = "#000000";
    }

    log ("new tds:", this.firstTime, this.lastTime);

    this.relativeToSets = new Array();
}

TimeValueDataSet.prototype = {
    __proto__: new TimeDataSet(),

    firstTime: 0,
    lastTime: 0,
    data: null,    // array: [time0, value0, time1, value1, ...]
    relativeTo: null,

    color: "black",

    minMaxValueForTimeRange: function (startTime, endTime) {
        var minValue = Number.POSITIVE_INFINITY;
        var maxValue = Number.NEGATIVE_INFINITY;
        for (var i = 0; i < this.data.length/2; i++) {
            var t = this.data[i*2];
            if (t >= startTime && t <= endTime) {
                var v = this.data[i*2+1];
                if (v < minValue)
                    minValue = v;
                if (v > maxValue)
                    maxValue = v;
            }
        }

        return [minValue, maxValue];
    },

    // create a new ds that's the average of this ds's values,
    // with the average sampled over the given interval,
    // at every avginterval/2
    createAverage: function (avginterval) {
        if (avginterval <= 0)
            throw "avginterval <= 0";

        if (this.averageDataSet != null &&
            this.averageInterval == avginterval)
        {
            return this.averageDataSet;
        }

        var newdata = [];

        var time0 = this.data[0];
        var val0 = 0;
        var count0 = 0;

        var time1 = time0 + avginterval/2;
        var val1 = 0;
        var count1 = 0;

        var ns = this.data.length/2;
        for (var i = 0; i < ns; i++) {
            var t = this.data[i*2];
            var v = this.data[i*2+1];
            if (t > time0+avginterval) {
                newdata.push(time0 + avginterval/2);
                newdata.push(count0 ? (val0 / count0) : 0);

                // catch up
                while (time1 < t) {
                    time0 += avginterval/2;
                    time1 = time0;
                }

                time0 = time1;
                val0 = val1;
                count0 = count1;

                time1 = time0 + avginterval/2;
                val1 = 0;
                count1 = 0;
            }

            val0 += v;
            count0++;

            if (t > time1) {
                val1 += v;
                count1++;
            }
        }

        if (count0 > 0) {
            newdata.push(time0 + avginterval/2);
            newdata.push(val0 / count0);
        }

        var newds = new TimeValueDataSet(newdata, lighterColor(this.color));
        newds.averageOf = this;

        this.averageDataSet = newds;
        this.averageInterval = avginterval;

        return newds;
    },

    // create a new dataset with this ds's data,
    // relative to otherds
    createRelativeTo: function (otherds, absval) {
        if (otherds == this) {
            log("error, same ds");
            return null;
        }

        for each (var s in this.relativeToSets) {
            if (s.relativeTo == otherds)
                return s;
        }

        var firstTime = this.firstTime;
        var lastTime = this.lastTime;

        if (otherds.firstTime > firstTime)
            firstTime = otherds.firstTime;
        if (otherds.lastTime < lastTime)
            lastTime = otherds.lastTime;

        var newdata = [];

        var thisidx = this.indicesForTimeRange (firstTime, lastTime);
        var otheridx = this.indicesForTimeRange (firstTime, lastTime);

        var o = otheridx[0];
        var ov, ov1, ov2, ot1, ot2;
        for (var i = thisidx[0]; i < thisidx[1]; i++) {
            var t = this.data[i*2];
            var tv = this.data[i*2+1];
            while (otherds.data[o*2] < t)
                o++;

            ot1 = otherds.data[o*2];
            ov1 = otherds.data[o*2+1];
            if (o < otheridx[1]) {
                ot2 = otherds.data[o*2+2];
                ov2 = otherds.data[o*2+3];
            } else {
                ot2 = ot1;
                ov2 = ov1;
            }


            var d = (t-ot1)/(ot2-ot1);
            ov = (1-d) * ov1 + d * ov2;

            newdata.push(t);
            //log ("i", i, "tv", tv, "ov", ov, "t", t, "ot1", ot1, "ot2", ot2, "ov1", ov1, "ov2", ov2);
            //log ("i", i, "tv", tv, "ov", ov, "tv/ov", tv/ov, "ov/tv", ov/tv);
            if (absval) {
                newdata.push(tv-ov);
            } else {
                if (tv > ov)
                    newdata.push((tv/ov) - 1);
                else
                    newdata.push(-((ov/tv) - 1));
            }
        }

        var newds = new TimeValueDataSet(newdata, this.color);
        newds.relativeTo = otherds;

        this.relativeToSets.push(newds);

        return newds;
    },
};

function TimeStringDataSet(data) {
    this.data = data;
}

TimeStringDataSet.prototype = {
    __proto__: new TimeDataSet(),
    data: null,

    onDataSetChanged: null,

    init: function () {
    },

    addString: function (time, string) {
    },

    removeStringAt: function (index) {
    },
};

