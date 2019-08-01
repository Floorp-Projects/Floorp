// This is a helper for MathML feature detection.
// It is indented to be used to prevent false negative test results.

var MathMLFeatureDetection = {

    has_mspace: function() {
        // https://mathml-refresh.github.io/mathml-core/#space-mspace
        if (!this.hasOwnProperty("_has_mspace")) {
            document.body.insertAdjacentHTML("beforeend", "<math>\
<mspace></mspace>\
<mspace width='20px'></mspace>\
</math>");
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
        // https://mathml-refresh.github.io/mathml-core/#dfn-lspace
        // https://mathml-refresh.github.io/mathml-core/#layout-of-mrow
        if (!this.hasOwnProperty("_has_operator_spacing")) {
            document.body.insertAdjacentHTML("beforeend", "<math>\
<mrow>\
  <mn>1</mn><mo lspace='0px' rspace='0px'>+</mo><mn>2</mn>\
</mrow>\
<mrow>\
  <mn>1</mn><mo lspace='8px' rspace='8px'>+</mo><mn>2</mn>\
</mrow>\
</math>");
            var math = document.body.lastElementChild;
            var mrow = math.getElementsByTagName("mrow");
            // lspace/rspace will add 16px per MathML and none if not supported.
            this._has_operator_spacing =
                mrow[1].getBoundingClientRect().width -
                mrow[0].getBoundingClientRect().width > 10;
            document.body.removeChild(math);
        }
        return this._has_operator_spacing;
    },

    has_mfrac: function() {
        if (!this.hasOwnProperty("_has_mfrac")) {
            // Use tall enough fraction to avoid side effect of min num/denum shifts.
            document.body.insertAdjacentHTML("beforeend", "<math>\
<mfrac>\
  <mspace height='50px' depth='50px'></mspace>\
  <mspace height='50px' depth='50px'></mspace>\
</mfrac>\
<mfrac>\
  <mspace height='60px' depth='60px'></mspace>\
  <mspace height='60px' depth='60px'></mspace>\
</mfrac>\
</math>");
            var math = document.body.lastElementChild;
            var mfrac = math.getElementsByTagName("mfrac");
            // height/depth will add 40px per MathML, 20px if mfrac does not stack its children and none if mspace is not supported.
            this._has_mfrac =
                mfrac[1].getBoundingClientRect().height -
                mfrac[0].getBoundingClientRect().height > 30;
            document.body.removeChild(math);
        }
        return this._has_mfrac;
    },

    has_dir: function() {
        if (!this.hasOwnProperty("_has_dir")) {
            document.body.insertAdjacentHTML("beforeend", "<math style='direction: ltr !important;'>\
<mtext dir='rtl'></mtext>\
</math>");
            var math = document.body.lastElementChild;
            this._has_dir =
                window.getComputedStyle(math.firstElementChild).
                getPropertyValue('direction') === 'rtl';
            document.body.removeChild(math);
        }
        return this._has_dir;
    },

    ensure_for_match_reftest: function(has_function) {
        if (!document.querySelector("link[rel='match']"))
            throw "This function must only be used for match reftest";
        // Add a little red square at the top left corner if the feature is not supported in order to make match reftest fail.
        if (!this[has_function]()) {
            document.body.insertAdjacentHTML("beforeend", "\
<div style='width: 10px !important; height: 10px !important;\
            position: absolute !important;\
            left: 0 !important; top: 0 !important;\
            background: red !important; z-index: 1000 !important;'></div>");
        }
    }
};
