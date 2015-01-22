addEventListener("message", Test, true);

function Test(evt)
{
    var data;
    
    if (evt.data == "removeEventListener")
    {
        removeEventListener("message", Test, true);
        data = "ping";
    }
    else if (evt.data == "TestEventTarget")
    {
        data = [this.toString(), evt.target.toString()];
    }
    else
    {
        data = evt.data;
    }

    postMessage(data);
}