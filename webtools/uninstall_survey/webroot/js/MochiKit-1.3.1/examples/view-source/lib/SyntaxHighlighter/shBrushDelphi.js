/* Delphi brush is contributed by Eddie Shipman */
dp.sh.Brushes.Delphi = function()
{
	var keywords =	'abs addr and ansichar ansistring array as asm begin boolean byte cardinal ' +
					'case char class comp const constructor currency destructor div do double ' +
					'downto else end except exports extended false file finalization finally ' +
					'for function goto if implementation in inherited int64 initialization ' +
					'integer interface is label library longint longword mod nil not object ' +
					'of on or packed pansichar pansistring pchar pcurrency pdatetime pextended ' + 
					'pint64 pointer private procedure program property pshortstring pstring ' + 
					'pvariant pwidechar pwidestring protected public published raise real real48 ' +
					'record repeat set shl shortint shortstring shr single smallint string then ' +
					'threadvar to true try type unit until uses val var varirnt while widechar ' +
					'widestring with word write writeln xor';

	this.regexList = [
		{ regex: new RegExp('\\(\\*[\\s\\S]*?\\*\\)', 'gm'),		css: 'comment' },  			// multiline comments (* *)
		{ regex: new RegExp('{(?!\\$)[\\s\\S]*?}', 'gm'),			css: 'comment' },  			// multiline comments { }
		{ regex: new RegExp('//.*$', 'gm'),							css: 'comment' },  			// one line
		{ regex: new RegExp('\'(?:\\.|[^\\\'\'])*\'', 'g'),			css: 'string' },			// strings
		{ regex: new RegExp('\\{\\$[a-zA-Z]+ .+\\}', 'g'),			css: 'directive' },			// Compiler Directives and Region tags
		{ regex: new RegExp('\\b[\\d\\.]+\\b', 'g'),				css: 'number' },			// numbers 12345
		{ regex: new RegExp('\\$[a-zA-Z0-9]+\\b', 'g'),				css: 'number' },			// numbers $F5D3
		{ regex: new RegExp(this.GetKeywords(keywords), 'gm'),		css: 'keyword' }			// keyword
		];

	this.CssClass = 'dp-delphi';
}

dp.sh.Brushes.Delphi.prototype	= new dp.sh.Highlighter();
dp.sh.Brushes.Delphi.Aliases	= ['delphi', 'pascal'];
