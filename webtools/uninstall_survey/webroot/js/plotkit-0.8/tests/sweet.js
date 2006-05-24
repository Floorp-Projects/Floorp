/* actual tests */

var opts = {
    "IECanvasHTC": "../plotkit/iecanvas.htc",
    "enableEvents": false
};

function genericTest(num, plotStyle) {
	var l = new PlotKit.Layout(plotStyle, {});
	var success = l.addDatasetFromTable("data" + num, $("test" + num));
	l.evaluate();
	var c = $("test" + num + "canvas");
	var g = new PlotKit.SweetCanvasRenderer(c, l, opts);
	g.render();
}

function dualDataSet(num, plotStyle) {
	var l = new PlotKit.Layout(plotStyle, {});
	l.addDatasetFromTable("data1." + num, $("test" + num), 0, 1);	
	l.addDatasetFromTable("data2." + num, $("test" + num), 0, 2);	
	l.evaluate();
	var c = $("test" + num + "canvas");
	var g = new PlotKit.SweetCanvasRenderer(c, l, opts);
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
    var canvas = CANVAS({"id":canvasid, "width": "400", "height": "200"}, "");
    return canvas
}

function generateUnitTest(num, func, data, type, desc) {
    var table = DIV({"class": "data"}, generateTestTable(num, data));
    var canvas = DIV({"class": "canvas"}, generateCanvas(num));
    var ending = DIV({"class":"ending"}, desc);
    
    addLoadEvent(partial(func, num, type));
    
    return DIV({"class": "unit"}, [table, canvas, ending]);
    
}

function generateTests() {
    var tests = $('tests');
    
    // datasets 
    var simpleData1 = [[0, 0], [1, 1], [2, 2], [3, 3]];
    var simpleData2 = [[1, 2], [2, 3], [3, 4], [4, 5]];
    var singleData = [[1, 1]];
    
    var floatData1 = [[0, 0.5], [1, 0.4], [2, 0.3]];
    var missingData = [[0, 1], [1, 4], [3, 16], [5, 17]];
    
    var dualData = [[0,0,0], [1,2,1], [2,4,4], [3,8,9], [4,16,16], [5,32,25], [6, 64, 36], [7, 128, 49]];

    tests.appendChild(H2(null, "Simple Tests"));

    tests.appendChild(generateUnitTest(1, genericTest, simpleData1,
    "bar", ""));
    tests.appendChild(generateUnitTest(2, dualDataSet, dualData,
    "bar", ""));

    tests.appendChild(generateUnitTest(3, genericTest, simpleData1,
    "line", ""));
    tests.appendChild(generateUnitTest(4, dualDataSet, dualData,
    "line", ""));
    
    tests.appendChild(generateUnitTest(5, genericTest, simpleData1,
    "pie", ""));

}

addLoadEvent(generateTests);
