typedef union {
	IDL_tree tree;
	struct {
		IDL_tree tree;
		gpointer data;
	} treedata;
	GHashTable *hash_table;
	char *str;
	gboolean boolean;
	IDL_declspec_t declspec;
	IDL_longlong_t integer;
	double floatp;
	enum IDL_unaryop unaryop;
	enum IDL_param_attr paramattr;
} YYSTYPE;
#define	TOK_ANY	258
#define	TOK_ATTRIBUTE	259
#define	TOK_BOOLEAN	260
#define	TOK_CASE	261
#define	TOK_CHAR	262
#define	TOK_CONST	263
#define	TOK_CONTEXT	264
#define	TOK_DEFAULT	265
#define	TOK_DOUBLE	266
#define	TOK_ENUM	267
#define	TOK_EXCEPTION	268
#define	TOK_FALSE	269
#define	TOK_FIXED	270
#define	TOK_FLOAT	271
#define	TOK_IN	272
#define	TOK_INOUT	273
#define	TOK_INTERFACE	274
#define	TOK_LONG	275
#define	TOK_MODULE	276
#define	TOK_NATIVE	277
#define	TOK_NOSCRIPT	278
#define	TOK_OBJECT	279
#define	TOK_OCTET	280
#define	TOK_ONEWAY	281
#define	TOK_OP_SCOPE	282
#define	TOK_OP_SHL	283
#define	TOK_OP_SHR	284
#define	TOK_OUT	285
#define	TOK_RAISES	286
#define	TOK_READONLY	287
#define	TOK_SEQUENCE	288
#define	TOK_SHORT	289
#define	TOK_STRING	290
#define	TOK_STRUCT	291
#define	TOK_SWITCH	292
#define	TOK_TRUE	293
#define	TOK_TYPECODE	294
#define	TOK_TYPEDEF	295
#define	TOK_UNION	296
#define	TOK_UNSIGNED	297
#define	TOK_VARARGS	298
#define	TOK_VOID	299
#define	TOK_WCHAR	300
#define	TOK_WSTRING	301
#define	TOK_FLOATP	302
#define	TOK_INTEGER	303
#define	TOK_DECLSPEC	304
#define	TOK_PROP_KEY	305
#define	TOK_PROP_VALUE	306
#define	TOK_NATIVE_TYPE	307
#define	TOK_IDENT	308
#define	TOK_SQSTRING	309
#define	TOK_DQSTRING	310
#define	TOK_FIXEDP	311
#define	TOK_CODEFRAG	312


extern YYSTYPE yylval;
