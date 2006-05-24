/* Python 2.3 syntax contributed by Gheorghe Milas */
dp.sh.Brushes.Python = function()
{
	var keywords =		'and assert break class continue def del elif else except exec ' +
						'finally for from global if import in is lambda not or object pass print ' +
						'raise return try yield while';
	
	var builtins =		'self __builtin__ __dict__ __future__ __methods__ __members__ __author__ __email__ __version__' +
						'__class__ __bases__ __import__ __main__ __name__ __doc__ __self__ __debug__ __slots__ ' +
						'abs append apply basestring bool buffer callable chr classmethod clear close cmp coerce compile complex ' +
						'conjugate copy count delattr dict dir divmod enumerate Ellipsis eval execfile extend False file fileno filter float flush ' +
						'get getattr globals has_key hasarttr hash hex id index input insert int intern isatty isinstance isubclass ' +
						'items iter keys len list locals long map max min mode oct open ord pop pow property range ' +
						'raw_input read readline readlines reduce reload remove repr reverse round seek setattr slice sum ' +
						'staticmethod str super tell True truncate tuple type unichr unicode update values write writelines xrange zip';
	
	var magicmethods =	'__abs__ __add__ __and__ __call__ __cmp__ __coerce__ __complex__ __concat__ __contains__ __del__ __delattr__ __delitem__ ' +
						'__delslice__ __div__ __divmod__ __float__ __getattr__ __getitem__ __getslice__ __hash__ __hex__ __eq__ __le__ __lt__ __gt__ __ge__ ' +
						'__iadd__ __isub__ __imod__ __idiv__ __ipow__ __iand__ __ior__ __ixor__ __ilshift__ __irshift__ ' +
						'__invert__ __init__ __int__ __inv__ __iter__ __len__ __long__ __lshift__ __mod__ __mul__ __new__ __neg__ __nonzero__ __oct__ __or__ ' +
						'__pos__ __pow__ __radd__ __rand__ __rcmp__ __rdiv__ __rdivmod__ __repeat__ __repr__ __rlshift__ __rmod__ __rmul__ ' +
						'__ror__ __rpow__ __rrshift__ __rshift__ __rsub__ __rxor__ __setattr__ __setitem__ __setslice__ __str__ __sub__ __xor__';
	
	var exceptions =	'Exception StandardError ArithmeticError LookupError EnvironmentError AssertionError AttributeError EOFError ' +
						'FutureWarning IndentationError OverflowWarning PendingDeprecationWarning ReferenceError RuntimeWarning ' +
						'SyntaxWarning TabError UnicodeDecodeError UnicodeEncodeError UnicodeTranslateError UserWarning Warning ' +
						'IOError ImportError IndexError KeyError KeyboardInterrupt MemoryError NameError NotImplementedError OSError ' +
						'RuntimeError StopIteration SyntaxError SystemError SystemExit TypeError UnboundLocalError UnicodeError ValueError ' +
						'FloatingPointError OverflowError WindowsError ZeroDivisionError';
	
	var types =			'NoneType TypeType IntType LongType FloatType ComplexType StringType UnicodeType BufferType TupleType ListType ' +
						'DictType FunctionType LambdaType CodeType ClassType UnboundMethodType InstanceType MethodType BuiltinFunctionType BuiltinMethodType ' +
						'ModuleType FileType XRangeType TracebackType FrameType SliceType EllipsisType';
	
	var commonlibs =	'anydbm array asynchat asyncore AST base64 binascii binhex bisect bsddb buildtools bz2 ' +
						'BaseHTTPServer Bastion calendar cgi cmath cmd codecs codeop commands compiler copy copy_reg ' +
						'cPickle crypt cStringIO csv curses Carbon CGIHTTPServer ConfigParser Cookie datetime dbhash ' +
						'dbm difflib dircache distutils doctest DocXMLRPCServer email encodings errno exceptions fcntl ' +
						'filecmp fileinput ftplib gc gdbm getopt getpass glob gopherlib gzip heapq htmlentitydefs ' +
						'htmllib httplib HTMLParser imageop imaplib imgfile imghdr imp inspect itertools jpeg keyword ' +
						'linecache locale logging mailbox mailcap marshal math md5 mhlib mimetools mimetypes mimify mmap ' +
						'mpz multifile mutex MimeWriter netrc new nis nntplib nsremote operator optparse os parser pickle pipes ' +
						'popen2 poplib posix posixfile pprint preferences profile pstats pwd pydoc pythonprefs quietconsole ' +
						'quopri Queue random re readline resource rexec rfc822 rgbimg sched select sets sgmllib sha shelve shutil ' +
						'signal site smtplib socket stat statcache string struct symbol sys syslog SimpleHTTPServer ' +
						'SimpleXMLRPCServer SocketServer StringIO tabnanny tarfile telnetlib tempfile termios textwrap ' +
						'thread threading time timeit token tokenize traceback tty types Tkinter unicodedata unittest ' +
						'urllib urllib2 urlparse user UserDict UserList UserString warnings weakref webbrowser whichdb ' +
						'xml xmllib xmlrpclib xreadlines zipfile zlib';

	this.regexList = [
		{ regex: new RegExp('#.*$', 'gm'),								css: 'comment' },			// comments
		{ regex: new RegExp('^\\s*"""(.|\n)*?"""\\s*$', 'gm'),			css: 'docstring' },			// documentation string "
		{ regex: new RegExp('^\\s*\'\'\'(.|\n)*?\'\'\'\\s*$', 'gm'),	css: 'docstring' },			// documentation string '
		{ regex: new RegExp('"""(.|\n)*?"""', 'g'),						css: 'string' },			// multi-line strings "
		{ regex: new RegExp('\'\'\'(.|\n)*?\'\'\'', 'g'),				css: 'string' },			// multi-line strings '
		{ regex: new RegExp('"(?:\\.|[^\\""])*"', 'g'),					css: 'string' },			// strings "
		{ regex: new RegExp('\'(?:\\.|[^\\\'\'])*\'', 'g'),				css: 'string' },			// strings '
		{ regex: new RegExp(this.GetKeywords(keywords), 'gm'),			css: 'keyword' },			// keywords
		{ regex: new RegExp(this.GetKeywords(builtins), 'gm'),			css: 'builtins' },			// builtin objects, functions, methods, magic attributes
		{ regex: new RegExp(this.GetKeywords(magicmethods), 'gm'),		css: 'magicmethods' },		// special methods
		{ regex: new RegExp(this.GetKeywords(exceptions), 'gm'),		css: 'exceptions' },		// standard exception classes
		{ regex: new RegExp(this.GetKeywords(types), 'gm'),				css: 'types' },				// types from types.py
		{ regex: new RegExp(this.GetKeywords(commonlibs), 'gm'),		css: 'commonlibs' }			// common standard library modules
		];

	this.CssClass = 'dp-py';
}

dp.sh.Brushes.Python.prototype	= new dp.sh.Highlighter();
dp.sh.Brushes.Python.Aliases	= ['py', 'python'];
