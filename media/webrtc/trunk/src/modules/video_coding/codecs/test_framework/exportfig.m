function exportfig(varargin)
%EXPORTFIG  Export a figure to Encapsulated Postscript.
%   EXPORTFIG(H, FILENAME) writes the figure H to FILENAME.  H is
%   a figure handle and FILENAME is a string that specifies the
%   name of the output file.
%
%   EXPORTFIG(...,PARAM1,VAL1,PARAM2,VAL2,...) specifies
%   parameters that control various characteristics of the output
%   file.
%
%   Format Paramter:
%     'Format'  one of the strings 'eps','eps2','jpeg','png','preview'
%          specifies the output format. Defaults to 'eps'.
%          The output format 'preview' does not generate an output
%          file but instead creates a new figure window with a
%          preview of the exported figure. In this case the
%          FILENAME parameter is ignored.
%
%     'Preview' one of the strings 'none', 'tiff'
%          specifies a preview for EPS files. Defaults to 'none'.
%
%   Size Parameters:
%     'Width'   a positive scalar
%          specifies the width in the figure's PaperUnits
%     'Height'  a positive scalar
%          specifies the height in the figure's PaperUnits
%
%     Specifying only one dimension sets the other dimension
%     so that the exported aspect ratio is the same as the
%     figure's current aspect ratio. 
%     If neither dimension is specified the size defaults to 
%     the width and height from the figure's PaperPosition. 
%           
%   Rendering Parameters:
%     'Color'     one of the strings 'bw', 'gray', 'cmyk'
%         'bw'    specifies that lines and text are exported in
%                 black and all other objects in grayscale
%         'gray'  specifies that all objects are exported in grayscale
%         'cmyk'  specifies that all objects are exported in color
%                 using the CMYK color space
%     'Renderer'  one of the strings 'painters', 'zbuffer', 'opengl'
%         specifies the renderer to use
%     'Resolution'   a positive scalar
%         specifies the resolution in dots-per-inch.
%     
%     The default color setting is 'bw'.
%
%   Font Parameters:
%     'FontMode'     one of the strings 'scaled', 'fixed'
%     'FontSize'     a positive scalar
%          in 'scaled' mode multiplies with the font size of each
%          text object to obtain the exported font size
%          in 'fixed' mode specifies the font size of all text
%          objects in points
%     'FontEncoding' one of the strings 'latin1', 'adobe'
%          specifies the character encoding of the font
%
%     If FontMode is 'scaled' but FontSize is not specified then a
%     scaling factor is computed from the ratio of the size of the
%     exported figure to the size of the actual figure. The minimum
%     font size allowed after scaling is 5 points.
%     If FontMode is 'fixed' but FontSize is not specified then the
%     exported font sizes of all text objects is 7 points.
%
%     The default 'FontMode' setting is 'scaled'.
%
%   Line Width Parameters:
%     'LineMode'     one of the strings 'scaled', 'fixed'
%     'LineWidth'    a positive scalar
%          the semantics of LineMode and LineWidth are exactly the
%          same as FontMode and FontSize, except that they apply
%          to line widths instead of font sizes. The minumum line
%          width allowed after scaling is 0.5 points.
%          If LineMode is 'fixed' but LineWidth is not specified 
%          then the exported line width of all line objects is 1
%          point. 
%
%   Examples:
%     exportfig(gcf,'fig1.eps','height',3);
%       Exports the current figure to the file named 'fig1.eps' with
%       a height of 3 inches (assuming the figure's PaperUnits is 
%       inches) and an aspect ratio the same as the figure's aspect
%       ratio on screen.
%
%     exportfig(gcf, 'fig2.eps', 'FontMode', 'fixed',...
%                'FontSize', 10, 'color', 'cmyk' );
%       Exports the current figure to 'fig2.eps' in color with all
%       text in 10 point fonts. The size of the exported figure is
%       the figure's PaperPostion width and height.


if (nargin < 2)
  error('Too few input arguments');
end

% exportfig(H, filename, ...)
H = varargin{1};
if ~ishandle(H) | ~strcmp(get(H,'type'), 'figure')
  error('First argument must be a handle to a figure.');
end
filename = varargin{2};
if ~ischar(filename)
  error('Second argument must be a string.');
end
paramPairs = varargin(3:end);

% Do some validity checking on param-value pairs
if (rem(length(paramPairs),2) ~= 0)
  error(['Invalid input syntax. Optional parameters and values' ...
	 ' must be in pairs.']);
end

format = 'eps';
preview = 'none';
width = -1;
height = -1;
color = 'bw';
fontsize = -1;
fontmode='scaled';
linewidth = -1;
linemode=[];
fontencoding = 'latin1';
renderer = [];
resolution = [];

% Process param-value pairs
args = {};
for k = 1:2:length(paramPairs)
  param = lower(paramPairs{k});
  if (~ischar(param))
    error('Optional parameter names must be strings');
  end
  value = paramPairs{k+1};
  
  switch (param)
   case 'format'
    format = value;
    if (~strcmp(format,{'eps','eps2','jpeg','png','preview'}))
      error(['Format must be ''eps'', ''eps2'', ''jpeg'', ''png'' or' ...
	     ' ''preview''.']);
    end
   case 'preview'
    preview = value;
    if (~strcmp(preview,{'none','tiff'}))
      error('Preview must be ''none'' or ''tiff''.');
    end
   case 'width'
    width = LocalToNum(value);
    if(~LocalIsPositiveScalar(width))
      error('Width must be a numeric scalar > 0');
    end
   case 'height'
    height = LocalToNum(value);
    if(~LocalIsPositiveScalar(height))
      error('Height must be a numeric scalar > 0');
    end
   case 'color'
    color = lower(value);
    if (~strcmp(color,{'bw','gray','cmyk'}))
      error('Color must be ''bw'', ''gray'' or ''cmyk''.');
    end
   case 'fontmode'
    fontmode = lower(value);
    if (~strcmp(fontmode,{'scaled','fixed'}))
      error('FontMode must be ''scaled'' or ''fixed''.');
    end
   case 'fontsize'
    fontsize = LocalToNum(value);
    if(~LocalIsPositiveScalar(fontsize))
      error('FontSize must be a numeric scalar > 0');
    end
   case 'fontencoding'
    fontencoding = lower(value);
    if (~strcmp(fontencoding,{'latin1','adobe'}))
      error('FontEncoding must be ''latin1'' or ''adobe''.');
    end
   case 'linemode'
    linemode = lower(value);
    if (~strcmp(linemode,{'scaled','fixed'}))
      error('LineMode must be ''scaled'' or ''fixed''.');
    end
   case 'linewidth'
    linewidth = LocalToNum(value);
    if(~LocalIsPositiveScalar(linewidth))
      error('LineWidth must be a numeric scalar > 0');
    end
   case 'renderer'
    renderer = lower(value);
    if (~strcmp(renderer,{'painters','zbuffer','opengl'}))
      error('Renderer must be ''painters'', ''zbuffer'' or ''opengl''.');
    end
   case 'resolution'
    resolution = LocalToNum(value);
    if ~(isnumeric(value) & (prod(size(value)) == 1) & (value >= 0));
      error('Resolution must be a numeric scalar >= 0');
    end
   otherwise
    error(['Unrecognized option ' param '.']);
  end
end

allLines  = findall(H, 'type', 'line');
allText   = findall(H, 'type', 'text');
allAxes   = findall(H, 'type', 'axes');
allImages = findall(H, 'type', 'image');
allLights = findall(H, 'type', 'light');
allPatch  = findall(H, 'type', 'patch');
allSurf   = findall(H, 'type', 'surface');
allRect   = findall(H, 'type', 'rectangle');
allFont   = [allText; allAxes];
allColor  = [allLines; allText; allAxes; allLights];
allMarker = [allLines; allPatch; allSurf];
allEdge   = [allPatch; allSurf];
allCData  = [allImages; allPatch; allSurf];

old.objs = {};
old.prop = {};
old.values = {};

% Process format and preview parameter
showPreview = strcmp(format,'preview');
if showPreview
  format = 'png';
  filename = [tempName '.png'];
end
if strncmp(format,'eps',3) & ~strcmp(preview,'none')
  args = {args{:}, ['-' preview]};
end

hadError = 0;
try
  % Process size parameters
  paperPos = get(H, 'PaperPosition');
  old = LocalPushOldData(old, H, 'PaperPosition', paperPos);
  figureUnits = get(H, 'Units');
  set(H, 'Units', get(H,'PaperUnits'));
  figurePos = get(H, 'Position');
  aspectRatio = figurePos(3)/figurePos(4);
  set(H, 'Units', figureUnits);
  if (width == -1) & (height == -1)
    width = paperPos(3);
    height = paperPos(4);
  elseif (width == -1)
    width = height * aspectRatio;
  elseif (height == -1)
    height = width / aspectRatio;
  end
  set(H, 'PaperPosition', [0 0 width height]);
  paperPosMode = get(H, 'PaperPositionMode');
  old = LocalPushOldData(old, H, 'PaperPositionMode', paperPosMode);
  set(H, 'PaperPositionMode', 'manual');

  % Process rendering parameters
  switch (color)
   case {'bw', 'gray'}
    if ~strcmp(color,'bw') & strncmp(format,'eps',3)
      format = [format 'c'];
    end
    args = {args{:}, ['-d' format]};

    %compute and set gray colormap
    oldcmap = get(H,'Colormap');
    newgrays = 0.30*oldcmap(:,1) + 0.59*oldcmap(:,2) + 0.11*oldcmap(:,3);
    newcmap = [newgrays newgrays newgrays];
    old = LocalPushOldData(old, H, 'Colormap', oldcmap);
    set(H, 'Colormap', newcmap);

    %compute and set ColorSpec and CData properties
    old = LocalUpdateColors(allColor, 'color', old);
    old = LocalUpdateColors(allAxes, 'xcolor', old);
    old = LocalUpdateColors(allAxes, 'ycolor', old);
    old = LocalUpdateColors(allAxes, 'zcolor', old);
    old = LocalUpdateColors(allMarker, 'MarkerEdgeColor', old);
    old = LocalUpdateColors(allMarker, 'MarkerFaceColor', old);
    old = LocalUpdateColors(allEdge, 'EdgeColor', old);
    old = LocalUpdateColors(allEdge, 'FaceColor', old);
    old = LocalUpdateColors(allCData, 'CData', old);
    
   case 'cmyk'
    if strncmp(format,'eps',3)
      format = [format 'c'];
      args = {args{:}, ['-d' format], '-cmyk'};
    else
      args = {args{:}, ['-d' format]};
    end
   otherwise
    error('Invalid Color parameter');
  end
  if (~isempty(renderer))
    args = {args{:}, ['-' renderer]};
  end
  if (~isempty(resolution)) | ~strncmp(format,'eps',3)
    if isempty(resolution)
      resolution = 0;
    end
    args = {args{:}, ['-r' int2str(resolution)]};
  end

  % Process font parameters
  if (~isempty(fontmode))
    oldfonts = LocalGetAsCell(allFont,'FontSize');
    switch (fontmode)
     case 'fixed'
      oldfontunits = LocalGetAsCell(allFont,'FontUnits');
      old = LocalPushOldData(old, allFont, {'FontUnits'}, oldfontunits);
      set(allFont,'FontUnits','points');
      if (fontsize == -1)
	set(allFont,'FontSize',7);
      else
	set(allFont,'FontSize',fontsize);
      end
     case 'scaled'
      if (fontsize == -1)
	wscale = width/figurePos(3);
	hscale = height/figurePos(4);
	scale = min(wscale, hscale);
      else
	scale = fontsize;
      end
      newfonts = LocalScale(oldfonts,scale,5);
      set(allFont,{'FontSize'},newfonts);
     otherwise
      error('Invalid FontMode parameter');
    end
    % make sure we push the size after the units
    old = LocalPushOldData(old, allFont, {'FontSize'}, oldfonts);
  end
  if strcmp(fontencoding,'adobe') & strncmp(format,'eps',3)
    args = {args{:}, '-adobecset'};
  end

  % Process linewidth parameters
  if (~isempty(linemode))
    oldlines = LocalGetAsCell(allMarker,'LineWidth');
    old = LocalPushOldData(old, allMarker, {'LineWidth'}, oldlines);
    switch (linemode)
     case 'fixed'
      if (linewidth == -1)
	set(allMarker,'LineWidth',1);
      else
	set(allMarker,'LineWidth',linewidth);
      end
     case 'scaled'
      if (linewidth == -1)
	wscale = width/figurePos(3);
	hscale = height/figurePos(4);
	scale = min(wscale, hscale);
      else
	scale = linewidth;
      end
      newlines = LocalScale(oldlines, scale, 0.5);
      set(allMarker,{'LineWidth'},newlines);
     otherwise
      error('Invalid LineMode parameter');
    end
  end

  % Export
  print(H, filename, args{:});

catch
  hadError = 1;
end

% Restore figure settings
for n=1:length(old.objs)
  set(old.objs{n}, old.prop{n}, old.values{n});
end

if hadError
  error(deblank(lasterr));
end

% Show preview if requested
if showPreview
  X = imread(filename,'png');
  delete(filename);
  f = figure( 'Name', 'Preview', ...
	      'Menubar', 'none', ...
	      'NumberTitle', 'off', ...
	      'Visible', 'off');
  image(X);
  axis image;
  ax = findobj(f, 'type', 'axes');
  set(ax, 'Units', get(H,'PaperUnits'), ...
	  'Position', [0 0 width height], ...
	  'Visible', 'off');
  set(ax, 'Units', 'pixels');
  axesPos = get(ax,'Position');
  figPos = get(f,'Position');
  rootSize = get(0,'ScreenSize');
  figPos(3:4) = axesPos(3:4);
  if figPos(1) + figPos(3) > rootSize(3)
    figPos(1) = rootSize(3) - figPos(3) - 50;
  end
  if figPos(2) + figPos(4) > rootSize(4)
    figPos(2) = rootSize(4) - figPos(4) - 50;
  end
  set(f, 'Position',figPos, ...
	 'Visible', 'on');
end

%
%  Local Functions
%

function outData = LocalPushOldData(inData, objs, prop, values)
outData.objs = {inData.objs{:}, objs};
outData.prop = {inData.prop{:}, prop};
outData.values = {inData.values{:}, values};

function cellArray = LocalGetAsCell(fig,prop);
cellArray = get(fig,prop);
if (~isempty(cellArray)) & (~iscell(cellArray))
  cellArray = {cellArray};
end

function newArray = LocalScale(inArray, scale, minValue)
n = length(inArray);
newArray = cell(n,1);
for k=1:n
  newArray{k} = max(minValue,scale*inArray{k}(1));
end

function newArray = LocalMapToGray(inArray);
n = length(inArray);
newArray = cell(n,1);
for k=1:n
  color = inArray{k};
  if (~isempty(color))
    if ischar(color)
      switch color(1)
       case 'y'
	color = [1 1 0];
       case 'm'
	color = [1 0 1];
       case 'c'
	color = [0 1 1];
       case 'r'
	color = [1 0 0];
       case 'g'
	color = [0 1 0];
       case 'b'
	color = [0 0 1];
       case 'w'
	color = [1 1 1];
       case 'k'
	color = [0 0 0];
       otherwise
	newArray{k} = color;
      end
    end
    if ~ischar(color)
      color = 0.30*color(1) + 0.59*color(2) + 0.11*color(3);
    end
  end
  if isempty(color) | ischar(color)
    newArray{k} = color;
  else
    newArray{k} = [color color color];
  end
end

function newArray = LocalMapCData(inArray);
n = length(inArray);
newArray = cell(n,1);
for k=1:n
  color = inArray{k};
  if (ndims(color) == 3) & isa(color,'double')
    gray = 0.30*color(:,:,1) + 0.59*color(:,:,2) + 0.11*color(:,:,3);
    color(:,:,1) = gray;
    color(:,:,2) = gray;
    color(:,:,3) = gray;
  end
  newArray{k} = color;
end

function outData = LocalUpdateColors(inArray, prop, inData)
value = LocalGetAsCell(inArray,prop);
outData.objs = {inData.objs{:}, inArray};
outData.prop = {inData.prop{:}, {prop}};
outData.values = {inData.values{:}, value};
if (~isempty(value))
  if strcmp(prop,'CData') 
    value = LocalMapCData(value);
  else
    value = LocalMapToGray(value);
  end
  set(inArray,{prop},value);
end

function bool = LocalIsPositiveScalar(value)
bool = isnumeric(value) & ...
       prod(size(value)) == 1 & ...
       value > 0;

function value = LocalToNum(value)
if ischar(value)
  value = str2num(value);
end
