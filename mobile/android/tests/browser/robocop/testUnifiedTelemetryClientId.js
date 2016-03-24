var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/ClientID.jsm');

var java = new JavaBridge(this);
do_register_cleanup(() => {
    java.disconnect();
});
do_test_pending();

var isClientIDSet;
var clientID;

var isResetDone;

function getAsyncClientId() {
    isClientIDSet = false;
    ClientID.getClientID().then(function (retClientID) {
        // Ideally, we'd directly send the client ID back to Java but Java won't listen for
        // js messages after we return from the containing function (bug 1253467).
        //
        // Note that my brief attempts to get synchronous Promise resolution (via Task.jsm)
        // working failed - I have other things to focus on.
        clientID = retClientID;
        isClientIDSet = true;
    }, function (fail) {
        // Since Java doesn't listen to our messages (bug 1253467), I don't expect
        // this throw to work correctly but we should timeout in Java.
        do_throw('Could not retrieve client ID: ' + fail);
    });
}

function pollGetAsyncClientId() {
    java.asyncCall('blockingFromJsResponseString', isClientIDSet, clientID);
}

function getAsyncReset() {
    isResetDone = false;
    ClientID._reset().then(function () {
        isResetDone = true;
    });
}

function pollGetAsyncReset() {
    java.asyncCall('blockingFromJsResponseString', isResetDone, '');
}

function endTest() {
    do_test_finished();
}
