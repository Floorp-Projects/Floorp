function function_ref(fn) {
    return A({"href": fn[1], "class": "mochiref reference"}, fn[0], BR());
};

function toggle_docs() {
    toggleElementClass("invisible", "show_index", "function_index");
    return false;
};

function create_toc() {
    if (getElement("distribution")) {
        return global_index();
    } 
    if (getElement("api-reference")) {
        return module_index();
    }
};

function doXHTMLRequest(url) {
    var req = getXMLHttpRequest();
    if (req.overrideMimeType) {
        req.overrideMimeType("text/xml");
    }
    req.open("GET", url, true);
    return sendXMLHttpRequest(req).addCallback(function (res) {
        return res.responseXML.documentElement;
    });
};

function load_request(href, div, doc) {
    var functions = withDocument(doc, spider_doc);
    forEach(functions, function (func) {
        // fix anchors
        if (func[1].charAt(0) == "#") {
            func[1] = href + func[1];
        }
    });
    var showLink = A({"class": "force-pointer"}, "[+]");
    var hideLink = A({"class": "force-pointer"}, "[\u2013]");
    var functionIndex = DIV({"id": "function_index", "class": "invisible"},
        hideLink,
        P(null, map(function_ref, functions))
    );
    var toggleFunc = function (e) {
        toggleElementClass("invisible", showLink, functionIndex);
    };
    connect(showLink, "onclick", toggleFunc);
    connect(hideLink, "onclick", toggleFunc);
    replaceChildNodes(div,
        showLink,
        functionIndex
    );
};

function global_index() {
    var distList = getElementsByTagAndClassName("ul")[0];
    var bullets = getElementsByTagAndClassName("li", null, distList);
    for (var i = 0; i < bullets.length; i++) {
        var tag = bullets[i];
        var firstLink = getElementsByTagAndClassName("a", "mochiref", tag)[0];
        var href = getNodeAttribute(firstLink, "href");
        var div = DIV(null, "[\u2026]");
        appendChildNodes(tag, BR(), div);
        var d = doXHTMLRequest(href).addCallback(load_request, href, div);
    }
};

function spider_doc() {
    return map(
        function (tag) {
            return [scrapeText(tag), getNodeAttribute(tag, "href")];
        },
        getElementsByTagAndClassName("a", "mochidef")
    );
};

function module_index() {
    var sections = getElementsByTagAndClassName("div", "section");
    var ptr = sections[1];
    var ref = DIV({"class": "section"},
        H1(null, "Function Index"),
        A({"id": "show_index", "href": "#", "onclick": toggle_docs}, "[show]"),
        DIV({"id": "function_index", "class": "invisible"},
            A({"href":"#", "onclick": toggle_docs}, "[hide]"),
            P(null, map(function_ref, spider_doc()))));
    ptr.parentNode.insertBefore(ref, ptr);
};

addLoadEvent(create_toc);
