/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_MATLABPLOT_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_MATLABPLOT_H_

#include <list>
#include <string>
#include <vector>

#include "webrtc/typedefs.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
}

//#define PLOT_TESTING

#ifdef MATLAB

typedef struct engine Engine;
typedef struct mxArray_tag mxArray;

class MatlabLine
{
    friend class MatlabPlot;

public:
    MatlabLine(int maxLen = -1, const char *plotAttrib = NULL, const char *name = NULL);
    ~MatlabLine();
    virtual void Append(double x, double y);
    virtual void Append(double y);
    void SetMaxLen(int maxLen);
    void SetAttribute(char *plotAttrib);
    void SetName(char *name);
    void Reset();
    virtual void PurgeOldData() {};

    void UpdateTrendLine(MatlabLine * sourceData, double slope, double offset);

    double xMin();
    double xMax();
    double yMin();
    double yMax();

protected:
    void GetPlotData(mxArray** xData, mxArray** yData);
    std::string GetXName();
    std::string GetYName();
    std::string GetPlotString();
    std::string GetRefreshString();
    std::string GetLegendString();
    bool hasLegend();
    std::list<double> _xData;
    std::list<double> _yData;
    mxArray* _xArray;
    mxArray* _yArray;
    int _maxLen;
    std::string _plotAttribute;
    std::string _name;
};


class MatlabTimeLine : public MatlabLine
{
public:
    MatlabTimeLine(int horizonSeconds = -1, const char *plotAttrib = NULL, const char *name = NULL,
        int64_t refTimeMs = -1);
    ~MatlabTimeLine() {};
    void Append(double y);
    void PurgeOldData();
    int64_t GetRefTime();

private:
    int64_t _refTimeMs;
    int _timeHorizon;
};


class MatlabPlot
{
    friend class MatlabEngine;

public:
    MatlabPlot();
    ~MatlabPlot();

    int AddLine(int maxLen = -1, const char *plotAttrib = NULL, const char *name = NULL);
    int AddTimeLine(int maxLen = -1, const char *plotAttrib = NULL, const char *name = NULL,
        int64_t refTimeMs = -1);
    int GetLineIx(const char *name);
    void Append(int lineIndex, double x, double y);
    void Append(int lineIndex, double y);
    int Append(const char *name, double x, double y);
    int Append(const char *name, double y);
    int Length(char *name);
    void SetPlotAttribute(char *name, char *plotAttrib);
    void Plot();
    void Reset();
    void SmartAxis(bool status = true) { _smartAxis = status; };
    void SetFigHandle(int handle);
    void EnableLegend(bool enable) { _legendEnabled = enable; };

    bool TimeToPlot();
    void Plotting();
    void DonePlotting();
    void DisablePlot();

    int MakeTrend(const char *sourceName, const char *trendName, double slope, double offset, const char *plotAttrib = NULL);

#ifdef PLOT_TESTING
    int64_t _plotStartTime;
    int64_t _plotDelay;
#endif

private:
    void UpdateData(Engine* ep);
    bool GetPlotCmd(std::ostringstream & cmd, Engine* ep);
    void GetPlotCmd(std::ostringstream & cmd); // call inside crit sect
    void GetRefreshCmd(std::ostringstream & cmd); // call inside crit sect
    void GetLegendCmd(std::ostringstream & cmd);
    bool DataAvailable();

    std::vector<MatlabLine *> _line;
    int _figHandle;
    bool _smartAxis;
    double _xlim[2];
    double _ylim[2];
    webrtc::CriticalSectionWrapper *_critSect;
    bool _timeToPlot;
    bool _plotting;
    bool _enabled;
    bool _firstPlot;
    bool _legendEnabled;
    webrtc::EventWrapper* _donePlottingEvent;
};


class MatlabEngine
{
public:
    MatlabEngine();
    ~MatlabEngine();

    MatlabPlot * NewPlot(MatlabPlot *newPlot);
    void DeletePlot(MatlabPlot *plot);

private:
    static bool PlotThread(void *obj);

    std::vector<MatlabPlot *> _plots;
    webrtc::CriticalSectionWrapper *_critSect;
    webrtc::EventWrapper *_eventPtr;
    rtc::scoped_ptr<webrtc::ThreadWrapper> _plotThread;
    bool _running;
    int _numPlots;
};

#endif //MATLAB

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_MATLABPLOT_H_
