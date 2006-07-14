function tc_init() {
    var divs = document.getElementsByClassName("testcase-content");
    allStretch = new fx.MultiFadeSize(divs, {height: true, opacity: true, duration: 400});

    allStretch.hideAll();

    testConfigHeight = new fx.Height('testconfig', {duration: 400});
    testConfigHeight.toggle();

    allStretch.fxa[0].toggle();
}

function confirmPartialSubmission() {
    msg = "Did you intend to only submit a single test result? (Hint: There is a 'Submit All Results' button at the bottom of the page.)";
   return confirm(msg);
}
