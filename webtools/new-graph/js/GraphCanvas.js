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

function Graph() {
}

Graph.prototype = {
    startTime: null,
    endTime: null,

    borderTop: 1,
    borderLeft: 1,

    yScale: 1,
    yOffset: 0,

    backBuffer: null,
    frontBuffer: null,
    yAxisDiv: null,
    xAxisDiv: null,

    dataSets: null,
    dataSetIndices: null,
    dataSetMinMaxes: null,

    dataSetMinMinVal: 0,
    dataSetMaxMaxVal: 0,

    xLabelContainer: null,
    xLabelWidth: 100,
    xLabelHeight: 50,

    yLabelContainer: null,
    yLabelWidth: 50,
    yLabelHeight: 50,

    selectionType: "none",
    selectionColor: "rgba(0,0,255,0.5)",
    selectionCursorTime: null,
    selectionStartTime: null,
    selectionEndTime: null,

    onSelectionChanged: null,
    onCursorMoved: null,

    cursorType: "none",
    cursorColor: "rgba(200,200,0,0.7)",
    cursorTime: null,
    cursorValue: null,

    markerColor: "rgba(200,0,0,0.4)",
    markersVisible: true,
    markers: null,

    dirty: true,
    valid: false,

    init: function (canvasElement) {
        this.frontBuffer = getElement(canvasElement);
        this.xLabelContainer = getElement(canvasElement + "-labels-x");
        this.yLabelContainer = getElement(canvasElement + "-labels-y");

        this.backBuffer = new CANVAS();
        this.backBuffer.width = this.frontBuffer.width;
        this.backBuffer.height = this.frontBuffer.height;

        this.overlayBuffer = new CANVAS();
        this.overlayBuffer.width = this.frontBuffer.width;
        this.overlayBuffer.height = this.frontBuffer.height;

        this.dataSets = new Array();
        this.dataSetMinMaxes = new Array();
        this.dataSetIndices = new Array();

        this.markers = new Array();

        this.onSelectionChanged = new YAHOO.util.CustomEvent("graphselectionchanged");
        this.onCursorMoved = new YAHOO.util.CustomEvent("graphcursormoved");
    },

    getQueryString: function (prefix) {
        var qs = "";

        qs += prefix + "st=" + this.selectionType;
        if (this.selectionType == "range") {
            if (this.selectionStartTime != null && this.selectionEndTime != null)
                qs += "&" + prefix + "ss=" + this.selectionStartTime + "&" + prefix + "se=" + this.selectionEndTime;
        } else if (this.selectionType == "cursor") {
            if (this.selectionCursorTime != null)
                qs += "&" + prefix + "sc=" + this.selectionCursorTime;
        }

        qs += "&" + prefix + "start=" + this.startTime + "&" + prefix + "end=" + this.endTime;

        return qs;
    },

    handleQueryStringData: function (prefix, qsdata) {
        // XX should do some more verification that
        // qsdata has the members we care about
        this.startTime = new Number(qsdata[prefix + "start"]);
        this.endTime = new Number(qsdata[prefix + "end"]);

        var st = qsdata[prefix + "st"];
        if (st == "range") {
            this.selectionType = "range";
            if (((prefix+"ss") in qsdata) && ((prefix+"se") in qsdata)) {
                this.selectionStartTime = new Number(qsdata[prefix + "ss"]);
                this.selectionEndTime = new Number(qsdata[prefix + "se"]);
            } else {
                this.selectionStartTime = null;
                this.selectionEndTime = null;
            }                
        } else if (st == "cursor") {
            this.selectionType = "Cursor";
            if ((prefix+"sc") in qsdata)
                this.selectionCursorTime = new Number(qsdata[prefix + "sc"]);
            else
                this.selectionCursorTime = null;
        }

        this.dirty = true;
    },

    addDataSet: function (ds, color) {
        if (this.dataSets.some(function(d) { return (d==ds); }))
            return;

        if (color == null) {
            if (ds.color != null) {
                color = ds.color;
            } else {
                color = randomColor();
            }
        }

        this.dataSets.push(ds);
        this.dataSetIndices.push(null);
        this.dataSetMinMaxes.push(null);

        this.dirty = true;
    },

    removeDataSet: function (ds) {
        for (var i = 0; i < this.dataSets.length; i++) {
            if (this.dataSets[i] == ds) {
                this.dataSets = Array.splice(this.dataSets, i, 1);
                this.dataSetIndices = Array.splice(this.dataSetIndices, i, 1);
                this.dataSetMinMaxes = Array.splice(this.dataSetMinMaxes, i, 1);
                return;
            }
        }
    },

    clearDataSets: function () {
        this.dataSets = new Array();
        this.dataSetMinMaxes = new Array();
        this.dataSetIndices = new Array();

        this.dirty = true;
    },

    setTimeRange: function (start, end) {
        this.startTime = start;
        this.endTime = end;

        this.dirty = true;
    },

    expandTimeRange: function (start, end) {
        if (this.startTime == null || start < this.startTime)
            this.startTime = start;
        if (this.endTime == null || end > this.endTime)
            this.endTime = end;

        this.dirty = true;
    },

    setSelectionType: function (stype) {
        if (this.selectionType == stype)
            return;

        // clear out old listeners
        if (this.selectionType == "range") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousedown", this.selectionMouseDown);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousemove", this.selectionMouseMove);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseup", this.selectionMouseUp);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseout", this.selectionMouseOut);
        } else if (this.selectionType == "cursor") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousedown", this.selectionMouseDown);
        }

        this.selectionStartTime = null;
        this.selectionEndTime = null;
        this.selectionCursorTime = null;

        if (stype == "range") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousedown", this.selectionMouseDown, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mousemove", this.selectionMouseMove, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseup", this.selectionMouseUp, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseout", this.selectionMouseOut, this, true);

            this.selectionType = "range";
        } else if (stype == "cursor") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousedown", this.selectionMouseDown, this, true);
            this.selectionType = "cursor";
        }

        this.redrawOverlayOnly();
    },

    setSelectionColor: function (scolor) {
        this.selectionColor = scolor;
        this.redrawOverlayOnly();
    },

    setCursorType: function (type) {
        if (this.cursorType == type)
            return;

        if (this.cursorType == "free") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousemove", this.cursorMouseMove);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseout", this.cursorMouseOut);
        }

        if (type == "free") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousemove", this.cursorMouseMove, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseout", this.cursorMouseOut, this, true);
            this.cursorType = "free";
        } else {
            this.cursorType = "none";
        }
    },

    recompute: function () {
        this.dataSetIndices = [];
        this.dataSetMinMaxes = [];

        this.hasRelative = false;
        var nonRelative = 0;

        for (var i = 0; i < this.dataSets.length; i++) {
            this.dataSetIndices.push (this.dataSets[i].indicesForTimeRange (this.startTime, this.endTime));
            this.dataSetMinMaxes.push (this.dataSets[i].minMaxValueForTimeRange (this.startTime, this.endTime));

            if (this.dataSets[i].relativeTo != null)
                this.hasRelative = true;
            else
                nonRelative++;
        }

        if (this.hasRelative && nonRelative > 1) {
            log("More than one non-relative dataset added to graph");
            throw "More than one non-relative dataset added to graph";
        }

        this.dataSetMinMinVal = Number.MAX_VALUE;
        this.dataSetMaxMaxVal = Number.MIN_VALUE;


        for each (var dsvals in this.dataSetMinMaxes) {
            if (dsvals[0] != Infinity && dsvals[1] != -Infinity) {
                if (this.dataSetMinMinVal > dsvals[0])
                    this.dataSetMinMinVal = dsvals[0];
                if (this.dataSetMaxMaxVal < dsvals[1])
                    this.dataSetMaxMaxVal = dsvals[1];
            }
        }

        if (this.dataSetMinMinVal == Number.MAX_VALUE &&
            this.dataSetMaxMaxVal == Number.MIN_VALUE)
        {
            this.dataSetMinMinVal = 0;
            this.dataSetMaxMaxVal = 100;
        }

        //log ("minmin:", this.dataSetMinMinVal, "maxmax:", this.dataSetMaxMaxVal);

        this.getTimeAxisLabels();
        this.getValueAxisLabels();

        this.dirty = false;
    },

    autoScale: function () {
        if (this.dirty)
            this.recompute();

        var vmin, vmax;

        if (this.hasRelative) {
            vmin = Math.floor(this.dataSetMinMinVal);
            vmax = Math.ceil(this.dataSetMaxMaxVal);

            if ((vmax - vmin) == 1)
                vmin--;

            //log ("vmin", vmin, "vmax", vmax);
            this.yOffset = vmin;
            this.yScale = this.frontBuffer.height / (vmax - vmin);
            this.dirty = true;
            return;
        }

        var delta = this.dataSetMaxMaxVal - this.dataSetMinMinVal;
        if (delta == 0) {
            this.yOffset = this.dataSetMinMinVal - (this.frontBuffer.height)/2;
            this.yScale = 1;
            scaled = true;
        } else {
            var scaled = false;
            for each (var sfactor in [1000, 500, 250, 100, 25, 10, 1]) {
                if (delta > sfactor) {
                    vmin = this.dataSetMinMinVal - (this.dataSetMinMinVal % sfactor);
                    vmax = (this.dataSetMaxMaxVal - (this.dataSetMaxMaxVal % sfactor)) + sfactor;
                    
                    this.yOffset = vmin;
                    this.yScale = this.frontBuffer.height / (vmax - vmin);
                    scaled = true;
                    break;
                }
            }
        }

        if (!scaled) {
            this.yOffset = this.dataSetMinMinVal;
            this.yScale = this.frontBuffer.height / (this.dataSetMaxMaxVal - this.dataSetMinMinVal);
        }

        //log ("autoScale: yscale:", this.yScale, "yoff:", this.yOffset);
        // we have to dirty again, due to the labels
        this.dirty = true;
    },

    redraw: function () {
        if (this.dirty)
            this.recompute();

        var ctx = this.backBuffer.getContext("2d");
        var cw = this.backBuffer.width;
        var ch = this.backBuffer.height;

        var xoffs = this.startTime;
        var yoffs = this.yOffset;

        var xscale = cw / (this.endTime - this.startTime);

        var hasAverageDSs = false;
        for each (var ds in this.dataSets) {
            if ("averageOf" in ds) {
                hasAverageDSs = true;
                break;
            }
        }

        for (var i = 0; i < this.dataSets.length; i++) {
            // yScale = pixels-per-value
            with (ctx) {
                clearRect (0, 0, cw, ch);
                lineWidth = 1.0;

                // draw gridlines
                var timeLabelValues = this.getTimeAxisLabels();
                strokeStyle = "#999999";
                for each (var label in timeLabelValues) {
                    // label[1] is the actual value of that label line; we need
                    // to scale it into place, but we can't just use scale()
                    // since we want to make sure it's a single-pixel line
                    var p = Math.round((label[1] - xoffs) * xscale) + 0.5;
                    beginPath();
                    moveTo(p, -0.5);
                    lineTo(p, this.frontBuffer.height + 0.5);
                    stroke();
                }

                var valueLabelValues = this.getValueAxisLabels();
                for each (var label in valueLabelValues) {
                    var p = Math.round((label[1] - yoffs) * this.yScale) + 0.5;
                    beginPath();
                    moveTo(-0.5, p);
                    lineTo(this.frontBuffer.width + 0.5, p);
                    stroke();
                }

                // draw markers
                strokeStyle = this.markerColor;
                for (var i = 0; i < this.markers.length/2; i++) {
                    var mtime = this.markers[i*2];
                    //var mlabel = this.markers[i*2+1];

                    if (mtime < this.startTime || mtime > this.endTime)
                        continue;

                    var p = Math.round((mtime - xoffs) * xscale) + 0.5;
                    beginPath();
                    moveTo(p, Math.round(this.frontBuffer.height*0.8)-0.5);
                    lineTo(p, this.frontBuffer.height+0.5);
                    stroke();
                }

                // draw actual graph lines

                for (var i = 0; i < this.dataSets.length; i++) {
                    if (this.dataSetIndices[i] == null) {
                        // there isn't anything in the data set in the given time range
                        continue;
                    }

                    //log ("ds start end", this.startTime, this.endTime, "timediff:", (this.endTime - this.startTime));
                    save();
                    scale(xscale, -this.yScale);
                    translate(0, -ch/this.yScale);

                    beginPath();

                    var first = true;
                    var startIdx = this.dataSetIndices[i][0];
                    var endIdx = this.dataSetIndices[i][1];

                    // start one before and go one after if we can,
                    // so that the plot doesn't have a hole at the start
                    // and end
                    if (startIdx > 0) startIdx--;
                    if (endIdx < ((this.dataSets[i].data.length)/2)) endIdx++;

                    for (var j = startIdx; j < endIdx; j++)
                    {
                        var t = this.dataSets[i].data[j*2];
                        var v = this.dataSets[i].data[j*2+1];
                        if (first) {
                            moveTo (t-xoffs, v-yoffs);
                            first = false;
                        } else {
                            lineTo (t-xoffs, v-yoffs);
                        }
                    }

                    /* bleh. */

                    restore();

                    if (hasAverageDSs && !("averageOf" in this.dataSets[i])) {
                        lineWidth = 0.5;
                    } else {
                        lineWidth = 1.0;
                    }

                    strokeStyle = colorToRgbString(this.dataSets[i].color);
                    stroke();
                }
            }
        }

        this.redrawOverlayOnly();

        try {
            this.makeLabels();
        } catch (e) {
            log(e);
        }

        this.valid = true;
    },

    redrawOverlayOnly: function () {
        with (this.frontBuffer.getContext("2d")) {
            globalCompositeOperation = "copy";
            drawImage(this.backBuffer, 0, 0);
        }

        var doDrawOverlay = false;

        with (this.overlayBuffer.getContext("2d")) {
            clearRect(0, 0, this.overlayBuffer.width, this.overlayBuffer.height);
            if (this.selectionCursorTime || (this.selectionStartTime && this.selectionEndTime)) {
                var spixel, epixel;
                var pps = (this.frontBuffer.width / (this.endTime - this.startTime));

                if (this.selectionCursorTime) {
                    spixel = Math.round((this.selectionCursorTime-this.startTime) * pps);
                    epixel = spixel + 1;
                } else if (this.selectionStartTime && this.selectionEndTime) {
                    spixel = Math.round((this.selectionStartTime-this.startTime) * pps);
                    epixel = Math.round((this.selectionEndTime-this.startTime) * pps);
                }

                globalCompositeOperation = "over";
                fillStyle = this.selectionColor;
                fillRect(spixel, 0, epixel - spixel, this.frontBuffer.height);

                doDrawOverlay = true;
            }

            if ((this.cursorType != "none") && this.cursorTime != null && this.cursorValue != null) {
                globalCompositeOperation = "over";
                strokeStyle = this.cursorColor;

                var cw = this.frontBuffer.width;
                var ch = this.frontBuffer.height;

                var v;

                v = ch - Math.round((this.cursorValue - this.yOffset) * this.yScale);
                beginPath();
                moveTo(  -0.5, v+0.5);
                lineTo(cw+0.5, v+0.5);
                stroke();

                v = Math.round((this.cursorTime-this.startTime) * cw/(this.endTime - this.startTime));
                beginPath();
                moveTo(v+0.5,   -0.5);
                lineTo(v+0.5, ch+0.5);
                stroke();

                doDrawOverlay = true;
            }
        }

        if (doDrawOverlay) {
            with (this.frontBuffer.getContext("2d")) {
                globalCompositeOperation = "over";
                drawImage(this.overlayBuffer, 0, 0);
            }
        }

    },


    getValueAxisLabels: function () {
        if (!this.dirty)
            return this.yAxisLabels;

        // see getTimeAxisLabels for more commentary

        // y axis is either an arbitrary value or a percent
        var visibleValues = this.frontBuffer.height * this.yScale;
        var valuePerPixel = 1/this.yScale;
        var labelValue = this.yLabelHeight * valuePerPixel;

        // round to nearest integer, but that's it; we can try to get
        // fancy later on
        var fixedPrecision;
        if (this.hasRelative) {
/*
            labelValue = 1;

            var vdiff = Math.ceil(this.dataSetMaxMaxVal) - Math.floor(this.dataSetMinMinVal);
            if (vdiff <= 2) {
                labelValue = .25;
            } else if (vdiff <= 3) {
                labelValue = .5;
            } else {
                labelValue = 1;
            }
*/
        } else {
            if (visibleValues > 1000) {
                fixedPrecision = 0;
            } else if (visibleValues > 100) {
                fixedPrecision = 1;
            } else if (visibleValues > 10) {
                fixedPrecision = 2;
            } else if (visibleValues > 1) {
                fixedPrecision = 3;
            }
        }

        var numLabels = Math.floor(this.frontBuffer.height / this.yLabelHeight) + 1;
        var labels = [];
        var firstLabelOffsetValue = (this.yOffset % labelValue);

        var visibleYMax = this.yOffset + this.frontBuffer.height/this.yScale;

        //log("yoffset", this.yOffset, "ymax", visibleYMax, "labelValue", labelValue, "numLabels", numLabels, "flo", firstLabelOffsetValue, "visibleYMax", visibleYMax);
        for (var i = 0; i < numLabels; i++) {
            // figure out the time value of this label
            var lvalue = this.yOffset + firstLabelOffsetValue + i*labelValue;
            if (lvalue > visibleYMax)
                break;

            // we want the text to correspond to the value drawn at the start of the block
            // also note that Y axis is inverted
            // XXX put back the -y/2 once we figure out how to vertically center a label's text
            var lpos = ((lvalue - this.yOffset)/valuePerPixel /* - (this.yLabelHeight/2)*/);
            var l;
            //log ("i", i, "lpos: ", lpos, "lvalue", lvalue, "ysc", this.yScale);
            if (this.hasRelative) {
                l = [lpos, lvalue, (lvalue * 100).toFixed(0).toString() + "%"];
            } else {
                l = [lpos, lvalue, lvalue.toFixed(fixedPrecision).toString()];
            }
            //log("lval", lvalue, "lpos", l[0]);
            labels.push(l);
        }

        this.yAxisLabels = labels;
        return labels;
    },

    getTimeAxisLabels: function () {
        if (!this.dirty)
            return this.xAxisLabels;

        // x axis is always time in seconds

        // duration is in seconds
        var duration = this.endTime - this.startTime;

        // we know the pixel size and we know the time, we can
        // compute the seconds per pixel
        var secondsPerPixel = duration / this.frontBuffer.width;

        // so what's the exact duration of one label of our desired size?
        var labelDuration = this.xLabelWidth * secondsPerPixel;

        // let's come up with a more round duration for our label.
        if (labelDuration <= 60) {
            labelDuration = 60;
        } else if (labelDuration <= 14*60) {
            labelDuration = Math.ceil(labelDuration / 60) * 60;
        } else if (labelDuration <= 15*60) {
            labelDuration = 15*60;
        } else if (labelDuration <= 59*60) {
            labelDuration = Math.ceil(labelDuration / (5*60)) * (5*60);
        } else if (labelDuration <= 23*ONE_HOUR_SECONDS) {
            labelDuration = Math.ceil(labelDuration / ONE_HOUR_SECONDS) * ONE_HOUR_SECONDS;
        } else if (labelDuration <= 6*ONE_DAY_SECONDS) {
            labelDuration = Math.ceil(labelDuration / ONE_DAY_SECONDS) * ONE_DAY_SECONDS;
        } else {
            // round to the nearest day at least
            labelDuration = labelDuration - (labelDuration%ONE_DAY_SECONDS);
        }

        // how many labels max can we fit?
        var numLabels = (this.frontBuffer.width / this.xLabelWidth);

        var labels = [];

        // we want our first label to land on a multiple of the label duration;
        // figure out where that lies.
        var firstLabelOffsetSeconds = (labelDuration - (this.startTime % labelDuration));

        //log ("sps", secondsPerPixel, "ldur", labelDuration, "nl", numLabels, "flo", firstLabelOffsetSeconds);

        for (var i = 0; i < numLabels; i++) {
            // figure out the time value of this label
            var ltime = this.startTime + firstLabelOffsetSeconds + i*labelDuration;
            if (ltime > this.endTime)
                break;

            // the first number is at what px position to place the label;
            // the second number is the actual value of the label
            // the third is an array of strings that go into the label
            var lval = [(ltime - this.startTime)/secondsPerPixel - (this.xLabelWidth/2), ltime, this.formatTimeLabel(ltime)];
            //log ("ltime", ltime, "lpos", lval[0], "end", this.endTime);
            labels.push(lval);
        }

        this.xAxisLabels = labels;
        return labels;
    },

    formatTimeLabel: function (ltime) {
        // this should be overridden; we just return ltime here
        return [ltime, ""];
    },

    makeLabels: function () {
        //log ("makeLabels");
        if (this.xLabelContainer) {
            var labels = [];
            var xboxPos = YAHOO.util.Dom.getXY(this.xLabelContainer);
            xboxPos[0] = xboxPos[0] + this.borderLeft;
            xboxPos[1] = xboxPos[1] + this.borderTop;
            var labelValues = this.getTimeAxisLabels();

            for each (var lval in labelValues) {
                var xpos = /*xboxPos[0] +*/ lval[0];
                var div = new DIV({ class: "x-axis-label" });
                div.style.position = "absolute";
                div.style.width = this.xLabelWidth + "px";
                div.style.height = this.xLabelHeight + "px";
                div.style.left = xpos + "px";
                div.style.top = "0px"; //xboxPos[1] + this.frontBuffer.height;

                // XXX don't hardcode [2][0] etc.
                appendChildNodes(div, lval[2][0], new BR(), lval[2][1]);

                labels.push(div);
            }

            replaceChildNodes(this.xLabelContainer, labels);
        }

        if (this.yLabelContainer) {
            var labels = [];
            var yboxPos = YAHOO.util.Dom.getXY(this.yLabelContainer);
            yboxPos[0] = yboxPos[0] + this.borderLeft;
            yboxPos[1] = yboxPos[1] + this.borderTop;
            var labelValues = this.getValueAxisLabels();
            var firstLabelShift = labelValues[labelValues.length-1][0]

            for each (var lval in labelValues) {
                var ypos = this.frontBuffer.height - Math.round((lval[1] - this.yOffset) * this.yScale);

                //var ypos = /*xboxPos[0] +*/ lval[0];
                var div = new DIV({ class: "y-axis-label" });
                div.style.position = "absolute";
                div.style.width = this.yLabelWidth + "px";
                div.style.height = this.yLabelHeight + "px";
                div.style.left = "0px"; //xboxPos[0]
                // XXX remove the -8 once we figure out how to vertically center text in this box
                div.style.top = (ypos-8) + "px";

                //log ("ypos: ", ypos, " lval: ", lval);
                // XXX don't hardcode [2] etc.
                appendChildNodes(div, lval[2]);
                labels.push(div);
            }

            replaceChildNodes(this.yLabelContainer, labels);
        }

        if (0) {
            var labels = [];
            var total_sz = this.frontBuffer.height;

            // the ideal label height is 30px; 10% extra for gaps
            var sz_desired = 30;
            var nlabels = Math.floor(total_sz / (sz_desired * 1.10));
            var label_sz = Math.floor(total_sz / nlabels);

            //log ("lsz: " + label_sz + " nl: " + nlabels);

            for (var i = 0; i < nlabels; i++) {
                var pos = label_sz * i;
                var div = new DIV({class: "y-axis-label", style: "width: 50px; height: " + label_sz + "px" });
                appendChildNodes(div, "Label " + i);

                labels.push(div);
            }

            replaceChildNodes(this.yLabelContainer, labels);
        }
    },

    //
    // selection handling
    //
    selectionMouseDown: function(event) {
        if (!this.valid)
            return;

        if (this.selectionType == "range") {
            var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
            this.dragState = { startX: event.pageX - pos };
            var ds = this.dragState;

            ds.curX = ds.startX + 1;
            ds.secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;

            this.selectionStartTime = ds.startX * ds.secondsPerPixel + this.startTime;
            this.selectionEndTime = ds.curX * ds.secondsPerPixel + this.startTime;

            this.redrawOverlayOnly();
            
            this.selectionSweeping = true;
        } else if (this.selectionType == "cursor") {
            var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
            var secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;

            this.selectionCursorTime = (event.pageX - pos) * secondsPerPixel + this.startTime;

            this.redrawOverlayOnly();

            this.onSelectionChanged.fire("cursor", this.selectionCursorTime);
        }
    },

    abortSelection: function() {
        if (!this.selectionSweeping)
            return;

        this.selectionSweeping = false;
        this.redrawOverlayOnly();
    },

    clearSelection: function() {
        this.selectionSweeping = false;
        this.selectionStartTime = null;
        this.selectionEndTime = null;

        this.redrawOverlayOnly();
    },

    selectionUpdateFromEventPageCoordinate: function(pagex) {
        var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
        var ds = this.dragState;
        ds.curX = pagex - pos;
        if (ds.curX > this.frontBuffer.width)
            ds.curX = this.frontBuffer.width;
        else if (ds.curX < 0)
            ds.curX = 0;

        var cxTime = (ds.curX * ds.secondsPerPixel) + this.startTime;
        var startxTime = (ds.startX * ds.secondsPerPixel) + this.startTime;
        if (ds.curX < ds.startX) {
            this.selectionEndTime = startxTime;
            this.selectionStartTime = cxTime;
        } else {
            this.selectionStartTime = startxTime;
            this.selectionEndTime = cxTime;
        }
    },

    selectionMouseMove: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionUpdateFromEventPageCoordinate(event.pageX);

        this.redrawOverlayOnly();
    },

    selectionMouseUp: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionSweeping = false;

        var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
        if (this.dragState.startX == event.pageX - pos) {
            // mouse didn't move
            this.selectionStartTime = null;
            this.selectionEndTime = null;

            this.redrawOverlayOnly();
        }

        this.onSelectionChanged.fire("range", this.selectionStartTime, this.selectionEndTime);
    },

    selectionMouseOut: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionUpdateFromEventPageCoordinate(event.pageX);

        this.selectionSweeping = false;
        this.onSelectionChanged.fire("range", this.selectionStartTime, this.selectionEndTime);
    },

    /*
     * cursor stuff
     */
    cursorMouseMove: function (event) {
        if (!this.valid)
            return;

        if (this.cursorType != "free")
            return;

        var pos = YAHOO.util.Dom.getXY(this.frontBuffer);
        pos[0] = pos[0] + this.borderLeft;
        pos[1] = pos[1] + this.borderTop;
        var secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;
        var valuesPerPixel = 1.0 / this.yScale;

        this.cursorTime = (event.pageX - pos[0]) * secondsPerPixel + this.startTime;
        this.cursorValue = (this.frontBuffer.height - (event.pageY - pos[1])) * valuesPerPixel + this.yOffset;

        this.onCursorMoved.fire(this.cursorTime, this.cursorValue);

        this.redrawOverlayOnly();
    },

    cursorMouseOut: function (event) {
        if (!this.valid)
            return;

        if (this.cursorType != "free")
            return;

        this.cursorTime = null;
        this.cursorValue = null;

        this.onCursorMoved.fire(this.cursorTime, this.cursorValue);

        this.redrawOverlayOnly();
    },

    /*
     * marker stuff
     */
    deleteAllMarkers: function () {
        this.markers = new Array();
    },

    addMarker: function (mtime, mlabel) {
        this.markers.push (mtime);
        this.markers.push (mlabel);
    },

};

function CalendarTimeGraph(canvasId) {
    this.__proto__.__proto__.init.call (this, canvasId);
}

function formatTime(ltime, twoLines) {
    // ltime is in seconds since the epoch in, um, so
    var d = new Date (ltime*1000);
    var s1 = d.getHours() +
        (d.getMinutes() < 10 ? ":0" : ":") + d.getMinutes() +
        (d.getSeconds() < 10 ? ":0" : ":") + d.getSeconds();
    if (twoLines) {
        var s2 = d.getDate() + " " + MONTH_ABBREV[d.getMonth()] + " " + (d.getYear()+1900);
        return [s1, s2];
    } else {
        var yr = d.getYear();
        if (yr > 100) yr -= 100;
        if (yr < 10) yr = "0" + yr;
        var s2 = d.getMonth() + "/" + d.getDate() + "/" + yr;
        return s2 + " " + s1;
    }
}

CalendarTimeGraph.prototype = {
    __proto__: new Graph(),

    formatTimeLabel: function (ltime) {
        return formatTime(ltime, true);
    },

};

