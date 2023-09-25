import { LLParse, source } from 'llparse';

import Match = source.node.Match;
import Node = source.node.Node;

import {
  ALPHA,
  CharList,
  ERROR,
  HTTPMode,
  STRICT_URL_CHAR,
  URL_CHAR,
  USERINFO_CHARS,
} from './constants';

type SpanName = 'schema' | 'host' | 'path' | 'query' | 'fragment' | 'url';

export interface IURLResult {
  readonly entry: {
    readonly normal: Node;
    readonly connect: Node;
  };
  readonly exit: {
    readonly toHTTP: Node;
    readonly toHTTP09: Node;
  };
}

type SpanTable = Map<SpanName, source.Span>;

export class URL {
  private readonly span: source.Span | undefined;
  private readonly spanTable: SpanTable = new Map();
  private readonly errorInvalid: Node;
  private readonly errorStrictInvalid: Node;
  private readonly URL_CHAR: CharList;

  constructor(private readonly llparse: LLParse,
              private readonly mode: HTTPMode = 'loose',
              separateSpans: boolean = false) {
    const p = this.llparse;

    this.errorInvalid = p.error(ERROR.INVALID_URL, 'Invalid characters in url');
    this.errorStrictInvalid =
      p.error(ERROR.INVALID_URL, 'Invalid characters in url (strict mode)');

    this.URL_CHAR = mode === 'strict' ? STRICT_URL_CHAR : URL_CHAR;

    const table = this.spanTable;
    if (separateSpans) {
      table.set('schema', p.span(p.code.span('llhttp__on_url_schema')));
      table.set('host', p.span(p.code.span('llhttp__on_url_host')));
      table.set('path', p.span(p.code.span('llhttp__on_url_path')));
      table.set('query', p.span(p.code.span('llhttp__on_url_query')));
      table.set('fragment',
        p.span(p.code.span('llhttp__on_url_fragment')));
    } else {
      table.set('url', p.span(p.code.span('llhttp__on_url')));
    }
  }

  public build(): IURLResult {
    const p = this.llparse;

    const entry = {
      connect: this.node('entry_connect'),
      normal: this.node('entry_normal'),
    };

    const start = this.node('start');
    const path = this.node('path');
    const queryOrFragment = this.node('query_or_fragment');
    const schema = this.node('schema');
    const schemaDelim = this.node('schema_delim');
    const server = this.node('server');
    const queryStart = this.node('query_start');
    const query = this.node('query');
    const fragment = this.node('fragment');
    const serverWithAt = this.node('server_with_at');

    entry.normal
      .otherwise(this.spanStart('url', start));

    entry.connect
      .otherwise(this.spanStart('url', this.spanStart('host', server)));

    start
      .peek([ '/', '*' ], this.spanStart('path').skipTo(path))
      .peek(ALPHA, this.spanStart('schema', schema))
      .otherwise(p.error(ERROR.INVALID_URL, 'Unexpected start char in url'));

    schema
      .match(ALPHA, schema)
      .peek(':', this.spanEnd('schema').skipTo(schemaDelim))
      .otherwise(p.error(ERROR.INVALID_URL, 'Unexpected char in url schema'));

    schemaDelim
      .match('//', this.spanStart('host', server))
      .otherwise(p.error(ERROR.INVALID_URL, 'Unexpected char in url schema'));

    for (const node of [server, serverWithAt]) {
      node
        .peek('/', this.spanEnd('host', this.spanStart('path').skipTo(path)))
        .match('?', this.spanEnd('host', this.spanStart('query', query)))
        .match(USERINFO_CHARS, server)
        .match([ '[', ']' ], server)
        .otherwise(p.error(ERROR.INVALID_URL, 'Unexpected char in url server'));

      if (node !== serverWithAt) {
        node.match('@', serverWithAt);
      }
    }

    serverWithAt
      .match('@', p.error(ERROR.INVALID_URL, 'Double @ in url'));

    path
      .match(this.URL_CHAR, path)
      .otherwise(this.spanEnd('path', queryOrFragment));

    // Performance optimization, split `path` so that the fast case remains
    // there
    queryOrFragment
      .match('?', this.spanStart('query', query))
      .match('#', this.spanStart('fragment', fragment))
      .otherwise(p.error(ERROR.INVALID_URL, 'Invalid char in url path'));

    query
      .match(this.URL_CHAR, query)
      // Allow extra '?' in query string
      .match('?', query)
      .peek('#', this.spanEnd('query')
        .skipTo(this.spanStart('fragment', fragment)))
      .otherwise(p.error(ERROR.INVALID_URL, 'Invalid char in url query'));

    fragment
      .match(this.URL_CHAR, fragment)
      .match([ '?', '#' ], fragment)
      .otherwise(
        p.error(ERROR.INVALID_URL, 'Invalid char in url fragment start'));

    for (const node of [ start, schema, schemaDelim ]) {
      /* No whitespace allowed here */
      node.match([ ' ', '\r', '\n' ], this.errorInvalid);
    }

    // Adaptors
    const toHTTP = this.node('to_http');
    const toHTTP09 = this.node('to_http_09');

    const skipToHTTP = this.node('skip_to_http')
      .skipTo(toHTTP);

    const skipToHTTP09 = this.node('skip_to_http09')
      .skipTo(toHTTP09);

    const skipCRLF = this.node('skip_lf_to_http09')
      .match('\r\n', toHTTP09)
      .otherwise(p.error(ERROR.INVALID_URL, 'Expected CRLF'));

    for (const node of [server, serverWithAt, queryOrFragment, queryStart, query, fragment]) {
      let spanName: SpanName | undefined;

      if (node === server || node === serverWithAt) {
        spanName = 'host';
      } else if (node === queryStart || node === query) {
        spanName = 'query';
      } else if (node === fragment) {
        spanName = 'fragment';
      }

      const endTo = (target: Node): Node => {
        let res: Node = this.spanEnd('url', target);
        if (spanName !== undefined) {
          res = this.spanEnd(spanName, res);
        }
        return res;
      };

      node.peek(' ', endTo(skipToHTTP));

      node.peek('\r', endTo(skipCRLF));
      node.peek('\n', endTo(skipToHTTP09));
    }

    return {
      entry,
      exit: {
        toHTTP,
        toHTTP09,
      },
    };
  }

  private spanStart(name: SpanName, otherwise?: Node): Node {
    let res: Node;
    if (this.spanTable.has(name)) {
      res = this.spanTable.get(name)!.start();
    } else {
      res = this.llparse.node('span_start_stub_' + name);
    }
    if (otherwise !== undefined) {
      res.otherwise(otherwise);
    }
    return res;
  }

  private spanEnd(name: SpanName, otherwise?: Node): Node {
    let res: Node;
    if (this.spanTable.has(name)) {
      res = this.spanTable.get(name)!.end();
    } else {
      res = this.llparse.node('span_end_stub_' + name);
    }
    if (otherwise !== undefined) {
      res.otherwise(otherwise);
    }
    return res;
  }

  private node(name: string): Match {
    const res = this.llparse.node('url_' + name);

    if (this.mode === 'strict') {
      res.match([ '\t', '\f' ], this.errorStrictInvalid);
    }

    return res;
  }
}
