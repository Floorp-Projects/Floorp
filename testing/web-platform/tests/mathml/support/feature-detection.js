// This is a helper for MathML feature detection.
// It is indented to be used to prevent false negative test results.

var MathMLFeatureDetection = {
    has_mspace: function() {
        if (!this.hasOwnProperty("_has_mspace")) {
            document.body.insertAdjacentHTML("beforeend", "<math><mspace></mspace><mspace width='20px'></mspace></math>");
            var math = document.body.lastElementChild;
            // The width attribute will add 20px per MathML and none if not supported.
            this._has_mspace =
                math.lastChild.getBoundingClientRect().width -
                math.firstChild.getBoundingClientRect().width > 10;
            document.body.removeChild(math);
        }
        return this._has_mspace;
    },

    has_operator_spacing: function() {
        if (!this.hasOwnProperty("_has_operator_spacing")) {
            document.body.insertAdjacentHTML("beforeend", "<math><mrow><mn>1</mn><mo lspace='0px' rspace='0px'>+</mo><mn>2</mn><mo lspace='8px' rspace='8px'>+</mo><mn>3</mn></mspace></mrow></math>");
            var math = document.body.lastElementChild;
            var mo = math.getElementsByTagName("mo");
            // lspace/rspace will add 16px per MathML and none if not supported.
            this._has_operator_spacing =
                mo[1].getBoundingClientRect().width -
                mo[0].getBoundingClientRect().width > 10;
            document.body.removeChild(math);
        }
        return this._has_operator_spacing;
    }
};
