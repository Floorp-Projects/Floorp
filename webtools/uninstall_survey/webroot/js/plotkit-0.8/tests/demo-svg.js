/* actual tests */

function drawDemo(element, layout, options) {
    var renderer = new PlotKit.SVGRenderer(element, layout, options);
    renderer.render();
}

function demoWithStyle(style) {
    // datasets 
    var dataset = [
       [0,1],
       [1,4],
       [2,16],
       [3,8],
       [4,16],
       [5,4],
       [6,1]
    ];

    var dataset_rev = [
       [6,0],
       [5,1],
       [4,4],
       [3,9],
       [2,16],
       [1,25],
       [0,36]
    ];

    var options = {
        "drawBackground": false,
        "shouldFill": true,
        "shouldStroke": true,
        "drawXAxis": true,
        "drawYAxis": true,
        "padding": {left: 40, right: 10, top: 10, bottom: 20},
        "axisLabelUseDiv": false
    };

    if (style == "pie") {
        options["padding"] = {left: 50, right: 50, top: 50, bottom: 50}
    }

    var layout = new PlotKit.Layout(style, options);
    layout.addDataset("noname", dataset);
    layout.evaluate();

    // stroke/fill toggle
    drawDemo($('test1'),  layout, options);
    options["shouldFill"] = false;
    drawDemo($('test2'),  layout, options);
    options["shouldStroke"] = false;
    options["shouldFill"] = true;
    drawDemo($('test3'),  layout, options);
    
    // drawing axis
    options["shouldFill"] = true;
    options["shouldStroke"] = true;
    options["drawXAxis"] = false;
    options["drawYAxis"] = false;
    drawDemo($('test4'),  layout, options);
    options["drawXAxis"] = true;
    drawDemo($('test5'),  layout, options);
    options["drawYAxis"] = true;
    options["drawXAxis"] = false;
    drawDemo($('test6'),  layout, options);    

    // changing background color and axis color
    options["drawXAxis"] = true;
    options["colorScheme"] = PlotKit.Base.colorScheme().reverse()
    drawDemo($('test7'),  layout, options);
    options["drawBackground"] = true;
    options["backgroundColor"] = Color.blueColor().lighterColorWithLevel(0.45);
    drawDemo($('test8'),  layout, options);
    options["drawBackground"] = false;
    options["axisLineColor"] = Color.grayColor();
    options["axisLabelColor"] = Color.grayColor();
    options["axisLabelFontSize"] = 9;
    drawDemo($('test9'),  layout, options);    

    // layout customisation
    options["colorScheme"] = PlotKit.Base.colorScheme();
    options["axisLineColor"] = Color.blackColor();
    options["axisLabelColor"] = Color.blackColor();
    options["axisLabelFontSize"] = 9;
    options["yNumberOfTicks"] = 3;

    layout.options.yNumberOfTicks = 3;
    layout.evaluate();
    drawDemo($('test10'), layout, options);

    layout.options.xNumberOfTicks = 3;
    layout.evaluate();
    drawDemo($('test11'), layout, options);

    layout.options.barWidthFillFraction = 0.5;
    layout.evaluate();
    drawDemo($('test12'), layout, options);
       

    // custom labels
    layout.options.barWidthFillFraction = 0.75;
    layout.options.yTicks = [{v:10}, {v:20}, {v:30}, {v:40}];
    layout.evaluate();
    drawDemo($('test13'), layout, options);

    layout.options.xTicks = [
            {v:1, label:"one"}, 
            {v:2, label:"two"}, 
            {v:3, label:"three"}, 
            {v:4, label:"four"},
            {v:5, label:"five"},
            {v:6, label:"six"}];
    layout.evaluate();
    drawDemo($('test14'), layout, options);

    layout.addDataset("reversed", dataset_rev);
    layout.options.yTicks = null;
    layout.options.xTicks = null;
    layout.options.xNumberOfTicks = 10;
    layout.options.yNumberOfTicks = 5;
    layout.options.xTicks = null;
    layout.options.yTicks = null;
    layout.evaluate();
    drawDemo($('test15'), layout, options);

}

function demo() {
    demoWithStyle("bar");
}

addLoadEvent(demo);
