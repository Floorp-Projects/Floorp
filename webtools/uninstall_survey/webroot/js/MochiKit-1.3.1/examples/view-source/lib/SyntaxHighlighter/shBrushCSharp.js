dp.sh.Brushes.CSharp = function()
{
	var keywords =	'abstract as base bool break byte case catch char checked class const ' +
					'continue decimal default delegate do double else enum event explicit ' +
					'extern false finally fixed float for foreach get goto if implicit in int ' +
					'interface internal is lock long namespace new null object operator out ' +
					'override params private protected public readonly ref return sbyte sealed set ' +
					'short sizeof stackalloc static string struct switch this throw true try ' +
					'typeof uint ulong unchecked unsafe ushort using virtual void while';

	this.regexList = [
		// There's a slight problem with matching single line comments and figuring out
		// a difference between // and ///. Using lookahead and lookbehind solves the
		// problem, unfortunately JavaScript doesn't support lookbehind. So I'm at a 
		// loss how to translate that regular expression to JavaScript compatible one.
//		{ regex: new RegExp('(?<!/)//(?!/).*$|(?<!/)////(?!/).*$|/\\*[^\\*]*(.)*?\\*/', 'gm'),	css: 'comment' },			// one line comments starting with anything BUT '///' and multiline comments
//		{ regex: new RegExp('(?<!/)///(?!/).*$', 'gm'),											css: 'comments' },		// XML comments starting with ///

		{ regex: new RegExp('//.*$', 'gm'),							css: 'comment' },			// one line comments
		{ regex: new RegExp('/\\*[\\s\\S]*?\\*/', 'g'),				css: 'comment' },			// multiline comments
		{ regex: new RegExp('"(?:\\.|[^\\""])*"', 'g'),				css: 'string' },			// strings
		{ regex: new RegExp('^\\s*#.*', 'gm'),						css: 'preprocessor' },		// preprocessor tags like #region and #endregion
		{ regex: new RegExp(this.GetKeywords(keywords), 'gm'),		css: 'keyword' }			// c# keyword
		];

	this.CssClass = 'dp-c';
}

dp.sh.Brushes.CSharp.prototype	= new dp.sh.Highlighter();
dp.sh.Brushes.CSharp.Aliases	= ['c#', 'c-sharp', 'csharp'];
