ProgressBar = Utilities.createClass(
    function(element, ranges)
    {
        this._element = element;
        this._ranges = ranges;
        this._currentRange = 0;
        this._updateElement();
    }, {

    _updateElement: function()
    {
        this._element.style.width = (this._currentRange * (100 / this._ranges)) + "%";
    },

    incrementRange: function()
    {
        ++this._currentRange;
        this._updateElement();
    }
});

DeveloperResultsTable = Utilities.createSubclass(ResultsTable,
    function(element, headers)
    {
        ResultsTable.call(this, element, headers);
    }, {

    _addGraphButton: function(td, testName, testResult, testData)
    {
        var button = Utilities.createElement("button", { class: "small-button" }, td);
        button.textContent = Strings.text.graph + "…";
        button.testName = testName;
        button.testResult = testResult;
        button.testData = testData;

        button.addEventListener("click", function(e) {
            benchmarkController.showTestGraph(e.target.testName, e.target.testResult, e.target.testData);
        });
    },

    _isNoisyMeasurement: function(jsonExperiment, data, measurement, options)
    {
        const percentThreshold = 10;
        const averageThreshold = 2;

        if (measurement == Strings.json.measurements.percent)
            return data[Strings.json.measurements.percent] >= percentThreshold;

        if (jsonExperiment == Strings.json.frameLength && measurement == Strings.json.measurements.average)
            return Math.abs(data[Strings.json.measurements.average] - options["frame-rate"]) >= averageThreshold;

        return false;
    },

    _addTest: function(testName, testResult, options, testData)
    {
        var row = Utilities.createElement("tr", {}, this.element);

        var isNoisy = false;
        [Strings.json.complexity, Strings.json.frameLength].forEach(function (experiment) {
            var data = testResult[experiment];
            for (var measurement in data) {
                if (this._isNoisyMeasurement(experiment, data, measurement, options))
                    isNoisy = true;
            }
        }, this);

        this._flattenedHeaders.forEach(function (header) {
            var className = "";
            if (header.className) {
                if (typeof header.className == "function")
                    className = header.className(testResult, options);
                else
                    className = header.className;
            }

            if (header.text == Strings.text.testName) {
                if (isNoisy)
                    className += " noisy-results";
                var td = Utilities.createElement("td", { class: className }, row);
                td.textContent = testName;
                return;
            }

            var td = Utilities.createElement("td", { class: className }, row);
            if (header.title == Strings.text.graph) {
                this._addGraphButton(td, testName, testResult, testData);
            } else if (!("text" in header)) {
                td.textContent = testResult[header.title];
            } else if (typeof header.text == "string") {
                var data = testResult[header.text];
                if (typeof data == "number")
                    data = data.toFixed(2);
                td.textContent = data;
            } else
                td.textContent = header.text(testResult);
        }, this);
    }
});

Utilities.extendObject(window.benchmarkRunnerClient, {
    testsCount: null,
    progressBar: null,

    initialize: function(suites, options)
    {
        this.testsCount = this.iterationCount * suites.reduce(function (count, suite) { return count + suite.tests.length; }, 0);
        this.options = options;
    },

    willStartFirstIteration: function()
    {
        this.results = new ResultsDashboard(this.options);
        this.progressBar = new ProgressBar(document.getElementById("progress-completed"), this.testsCount);
    },

    didRunTest: function(testData)
    {
        this.progressBar.incrementRange();
        this.results.calculateScore(testData);
    }
});

Utilities.extendObject(window.sectionsManager, {
    setSectionHeader: function(sectionIdentifier, title)
    {
        document.querySelector("#" + sectionIdentifier + " h1").textContent = title;
    },

    populateTable: function(tableIdentifier, headers, dashboard)
    {
        var table = new DeveloperResultsTable(document.getElementById(tableIdentifier), headers);
        table.showIterations(dashboard);
    }
});

window.optionsManager =
{
    valueForOption: function(name)
    {
        var formElement = document.forms["benchmark-options"].elements[name];
        if (formElement.type == "checkbox")
            return formElement.checked;
        else if (formElement.constructor === HTMLCollection) {
            for (var i = 0; i < formElement.length; ++i) {
                var radio = formElement[i];
                if (radio.checked)
                    return formElement.value;
            }
            return null;
        }
        return formElement.value;
    },

    updateUIFromLocalStorage: function()
    {
        var formElements = document.forms["benchmark-options"].elements;

        for (var i = 0; i < formElements.length; ++i) {
            var formElement = formElements[i];
            var name = formElement.id || formElement.name;
            var type = formElement.type;

            var value = localStorage.getItem(name);
            if (value === null)
                continue;

            if (type == "number")
                formElements[name].value = +value;
            else if (type == "checkbox")
                formElements[name].checked = value == "true";
            else if (type == "radio")
                formElements[name].value = value;
        }
    },

    updateLocalStorageFromUI: function()
    {
        var formElements = document.forms["benchmark-options"].elements;
        var options = {};

        for (var i = 0; i < formElements.length; ++i) {
            var formElement = formElements[i];
            var name = formElement.id || formElement.name;
            var type = formElement.type;

            if (type == "number")
                options[name] = +formElement.value;
            else if (type == "checkbox")
                options[name] = formElement.checked;
            else if (type == "radio") {
                var radios = formElements[name];
                if (radios.constructor === HTMLCollection) {
                    for (var j = 0; j < radios.length; ++j) {
                        var radio = radios[j];
                        if (radio.checked) {
                            options[name] = radio.value;
                            break;
                        }
                    }
                } else
                    options[name] = formElements[name].value;
            }

            try {
                localStorage.setItem(name, options[name]);
            } catch (e) {}
        }

        return options;
    },

    updateDisplay: function()
    {
        document.body.classList.remove("display-minimal");
        document.body.classList.remove("display-progress-bar");

        document.body.classList.add("display-" + optionsManager.valueForOption("display"));
    },

    updateTiles: function()
    {
        document.body.classList.remove("tiles-big");
        document.body.classList.remove("tiles-classic");

        document.body.classList.add("tiles-" + optionsManager.valueForOption("tiles"));
    }
};

window.suitesManager =
{
    _treeElement: function()
    {
        return document.querySelector("#suites > .tree");
    },

    _suitesElements: function()
    {
        return document.querySelectorAll("#suites > ul > li");
    },

    _checkboxElement: function(element)
    {
        return element.querySelector("input[type='checkbox']:not(.expand-button)");
    },

    _editElement: function(element)
    {
        return element.querySelector("input[type='number']");
    },

    _editsElements: function()
    {
        return document.querySelectorAll("#suites input[type='number']");
    },

    _localStorageNameForTest: function(suiteName, testName)
    {
        return suiteName + "/" + testName;
    },

    _updateSuiteCheckboxState: function(suiteCheckbox)
    {
        var numberEnabledTests = 0;
        suiteCheckbox.testsElements.forEach(function(testElement) {
            var testCheckbox = this._checkboxElement(testElement);
            if (testCheckbox.checked)
                ++numberEnabledTests;
        }, this);
        suiteCheckbox.checked = numberEnabledTests > 0;
        suiteCheckbox.indeterminate = numberEnabledTests > 0 && numberEnabledTests < suiteCheckbox.testsElements.length;
    },

    isAtLeastOneTestSelected: function()
    {
        var suitesElements = this._suitesElements();

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);

            if (suiteCheckbox.checked)
                return true;
        }

        return false;
    },

    _onChangeSuiteCheckbox: function(event)
    {
        var selected = event.target.checked;
        event.target.testsElements.forEach(function(testElement) {
            var testCheckbox = this._checkboxElement(testElement);
            testCheckbox.checked = selected;
        }, this);
        benchmarkController.updateStartButtonState();
    },

    _onChangeTestCheckbox: function(suiteCheckbox)
    {
        this._updateSuiteCheckboxState(suiteCheckbox);
        benchmarkController.updateStartButtonState();
    },

    _createSuiteElement: function(treeElement, suite, id)
    {
        var suiteElement = Utilities.createElement("li", {}, treeElement);
        var expand = Utilities.createElement("input", { type: "checkbox",  class: "expand-button", id: id }, suiteElement);
        var label = Utilities.createElement("label", { class: "tree-label", for: id }, suiteElement);

        var suiteCheckbox = Utilities.createElement("input", { type: "checkbox" }, label);
        suiteCheckbox.suite = suite;
        suiteCheckbox.onchange = this._onChangeSuiteCheckbox.bind(this);
        suiteCheckbox.testsElements = [];

        label.appendChild(document.createTextNode(" " + suite.name));
        return suiteElement;
    },

    _createTestElement: function(listElement, test, suiteCheckbox)
    {
        var testElement = Utilities.createElement("li", {}, listElement);
        var span = Utilities.createElement("label", { class: "tree-label" }, testElement);

        var testCheckbox = Utilities.createElement("input", { type: "checkbox" }, span);
        testCheckbox.test = test;
        testCheckbox.onchange = function(event) {
            this._onChangeTestCheckbox(event.target.suiteCheckbox);
        }.bind(this);
        testCheckbox.suiteCheckbox = suiteCheckbox;

        suiteCheckbox.testsElements.push(testElement);
        span.appendChild(document.createTextNode(" " + test.name + " "));

        testElement.appendChild(document.createTextNode(" "));
        var link = Utilities.createElement("span", {}, testElement);
        link.classList.add("link");
        link.textContent = "link";
        link.suiteName = Utilities.stripNonASCIICharacters(suiteCheckbox.suite.name);
        link.testName = test.name;
        link.onclick = function(event) {
            var element = event.target;
            var title = "Link to run “" + element.testName + "” with current options:";
            var url = location.href.split(/[?#]/)[0];
            var options = optionsManager.updateLocalStorageFromUI();
            Utilities.extendObject(options, {
                "suite-name": element.suiteName,
                "test-name": Utilities.stripNonASCIICharacters(element.testName)
            });
            var complexity = suitesManager._editElement(element.parentNode).value;
            if (complexity)
                options.complexity = complexity;
            prompt(title, url + Utilities.convertObjectToQueryString(options));
        };

        var complexity = Utilities.createElement("input", { type: "number" }, testElement);
        complexity.relatedCheckbox = testCheckbox;
        complexity.oninput = function(event) {
            var relatedCheckbox = event.target.relatedCheckbox;
            relatedCheckbox.checked = true;
            this._onChangeTestCheckbox(relatedCheckbox.suiteCheckbox);
        }.bind(this);
        return testElement;
    },

    createElements: function()
    {
        var treeElement = this._treeElement();

        Suites.forEach(function(suite, index) {
            var suiteElement = this._createSuiteElement(treeElement, suite, "suite-" + index);
            var listElement = Utilities.createElement("ul", {}, suiteElement);
            var suiteCheckbox = this._checkboxElement(suiteElement);

            suite.tests.forEach(function(test) {
                this._createTestElement(listElement, test, suiteCheckbox);
            }, this);
        }, this);
    },

    updateEditsElementsState: function()
    {
        var editsElements = this._editsElements();
        var showComplexityInputs = ["fixed", "step"].indexOf(optionsManager.valueForOption("controller")) != -1;

        for (var i = 0; i < editsElements.length; ++i) {
            var editElement = editsElements[i];
            if (showComplexityInputs)
                editElement.classList.add("selected");
            else
                editElement.classList.remove("selected");
        }
    },

    updateUIFromLocalStorage: function()
    {
        var suitesElements = this._suitesElements();

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);
            var suite = suiteCheckbox.suite;

            suiteCheckbox.testsElements.forEach(function(testElement) {
                var testCheckbox = this._checkboxElement(testElement);
                var testEdit = this._editElement(testElement);
                var test = testCheckbox.test;

                var str = localStorage.getItem(this._localStorageNameForTest(suite.name, test.name));
                if (str === null)
                    return;

                var value = JSON.parse(str);
                testCheckbox.checked = value.checked;
                testEdit.value = value.complexity;
            }, this);

            this._updateSuiteCheckboxState(suiteCheckbox);
        }

        benchmarkController.updateStartButtonState();
    },

    updateLocalStorageFromUI: function()
    {
        var suitesElements = this._suitesElements();
        var suites = [];

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);
            var suite = suiteCheckbox.suite;

            var tests = [];
            suiteCheckbox.testsElements.forEach(function(testElement) {
                var testCheckbox = this._checkboxElement(testElement);
                var testEdit = this._editElement(testElement);
                var test = testCheckbox.test;

                if (testCheckbox.checked) {
                    test.complexity = testEdit.value;
                    tests.push(test);
                }

                var value = { checked: testCheckbox.checked, complexity: testEdit.value };
                try {
                    localStorage.setItem(this._localStorageNameForTest(suite.name, test.name), JSON.stringify(value));
                } catch (e) {}
            }, this);

            if (tests.length)
                suites.push(new Suite(suiteCheckbox.suite.name, tests));
        }

        return suites;
    },

    suitesFromQueryString: function(suiteName, testName, oskey=null)
    {
        var suites = [];
        var suiteRegExp = new RegExp(suiteName, "i");
        var testRegExp = new RegExp(testName, "i");

        for (var i = 0; i < Suites.length; ++i) {
            var suite = Suites[i];
            if (!Utilities.stripNonASCIICharacters(suite.name).match(suiteRegExp))
                continue;

            var test;
            for (var j = 0; j < suite.tests.length; ++j) {
                suiteTest = suite.tests[j];
                // MOZILLA: Run all the tests in a given suite
                if (typeof(testName) === "undefined") {
                    let complexity = {"HTMLsuite": {
                        "CSSbouncingcircles": {"win": 322, "linux64": 322, "osx": 218},
                        "CSSbouncingclippedrects": {"win": 520, "linux64": 520, "osx": 75},
                        "CSSbouncinggradientcircles": {"win": 402, "linux64": 402, "osx": 97},
                        "CSSbouncingblendcircles": {"win": 171, "linux64": 171, "osx": 254},
                        "CSSbouncingfiltercircles": {"win": 189, "linux64": 189, "osx": 189},
                        "CSSbouncingSVGimages": {"win": 329, "linux64": 329, "osx": 392},
                        "CSSbouncingtaggedimages": {"win": 255, "linux64": 255, "osx": 351},
                        "Leaves20": {"win": 262, "linux64": 262, "osx": 191},
                        "Focus20": {"win": 15, "linux64": 15, "osx": 18},
                        "DOMparticlesSVGmasks": {"win": 390, "linux64": 390, "osx": 54},
                        "CompositedTransforms": {"win": 400, "linux64": 400, "osx": 75}
                      }, "Animometer": {
                        "Multiply": {"win": 391, "linux64": 391, "osx": 193},
                        "CanvasArcs": {"win": 1287, "linux64": 1287, "osx": 575},
                        "Leaves": {"win": 550, "linux64": 550, "osx": 271},
                        "Paths": {"win": 4070, "linux64": 4070, "osx": 2024},
                        "CanvasLines": {"win": 4692, "linux64": 4692, "osx": 10932},
                        "Focus": {"win": 44, "linux64": 44, "osx": 32},
                        "Images": {"win": 293, "linux64": 293, "osx": 188},
                        "Design": {"win": 60, "linux64": 60, "osx": 17},
                        "Suits": {"win": 210, "linux64": 210, "osx": 145}
                      }
                    };
                    if (oskey == null) {
                        oskey = "linux64";
                    }
                    suiteTest.complexity = complexity[suiteName][Utilities.stripNonASCIICharacters(suiteTest.name)][oskey];
                    suites.push(new Suite(suiteName, [suiteTest]));
                    continue;
                }

                if (Utilities.stripNonASCIICharacters(suiteTest.name).match(testRegExp)) {
                    test = suiteTest;
                    break;
                }
            }

            if (!test)
                continue;

            suites.push(new Suite(suiteName, [test]));
        };

        return suites;
    },

    updateLocalStorageFromJSON: function(results)
    {
        for (var suiteName in results[Strings.json.results.tests]) {
            var suiteResults = results[Strings.json.results.tests][suiteName];
            for (var testName in suiteResults) {
                var testResults = suiteResults[testName];
                var data = testResults[Strings.json.controller];
                var complexity = Math.round(data[Strings.json.measurements.average]);

                var value = { checked: true, complexity: complexity };
                try {
                    localStorage.setItem(this._localStorageNameForTest(suiteName, testName), JSON.stringify(value));
                } catch (e) {}
            }
        }
    }
}

Utilities.extendObject(window.benchmarkController, {
    initialize: function()
    {
        document.forms["benchmark-options"].addEventListener("change", benchmarkController.onBenchmarkOptionsChanged, true);
        document.forms["graph-type"].addEventListener("change", benchmarkController.onGraphTypeChanged, true);
        document.forms["time-graph-options"].addEventListener("change", benchmarkController.onTimeGraphOptionsChanged, true);
        document.forms["complexity-graph-options"].addEventListener("change", benchmarkController.onComplexityGraphOptionsChanged, true);
        optionsManager.updateUIFromLocalStorage();
        optionsManager.updateDisplay();
        optionsManager.updateTiles();

        if (benchmarkController.startBenchmarkImmediatelyIfEncoded())
            return;

        benchmarkController.addOrientationListenerIfNecessary();
        suitesManager.createElements();
        suitesManager.updateUIFromLocalStorage();
        suitesManager.updateEditsElementsState();

        var dropTarget = document.getElementById("drop-target");
        function stopEvent(e) {
            e.stopPropagation();
            e.preventDefault();
        }
        dropTarget.addEventListener("dragenter", stopEvent, false);
        dropTarget.addEventListener("dragover", stopEvent, false);
        dropTarget.addEventListener("dragleave", stopEvent, false);
        dropTarget.addEventListener("drop", function (e) {
            e.stopPropagation();
            e.preventDefault();

            if (!e.dataTransfer.files.length)
                return;

            var file = e.dataTransfer.files[0];

            var reader = new FileReader();
            reader.filename = file.name;
            reader.onload = function(e) {
                var run = JSON.parse(e.target.result);
                if (run.debugOutput instanceof Array)
                    run = run.debugOutput[0];
                benchmarkRunnerClient.results = new ResultsDashboard(run.options, run.data);
                benchmarkController.showResults();
            };

            reader.readAsText(file);
            document.title = "File: " + reader.filename;
        }, false);
    },

    updateStartButtonState: function()
    {
        var startButton = document.getElementById("run-benchmark");
        if ("isInLandscapeOrientation" in this && !this.isInLandscapeOrientation) {
            startButton.disabled = true;
            return;
        }
        startButton.disabled = !suitesManager.isAtLeastOneTestSelected();
    },

    onBenchmarkOptionsChanged: function(event)
    {
        switch (event.target.name) {
        case "controller":
            suitesManager.updateEditsElementsState();
            break;
        case "display":
            optionsManager.updateDisplay();
            break;
        case "tiles":
            optionsManager.updateTiles();
            break;
        }
    },

    startBenchmark: function()
    {
        benchmarkController.determineCanvasSize();
        benchmarkController.options = optionsManager.updateLocalStorageFromUI();
        benchmarkController.suites = suitesManager.updateLocalStorageFromUI();
        this._startBenchmark(benchmarkController.suites, benchmarkController.options, "running-test");
    },

    startBenchmarkImmediatelyIfEncoded: function()
    {
        benchmarkController.options = Utilities.convertQueryStringToObject(location.search);
        if (!benchmarkController.options)
            return false;

        this.raptor = benchmarkController.options["raptor"];
        benchmarkController.suites = suitesManager.suitesFromQueryString(benchmarkController.options["suite-name"],
                                                                         benchmarkController.options["test-name"],
                                                                         benchmarkController.options["oskey"]);
        if (!benchmarkController.suites.length)
            return false;

        setTimeout(function() {
            this._startBenchmark(benchmarkController.suites, benchmarkController.options, "running-test");
        }.bind(this), 0);
        return true;
    },

    restartBenchmark: function()
    {
        this._startBenchmark(benchmarkController.suites, benchmarkController.options, "running-test");
    },

    showResults: function()
    {
        if (!this.addedKeyEvent) {
            document.addEventListener("keypress", this.handleKeyPress, false);
            this.addedKeyEvent = true;
        }

        var dashboard = benchmarkRunnerClient.results;
        if (["ramp", "ramp30"].indexOf(dashboard.options["controller"]) != -1)
            Headers.details[3].disabled = true;
        else {
            Headers.details[1].disabled = true;
            Headers.details[4].disabled = true;
        }

        if (dashboard.options[Strings.json.configuration]) {
            document.body.classList.remove("small", "medium", "large");
            document.body.classList.add(dashboard.options[Strings.json.configuration]);
        }

        var score = dashboard.score;
        var item = dashboard._results['iterationsResults'][0];
        var fullNames = new Array;
        var values = new Array;
        for (var suite in item['testsResults']) {
            for (var subtest in item['testsResults'][suite.toString()]) {
                fullNames.push(suite.toString() + "-" + subtest.toString().replace(/ /g, '_'));
                if (dashboard.options["controller"] === "fixed") {
                    values.push(item['testsResults'][suite.toString()][subtest.toString()]['frameLength']['average']);
                } else if (dashboard.options["controller"] === "ramp") {
                    values.push(item['testsResults'][suite.toString()][subtest.toString()]['complexity']['bootstrap']['median']);
                }

           }
        }
        if (typeof tpRecordTime !== "undefined") {
            tpRecordTime(values.join(','), 0, fullNames.join(','));
        }
        if (this.raptor) {
          _data = ['raptor-benchmark', 'motionmark', item['testsResults']];
          window.postMessage(_data, '*');
          window.sessionStorage.setItem('benchmark_results',  JSON.stringify(_data));
        }

        var confidence = ((dashboard.scoreLowerBound / score - 1) * 100).toFixed(2) +
            "% / +" + ((dashboard.scoreUpperBound / score - 1) * 100).toFixed(2) + "%";
        sectionsManager.setSectionScore("results", score.toFixed(2), confidence);
        sectionsManager.populateTable("results-header", Headers.testName, dashboard);
        sectionsManager.populateTable("results-score", Headers.score, dashboard);
        sectionsManager.populateTable("results-data", Headers.details, dashboard);
        sectionsManager.showSection("results", true);

        suitesManager.updateLocalStorageFromJSON(dashboard.results[0]);
    },

    showTestGraph: function(testName, testResult, testData)
    {
        sectionsManager.setSectionHeader("test-graph", testName);
        sectionsManager.showSection("test-graph", true);
        this.updateGraphData(testResult, testData, benchmarkRunnerClient.results.options);
    }
});
