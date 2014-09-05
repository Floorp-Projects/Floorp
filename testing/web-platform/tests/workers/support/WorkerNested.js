var result = "Fail";

try
{
    var worker1 = new Worker("WorkerBasic.js");

    worker1.postMessage("ping");

    worker1.onmessage = function(evt)
    {
        result = evt.data; 
        self.postMessage(result);
        worker1.terminate();
    }
}
catch(ex)
{
    postMessage(result);
}