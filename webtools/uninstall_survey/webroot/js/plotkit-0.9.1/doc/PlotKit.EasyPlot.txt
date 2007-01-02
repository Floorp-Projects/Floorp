{% extends "basex.html" %}
{% load markup %}
{% block pageid %}code{% endblock %}
{% block headers %}
<link href="doc.css" media="screen" rel="stylesheet" type="text/css" />
{% endblock %}
{% block title %}PlotKit.Canvas{% endblock %}

{% block content %}
<div class="page doc api">
{% filter markdown %}
[PlotKit Home](PlotKit.html) | [<<](PlotKit.SweetSVG.html) 

PlotKit EasyPlot
================

EasyPlot is a wrapper around the various PlotKit classes to allow you to get a chart plotted as quick as possible with as little code as possible. Using EasyPlot, you will get a chart started with just a single line.

Constructor
-----------
``PlotKit.EasyPlot(style, options, divElement, datasourceArray)``

EasyPlot object will automatically choose the supported render method, currently Canvas or SVG in that order, and render the datasources given in ``datasourceArray``.

### Arguments:

* ``style`` may be ``line``, ``bar`` or ``pie``.
* ``options`` is an associative dictionary that is the combined options of both ``Layout`` and ``Renderer``.
* ``divElement`` is the container that the chart should be rendered in. It is best that the ``width`` and ``height`` attribute is set in the ``DIV`` element.
* ``datasourceArray`` is an array of data sources. The elements of the array can either be a two dimensional array given in ``Plotkit.Layout.addDataset`` or it can be a string that points to the relative URL of a comma separated data file.

EasyPlot Example
----------------

    <div id="example" style="margin: 0 auto 0 auto;" width="400" height="400"></div>
    
    <script type="text/javascript">
    var data = [[0,0], [1,2], [2,3], [3, 7], [4, 8], [5, 6]];
    var plotter = EasyPlot("line", {}, $("example"), [data, "sample.txt"]);
    </script>


In this example, two datasets are passed, one defined as a 2D array and another which is a comma separated text file (CSV) at the location "sample.txt".  A demonstration of this is found in the [QuickStartEasy][] example.


[QuickStartEasy]: http://media.liquidx.net/js/plotkit-tests/quickstart-easy.html

{% endfilter %}
</div>
{% endblock %}