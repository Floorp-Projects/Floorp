dp.sh.Brushes.JScript = function()
{
	var keywords =	'abstract boolean break byte case catch char class const continue debugger ' +
					'default delete do double else enum export extends false final finally float ' +
					'for function goto if implements import in instanceof int interface long native ' +
					'new null package private protected public return short static super switch ' +
					'synchronized this throw throws transient true try typeof var void volatile while with';

	this.regexList = [
		{ regex: new RegExp('//.*$', 'gm'),							css: 'comment' },			// one line comments
		{ regex: new RegExp('/\\*[\\s\\S]*?\\*/', 'g'),				css: 'comment' },			// multiline comments
		{ regex: new RegExp('"(?:[^"\n]|[\"])*?"', 'g'),			css: 'string' },			// double quoted strings
		{ regex: new RegExp("'(?:[^'\n]|[\'])*?'", 'g'),			css: 'string' },			// single quoted strings
		{ regex: new RegExp('^\\s*#.*', 'gm'),						css: 'preprocessor' },		// preprocessor tags like #region and #endregion
		{ regex: new RegExp(this.GetKeywords(keywords), 'gm'),		css: 'keyword' }			// keywords
		];

	this.CssClass = 'dp-c';
}

dp.sh.Brushes.JScript.prototype	= new dp.sh.Highlighter();
dp.sh.Brushes.JScript.Aliases	= ['js', 'jscript', 'javascript'];
