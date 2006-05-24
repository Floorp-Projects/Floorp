/* actual tests */

var options = {
    "axisLineWidth": 2.0,
    "axisLabelColor": Color.grayColor(),
    "axisLineColor": Color.blackColor(),
    "padding": {top: 5, bottom: 20, left: 40, right: 10},
    "backgroundColor": Color.whiteColor(),
    "strokeColor": null,
    "axisLabelUseDiv": false
};

function genericTest(num, plotStyle) {
    var l = new PlotKit.Layout(plotStyle, {});
    var success = l.addDatasetFromTable("data" + num, $("test" + num));
    l.evaluate();
    var c = $("test" + num + "canvas");
    var g = new PlotKit.SVGRenderer(c, l, options);
    g.render();
}

function genericTestAndClear(num, plotStyle) {
    var l = new PlotKit.Layout(plotStyle, {});
    l.addDatasetFromTable("data" + num, $("test" + num));   
    l.evaluate();
    var c = $("test" + num + "canvas");
    var g = new PlotKit.SVGRenderer(c, l, {});
    g.render();
    g.clear();
}

function dualDataSet(num, plotStyle) {
    var l = new PlotKit.Layout(plotStyle, {});
    l.addDatasetFromTable("data1." + num, $("test" + num), 0, 1);   
    l.addDatasetFromTable("data2." + num, $("test" + num), 0, 2);   
    l.evaluate();
    var c = $("test" + num + "canvas");
    var g = new PlotKit.SVGRenderer(c, l, options);
    g.render();
}


/* create HTML for tests */

function makeTableRow(list) {
    return TR({}, map(partial(TD, null), list));
}

function generateTestTable(num, data) {
    var tableid = "test" + num;
    var tablehead = THEAD(null, map(makeTableRow, [["x", "y"]]));
    var tablebody = TBODY(null, map(makeTableRow, data));
    
    var table = TABLE({"class": "data", "id": tableid}, [tablehead, tablebody]);
    return table;
}

function generateCanvas(num) {
    var canvasid = "test" + num + "canvas";
    var canvasopts = {
        "id": canvasid,
        "width": 400,
        "height": 200,
        "version": "1.1",
        "baseProfile": "full"
    };

    var canvas = PlotKit.SVGRenderer.SVG(canvasopts, "");
    return canvas
}

function generateUnitTest(num, func, data, type, desc) {
    var table = DIV({"class": "data"}, generateTestTable(num, data));
    var canvas = DIV({"class": "canvas"}, generateCanvas(num));
    var ending = DIV({"class":"ending"}, desc);
    
    addLoadEvent(partial(func, num, type));
    
    return DIV({"class": "unit"}, [table, canvas, ending]);
    return DIV({"class": "unit"}, [table, ending]);
}

function generateTests() {
    var tests = $('tests');

    // datasets 
    var simpleData1 = [[0, 0], [1, 1], [2, 2], [3, 3]];
    var simpleData2 = [[1, 2], [2, 3], [3, 4], [4, 5]];
    var singleData = [[1, 1]];

    var ninety = [[1, 9], [2, 1]];

    var floatData1 = [[0, 0.5], [1, 0.4], [2, 0.3]];
    var missingData = [[0, 1], [1, 4], [3, 16], [5, 17]];

    var dualData = [[0,0,0], [1,2,1], [2,4,4], [3,8,9], [4,16,16], [5,32,25], [6, 64, 36], [7, 128, 49]];

    tests.appendChild(H2(null, "Simple Tests"));

    tests.appendChild(generateUnitTest(1, genericTest, simpleData1, "bar", ""));

    tests.appendChild(generateUnitTest(2, genericTest, simpleData1, 
    "line", ""));
    tests.appendChild(generateUnitTest(3, genericTest, simpleData2,
    "pie", ""));

    tests.appendChild(H2(null, "One Value Set"));

    tests.appendChild(generateUnitTest(4, genericTest, singleData,
    "bar", ""));
    tests.appendChild(generateUnitTest(5, genericTest, singleData, 
    "line", ""));
    tests.appendChild(generateUnitTest(6, genericTest, singleData, 
    "pie", ""));

    tests.appendChild(H2(null, "Float Values Set"));
    tests.appendChild(generateUnitTest(7, genericTest, floatData1,
    "bar", ""));
    tests.appendChild(generateUnitTest(8, genericTest, floatData1, 
    "line", ""));
    tests.appendChild(generateUnitTest(9, genericTest, floatData1, 
    "pie", ""));    

    tests.appendChild(H2(null, "Dual Value Set"));
    tests.appendChild(generateUnitTest(10, dualDataSet, dualData,
    "bar", ""));
    tests.appendChild(generateUnitTest(11, dualDataSet, dualData, 
    "line", ""));

    tests.appendChild(H2(null, "Drawing and Clearing"));
    tests.appendChild(generateUnitTest(12, genericTest, floatData1,
    "bar", ""));    
    tests.appendChild(generateUnitTest(13, genericTestAndClear, floatData1,
    "bar", ""));
    tests.appendChild(generateUnitTest(14, genericTest, floatData1,
    "pie", ""));    
    tests.appendChild(generateUnitTest(15, genericTestAndClear, floatData1,
    "pie", ""));
    
    tests.appendChild(H2(null, "Testing Circle Drawing"));

     tests.appendChild(generateUnitTest(16, genericTest, ninety,
     "pie", ""));

}

addLoadEvent(generateTests);
