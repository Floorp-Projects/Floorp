
/**
 * Stores a pointer to the ops struct, and the offset: the place to
 * write the parsed result in the destination structure.
 */
struct cmdline_token_hdr {
	struct cmdline_token_ops *ops;
	unsigned int offset;
};
typedef struct cmdline_token_hdr cmdline_parse_token_hdr_t;

/**
 * A token is defined by this structure.
 *
 * parse() takes the token as first argument, then the source buffer
 * starting at the token we want to parse. The 3rd arg is a pointer
 * where we store the parsed data (as binary). It returns the number of
 * parsed chars on success and a negative value on error.
 *
 * complete_get_nb() returns the number of possible values for this
 * token if completion is possible. If it is NULL or if it returns 0,
 * no completion is possible.
 *
 * complete_get_elt() copy in dstbuf (the size is specified in the
 * parameter) the i-th possible completion for this token.  returns 0
 * on success or and a negative value on error.
 *
 * get_help() fills the dstbuf with the help for the token. It returns
 * -1 on error and 0 on success.
 */
struct cmdline_token_ops {
	/** parse(token ptr, buf, res pts, buf len) */
	int (*parse)(cmdline_parse_token_hdr_t *, const char *, void *,
		unsigned int);
	/** return the num of possible choices for this token */
	int (*complete_get_nb)(cmdline_parse_token_hdr_t *);
	/** return the elt x for this token (token, idx, dstbuf, size) */
	int (*complete_get_elt)(cmdline_parse_token_hdr_t *, int, char *,
		unsigned int);
	/** get help for this token (token, dstbuf, size) */
	int (*get_help)(cmdline_parse_token_hdr_t *, char *, unsigned int);
};

enum cmdline_numtype {
	UINT8 = 0,
	UINT16,
	UINT32,
	UINT64,
	INT8,
	INT16,
	INT32,
	INT64
};

struct cmdline_token_num_data {
	enum cmdline_numtype type;
};

struct cmdline_token_num {
	struct cmdline_token_hdr hdr;
	struct cmdline_token_num_data num_data;
};
typedef struct cmdline_token_num cmdline_parse_token_num_t;