dp.sh.Brushes.Xml = function()
{
	this.CssClass = 'dp-xml';
}

dp.sh.Brushes.Xml.prototype	= new dp.sh.Highlighter();
dp.sh.Brushes.Xml.Aliases	= ['xml', 'xhtml', 'xslt', 'html', 'xhtml'];

dp.sh.Brushes.Xml.prototype.ProcessRegexList = function()
{
	function push(array, value)
	{
		array[array.length] = value;
	}
	
	/* If only there was a way to get index of a group within a match, the whole XML
	   could be matched with the expression looking something like that:
	
	   (<!\[CDATA\[\s*.*\s*\]\]>)
	   | (<!--\s*.*\s*?-->)
	   | (<)*(\w+)*\s*(\w+)\s*=\s*(".*?"|'.*?'|\w+)(/*>)*
	   | (</?)(.*?)(/?>)
	*/
	var index	= 0;
	var match	= null;
	var regex	= null;

	// Match CDATA in the following format <![ ... [ ... ]]>
	// <\!\[[\w\s]*?\[(.|\s)*?\]\]>
	this.GetMatches(new RegExp('<\\!\\[[\\w\\s]*?\\[(.|\\s)*?\\]\\]>', 'gm'), 'cdata');
	
	// Match comments
	// <!--\s*.*\s*?-->
	this.GetMatches(new RegExp('<!--\\s*.*\\s*?-->', 'gm'), 'comments');

	// Match attributes and their values
	// (\w+)\s*=\s*(".*?"|\'.*?\'|\w+)*
	regex = new RegExp('([\\w-\.]+)\\s*=\\s*(".*?"|\'.*?\'|\\w+)*', 'gm');
	while((match = regex.exec(this.code)) != null)
	{
		push(this.matches, new dp.sh.Match(match[1], match.index, 'attribute'));
	
		// if xml is invalid and attribute has no property value, ignore it	
		if(match[2] != undefined)
		{
			push(this.matches, new dp.sh.Match(match[2], match.index + match[0].indexOf(match[2]), 'attribute-value'));
		}
	}

	// Match opening and closing tag brackets
	// </*\?*(?!\!)|/*\?*>
	this.GetMatches(new RegExp('</*\\?*(?!\\!)|/*\\?*>', 'gm'), 'tag');

	// Match tag names
	// </*\?*\s*(\w+)
	regex = new RegExp('</*\\?*\\s*([\\w-\.]+)', 'gm');
	while((match = regex.exec(this.code)) != null)
	{
		push(this.matches, new dp.sh.Match(match[1], match.index + match[0].indexOf(match[1]), 'tag-name'));
	}
}
