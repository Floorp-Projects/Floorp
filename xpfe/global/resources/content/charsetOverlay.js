function SetDefaultCharacterSet(node)
{
	dump("Charset Overlay menu item pressed.\n");
    var charset = node.getAttribute('id');
	dump(charset);
	dump("\n");
    BrowserSetDefaultCharacterSet(charset);
}
