const nsILocalFile = Components.interfaces.nsILocalFile;
var prefix = "";

function ls(path, recur)
{
    var file = Components.classes["@mozilla.org/file/local;1"].
	createInstance(nsILocalFile);
    try {
        file.initWithPath( path );
    
        if (file.isDirectory() && arguments.length == 1)
	    ls_dir(file, recur);
        else
	    ls_file(file, recur);
    }
    catch (e) {
        dump("Error Returned " + e + "\n");
    }
}
function ls_file(file, recur)
{
    dump(prefix);

    try {
        if (file.isDirectory()) {
            dump("directory " + file.leafName + "\n");
	        if(recur)
	            ls_dir(file, true);
	        return;
        }

        dump(file.leafName + " " + file.fileSize);
        if (file.isSymlink())
	        dump(" -> " + file.target);
        dump("\n");
    }

    catch (e) {
        dump(file.leafName + " (error accessing)\n");
    }
}

function ls_dir(file, recur)
{
    var leafName = file.leafName;

    var old = prefix;
    prefix = prefix + "    ";

    iter = file.directoryEntries;
    dump(iter + "\n");

    foreach_iter(iter,
		 function (file) { ls_file(file, recur); });
    prefix = old;
}

function foreach_iter(iter, fun)
{
    while (iter.hasMoreElements()) {
	var item = iter.getNext().QueryInterface(nsILocalFile);
	fun(item);
    }
}
