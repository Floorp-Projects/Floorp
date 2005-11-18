function tc_init() {
    var divs = document.getElementsByClassName("section-content");
    allStretch = new fx.MultiFadeSize(divs, {duration: 400});

    for (i = 0; i < divs.length; i++) {
	allStretch.hide(divs[i], 'height');
    }

    testConfigHeight = new fx.Height('testconfig', {duration: 400});
    testConfigHeight.toggle();
    divs[0].fs.toggle('height');
}

function confirmPartialSubmission() {
    msg = "Did you intend to only submit a single test result? (Hint: There is a 'Submit All Results' button at the bottom of the page.)";
   return confirm(msg);
}

