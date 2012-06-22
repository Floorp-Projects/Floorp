function plotBenchmark(fileNames, export)
%PLOTBENCHMARK Plots and exports video codec benchmarking results.
%   PLOTBENCHMARK(FILENAMES, EXPORT) parses the video codec benchmarking result
%   files given by the cell array of strings FILENAME. It plots the results and
%   optionally exports each plot to an appropriately named file.
%
%   EXPORT parameter:
%       'none'  No file exports.
%       'eps'   Exports to eps files (default).
%       'pdf'   Exports to eps files and uses the command-line utility
%               epstopdf to obtain pdf files.
%
%   Example:
%       plotBenchmark({'H264Benchmark.txt' 'LSVXBenchmark.txt'}, 'pdf')

if (nargin < 1)
    error('Too few input arguments');
elseif (nargin < 2)
    export = 'eps';
end

if ~iscell(fileNames)
    if ischar(fileNames)
        % one single file name as a string is ok
        if size(fileNames,1) > 1
            % this is a char matrix, not ok
            error('First argument must not be a char matrix');
        end
        % wrap in a cell array
        fileNames = {fileNames};
    else
        error('First argument must be a cell array of strings');
    end
end

if ~ischar(export)
    error('Second argument must be a string');
end

outpath = 'BenchmarkPlots';
[status, errMsg] = mkdir(outpath);
if status == 0
    error(errMsg);
end

nCases = 0;
testCases = [];
% Read each test result file
for fileIdx = 1:length(fileNames)
    if ~isstr(fileNames{fileIdx})
        error('First argument must be a cell array of strings');
    end

    fid = fopen(fileNames{fileIdx}, 'rt');
    if fid == -1
        error(['Unable to open ' fileNames{fileIdx}]);
    end

    version = '1.0';
    if ~strcmp(fgetl(fid), ['#!benchmark' version])
        fclose(fid);
        error(['Requires benchmark file format version ' version]);
    end

    % Parse results file into testCases struct
    codec = fgetl(fid);
    tline = fgetl(fid);
    while(tline ~= -1)
        nCases = nCases + 1;

        delim = strfind(tline, ',');
        name = tline(1:delim(1)-1);
        % Drop underscored suffix from name
        underscore = strfind(name, '_'); 
        if ~isempty(underscore)
            name = name(1:underscore(1)-1);
        end

        resolution = tline(delim(1)+1:delim(2)-1);
        frameRate = tline(delim(2)+1:end);

        tline = fgetl(fid);
        delim = strfind(tline, ',');
        bitrateLabel = tline(1:delim(1)-1); 
        bitrate = sscanf(tline(delim(1):end),',%f');

        tline = fgetl(fid);
        delim = strfind(tline, ',');
        psnrLabel = tline(1:delim(1)-1); 
        psnr = sscanf(tline(delim(1):end),',%f'); 


        % Default data for the optional lines
        speedLabel = 'Default';
        speed = 0;
        ssimLabel = 'Default';
        ssim = 0;
        
        tline = fgetl(fid);
        delim = strfind(tline, ',');
        
        while ~isempty(delim)
            % More data
            % Check type of data
            if strncmp(lower(tline), 'speed', 5)
                % Speed data included
                speedLabel = tline(1:delim(1)-1);
                speed = sscanf(tline(delim(1):end), ',%f');

                tline = fgetl(fid);
                
            elseif strncmp(lower(tline), 'encode time', 11)
                % Encode and decode times included
                % TODO: take care of the data
                
                % pop two lines from file
                tline = fgetl(fid);
                tline = fgetl(fid);
                
            elseif strncmp(tline, 'SSIM', 4)
                % SSIM data included
                ssimLabel = tline(1:delim(1)-1);
                ssim = sscanf(tline(delim(1):end), ',%f');

                tline = fgetl(fid);
            end
            delim = strfind(tline, ',');
        end

        testCases = [testCases struct('codec', codec, 'name', name, 'resolution', ...
            resolution, 'frameRate', frameRate, 'bitrate', bitrate, 'psnr', psnr, ...
            'speed', speed, 'bitrateLabel', bitrateLabel, 'psnrLabel', psnrLabel, ...
            'speedLabel', speedLabel, ...
            'ssim', ssim, 'ssimLabel', ssimLabel)];

        tline = fgetl(fid);
    end

    fclose(fid);
end

i = 0;
casesPsnr = testCases;
while ~isempty(casesPsnr)
    i = i + 1;
    casesPsnr = plotOnePsnr(casesPsnr, i, export, outpath);
end

casesSSIM = testCases;
while ~isempty(casesSSIM)
    i = i + 1;
    casesSSIM = plotOneSSIM(casesSSIM, i, export, outpath);
end

casesSpeed = testCases;
while ~isempty(casesSpeed)
    if casesSpeed(1).speed == 0
        casesSpeed = casesSpeed(2:end);
    else
        i = i + 1;
        casesSpeed = plotOneSpeed(casesSpeed, i, export, outpath);
    end
end



%%%%%%%%%%%%%%%%%%
%% SUBFUNCTIONS %%
%%%%%%%%%%%%%%%%%%

function casesOut = plotOnePsnr(cases, num, export, outpath)
% Find matching specs
plotIdx = 1;
for i = 2:length(cases)
    if strcmp(cases(1).resolution, cases(i).resolution) & ...
        strcmp(cases(1).frameRate, cases(i).frameRate)
        plotIdx = [plotIdx i];
    end
end

% Return unplotted cases
casesOut = cases(setdiff(1:length(cases), plotIdx));
cases = cases(plotIdx);

% Prune similar results
for i = 1:length(cases)
    simIndx = find(abs(cases(i).bitrate - [cases(i).bitrate(2:end) ; 0]) < 10);
    while ~isempty(simIndx)
        diffIndx = setdiff(1:length(cases(i).bitrate), simIndx);
        cases(i).psnr = cases(i).psnr(diffIndx);
        cases(i).bitrate = cases(i).bitrate(diffIndx);
        simIndx = find(abs(cases(i).bitrate - [cases(i).bitrate(2:end) ; 0]) < 10);
    end
end

% Prepare figure with axis labels and so on
hFig = figure(num);
clf;
hold on;
grid on;
axis([0 1100 20 50]);
set(gca, 'XTick', 0:200:1000);
set(gca, 'YTick', 20:10:60);
xlabel(cases(1).bitrateLabel);
ylabel(cases(1).psnrLabel);
res = cases(1).resolution;
frRate = cases(1).frameRate;
title([res ', ' frRate]);

hLines = [];
codecs = {};
sequences = {};
i = 0;
while ~isempty(cases)
    i = i + 1;
    [cases, hLine, codec, sequences] = plotOneCodec(cases, 'bitrate', 'psnr', i, sequences, 1);

    % Stored to generate the legend
    hLines = [hLines ; hLine];
    codecs = {codecs{:} codec};
end
legend(hLines, codecs, 4);
hold off;

if ~strcmp(export, 'none')
    % Export figure to an eps file
    res = stripws(res);
    frRate = stripws(frRate);
    exportName = [outpath '/psnr-' res '-' frRate];
    exportfig(hFig, exportName, 'Format', 'eps2', 'Color', 'cmyk');
end

if strcmp(export, 'pdf')
    % Use the epstopdf utility to convert to pdf
    system(['epstopdf ' exportName '.eps']);  
end


function casesOut = plotOneSSIM(cases, num, export, outpath)
% Find matching specs
plotIdx = 1;
for i = 2:length(cases)
    if strcmp(cases(1).resolution, cases(i).resolution) & ...
        strcmp(cases(1).frameRate, cases(i).frameRate)
        plotIdx = [plotIdx i];
    end
end

% Return unplotted cases
casesOut = cases(setdiff(1:length(cases), plotIdx));
cases = cases(plotIdx);

% Prune similar results
for i = 1:length(cases)
    simIndx = find(abs(cases(i).bitrate - [cases(i).bitrate(2:end) ; 0]) < 10);
    while ~isempty(simIndx)
        diffIndx = setdiff(1:length(cases(i).bitrate), simIndx);
        cases(i).ssim = cases(i).ssim(diffIndx);
        cases(i).bitrate = cases(i).bitrate(diffIndx);
        simIndx = find(abs(cases(i).bitrate - [cases(i).bitrate(2:end) ; 0]) < 10);
    end
end

% Prepare figure with axis labels and so on
hFig = figure(num);
clf;
hold on;
grid on;
axis([0 1100 0.5 1]); % y-limit are set to 'auto' below
set(gca, 'XTick', 0:200:1000);
%set(gca, 'YTick', 20:10:60);
xlabel(cases(1).bitrateLabel);
ylabel(cases(1).ssimLabel);
res = cases(1).resolution;
frRate = cases(1).frameRate;
title([res ', ' frRate]);

hLines = [];
codecs = {};
sequences = {};
i = 0;
while ~isempty(cases)
    i = i + 1;
    [cases, hLine, codec, sequences] = plotOneCodec(cases, 'bitrate', 'ssim', i, sequences, 1);

    % Stored to generate the legend
    hLines = [hLines ; hLine];
    codecs = {codecs{:} codec};
end
%set(gca,'YLimMode','auto')
set(gca,'YLim',[0.5 1])
set(gca,'YScale','log')
legend(hLines, codecs, 4);
hold off;

if ~strcmp(export, 'none')
    % Export figure to an eps file
    res = stripws(res);
    frRate = stripws(frRate);
    exportName = [outpath '/psnr-' res '-' frRate];
    exportfig(hFig, exportName, 'Format', 'eps2', 'Color', 'cmyk');
end

if strcmp(export, 'pdf')
    % Use the epstopdf utility to convert to pdf
    system(['epstopdf ' exportName '.eps']);  
end


function casesOut = plotOneSpeed(cases, num, export, outpath)
% Find matching specs
plotIdx = 1;
for i = 2:length(cases)
    if strcmp(cases(1).resolution, cases(i).resolution) & ...
        strcmp(cases(1).frameRate, cases(i).frameRate) & ...
        strcmp(cases(1).name, cases(i).name)
        plotIdx = [plotIdx i];
    end
end

% Return unplotted cases
casesOut = cases(setdiff(1:length(cases), plotIdx));
cases = cases(plotIdx);

% Prune similar results
for i = 1:length(cases)
    simIndx = find(abs(cases(i).psnr - [cases(i).psnr(2:end) ; 0]) < 0.25);
    while ~isempty(simIndx)
        diffIndx = setdiff(1:length(cases(i).psnr), simIndx);
        cases(i).psnr = cases(i).psnr(diffIndx);
        cases(i).speed = cases(i).speed(diffIndx);
        simIndx = find(abs(cases(i).psnr - [cases(i).psnr(2:end) ; 0]) < 0.25);
    end
end

hFig = figure(num);
clf;
hold on;
%grid on;
xlabel(cases(1).psnrLabel);
ylabel(cases(1).speedLabel);
res = cases(1).resolution;
name = cases(1).name;
frRate = cases(1).frameRate;
title([name ', ' res ', ' frRate]);

hLines = [];
codecs = {};
sequences = {};
i = 0;
while ~isempty(cases)
    i = i + 1;
    [cases, hLine, codec, sequences] = plotOneCodec(cases, 'psnr', 'speed', i, sequences, 0);

    % Stored to generate the legend
    hLines = [hLines ; hLine];
    codecs = {codecs{:} codec};
end
legend(hLines, codecs, 1);
hold off;

if ~strcmp(export, 'none')
    % Export figure to an eps file
    res = stripws(res);
    frRate = stripws(frRate);
    exportName = [outpath '/speed-' name '-' res '-' frRate];
    exportfig(hFig, exportName, 'Format', 'eps2', 'Color', 'cmyk');
end

if strcmp(export, 'pdf')
    % Use the epstopdf utility to convert to pdf
    system(['epstopdf ' exportName '.eps']);  
end


function [casesOut, hLine, codec, sequences] = plotOneCodec(cases, xfield, yfield, num, sequences, annotatePlot)
plotStr = {'gx-', 'bo-', 'r^-', 'kd-', 'cx-', 'go--', 'b^--'};
% Find matching codecs
plotIdx = 1;
for i = 2:length(cases)
    if strcmp(cases(1).codec, cases(i).codec)
        plotIdx = [plotIdx i];
    end
end

% Return unplotted cases
casesOut = cases(setdiff(1:length(cases), plotIdx));
cases = cases(plotIdx);

for i = 1:length(cases)
    % Plot a single case
    hLine = plot(getfield(cases(i), xfield), getfield(cases(i), yfield), plotStr{num}, ...
        'LineWidth', 1.1, 'MarkerSize', 6);
end

% hLine handle and codec are returned to construct the legend afterwards
codec = cases(1).codec;

if annotatePlot == 0
    return;
end

for i = 1:length(cases)
    % Print the codec name as a text label
    % Ensure each codec is only printed once
    sequencePlotted = 0;
    for j = 1:length(sequences)
        if strcmp(cases(i).name, sequences{j})
            sequencePlotted = 1;
            break;
        end
    end

    if sequencePlotted == 0
        text(getfield(cases(i), xfield, {1}), getfield(cases(i), yfield, {1}), ...
            ['    ' cases(i).name]);
        sequences = {sequences{:} cases(i).name};
    end
end


% Strip whitespace from string
function str = stripws(str)
if ~isstr(str)
    error('String required');
end
str = str(setdiff(1:length(str), find(isspace(str) == 1)));
