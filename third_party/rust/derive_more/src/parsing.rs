use self::RuleResult::{Failed, Matched};
fn escape_default(s: &str) -> String {
    s.chars().flat_map(|c| c.escape_default()).collect()
}
fn char_range_at(s: &str, pos: usize) -> (char, usize) {
    let c = &s[pos..].chars().next().unwrap();
    let next_pos = pos + c.len_utf8();
    (*c, next_pos)
}
#[derive(Clone)]
enum RuleResult<T> {
    Matched(usize, T),
    Failed,
}
#[derive(PartialEq, Eq, Debug, Clone)]
pub struct ParseError {
    pub line: usize,
    pub column: usize,
    pub offset: usize,
    pub expected: ::std::collections::HashSet<&'static str>,
}
pub type ParseResult<T> = Result<T, ParseError>;
impl ::std::fmt::Display for ParseError {
    fn fmt(
        &self,
        fmt: &mut ::std::fmt::Formatter,
    ) -> ::std::result::Result<(), ::std::fmt::Error> {
        write!(fmt, "error at {}:{}: expected ", self.line, self.column)?;
        if self.expected.len() == 0 {
            write!(fmt, "EOF")?;
        } else if self.expected.len() == 1 {
            write!(
                fmt,
                "`{}`",
                escape_default(self.expected.iter().next().unwrap())
            )?;
        } else {
            let mut iter = self.expected.iter();
            write!(fmt, "one of `{}`", escape_default(iter.next().unwrap()))?;
            for elem in iter {
                write!(fmt, ", `{}`", escape_default(elem))?;
            }
        }
        Ok(())
    }
}
impl ::std::error::Error for ParseError {
    fn description(&self) -> &str {
        "parse error"
    }
}
fn slice_eq(
    input: &str,
    state: &mut ParseState,
    pos: usize,
    m: &'static str,
) -> RuleResult<()> {
    #![inline]
    #![allow(dead_code)]
    let l = m.len();
    if input.len() >= pos + l && &input.as_bytes()[pos..pos + l] == m.as_bytes() {
        Matched(pos + l, ())
    } else {
        state.mark_failure(pos, m)
    }
}
fn slice_eq_case_insensitive(
    input: &str,
    state: &mut ParseState,
    pos: usize,
    m: &'static str,
) -> RuleResult<()> {
    #![inline]
    #![allow(dead_code)]
    let mut used = 0usize;
    let mut input_iter = input[pos..].chars().flat_map(|x| x.to_uppercase());
    for m_char_upper in m.chars().flat_map(|x| x.to_uppercase()) {
        used += m_char_upper.len_utf8();
        let input_char_result = input_iter.next();
        if input_char_result.is_none() || input_char_result.unwrap() != m_char_upper {
            return state.mark_failure(pos, m);
        }
    }
    Matched(pos + used, ())
}
fn any_char(input: &str, state: &mut ParseState, pos: usize) -> RuleResult<()> {
    #![inline]
    #![allow(dead_code)]
    if input.len() > pos {
        let (_, next) = char_range_at(input, pos);
        Matched(next, ())
    } else {
        state.mark_failure(pos, "<character>")
    }
}
fn pos_to_line(input: &str, pos: usize) -> (usize, usize) {
    let before = &input[..pos];
    let line = before.as_bytes().iter().filter(|&&c| c == b'\n').count() + 1;
    let col = before.chars().rev().take_while(|&c| c != '\n').count() + 1;
    (line, col)
}
impl<'input> ParseState<'input> {
    fn mark_failure(&mut self, pos: usize, expected: &'static str) -> RuleResult<()> {
        if self.suppress_fail == 0 {
            if pos > self.max_err_pos {
                self.max_err_pos = pos;
                self.expected.clear();
            }
            if pos == self.max_err_pos {
                self.expected.insert(expected);
            }
        }
        Failed
    }
}
struct ParseState<'input> {
    max_err_pos: usize,
    suppress_fail: usize,
    expected: ::std::collections::HashSet<&'static str>,
    _phantom: ::std::marker::PhantomData<&'input ()>,
}
impl<'input> ParseState<'input> {
    fn new() -> ParseState<'input> {
        ParseState {
            max_err_pos: 0,
            suppress_fail: 0,
            expected: ::std::collections::HashSet::new(),
            _phantom: ::std::marker::PhantomData,
        }
    }
}

fn __parse_discard_doubles<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<Option<&'input str>> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = {
            let __choice_res = {
                let __seq_res = slice_eq(__input, __state, __pos, "{");
                match __seq_res {
                    Matched(__pos, _) => slice_eq(__input, __state, __pos, "{"),
                    Failed => Failed,
                }
            };
            match __choice_res {
                Matched(__pos, __value) => Matched(__pos, __value),
                Failed => {
                    let __seq_res = slice_eq(__input, __state, __pos, "}");
                    match __seq_res {
                        Matched(__pos, _) => slice_eq(__input, __state, __pos, "}"),
                        Failed => Failed,
                    }
                }
            }
        };
        match __seq_res {
            Matched(__pos, _) => Matched(__pos, { None }),
            Failed => Failed,
        }
    }
}

fn __parse_placeholder_inner<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<Option<&'input str>> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = {
            let str_start = __pos;
            match {
                let __seq_res = if __input.len() > __pos {
                    let (__ch, __next) = char_range_at(__input, __pos);
                    match __ch {
                        '{' => Matched(__next, ()),
                        _ => __state.mark_failure(__pos, "[{]"),
                    }
                } else {
                    __state.mark_failure(__pos, "[{]")
                };
                match __seq_res {
                    Matched(__pos, _) => {
                        let __seq_res = {
                            let mut __repeat_pos = __pos;
                            loop {
                                let __pos = __repeat_pos;
                                let __step_res = {
                                    let __seq_res = {
                                        __state.suppress_fail += 1;
                                        let __assert_res = if __input.len() > __pos {
                                            let (__ch, __next) =
                                                char_range_at(__input, __pos);
                                            match __ch {
                                                '{' | '}' => Matched(__next, ()),
                                                _ => {
                                                    __state.mark_failure(__pos, "[{}]")
                                                }
                                            }
                                        } else {
                                            __state.mark_failure(__pos, "[{}]")
                                        };
                                        __state.suppress_fail -= 1;
                                        match __assert_res {
                                            Failed => Matched(__pos, ()),
                                            Matched(..) => Failed,
                                        }
                                    };
                                    match __seq_res {
                                        Matched(__pos, _) => {
                                            any_char(__input, __state, __pos)
                                        }
                                        Failed => Failed,
                                    }
                                };
                                match __step_res {
                                    Matched(__newpos, __value) => {
                                        __repeat_pos = __newpos;
                                    }
                                    Failed => {
                                        break;
                                    }
                                }
                            }
                            Matched(__repeat_pos, ())
                        };
                        match __seq_res {
                            Matched(__pos, _) => {
                                if __input.len() > __pos {
                                    let (__ch, __next) = char_range_at(__input, __pos);
                                    match __ch {
                                        '}' => Matched(__next, ()),
                                        _ => __state.mark_failure(__pos, "[}]"),
                                    }
                                } else {
                                    __state.mark_failure(__pos, "[}]")
                                }
                            }
                            Failed => Failed,
                        }
                    }
                    Failed => Failed,
                }
            } {
                Matched(__newpos, _) => {
                    Matched(__newpos, &__input[str_start..__newpos])
                }
                Failed => Failed,
            }
        };
        match __seq_res {
            Matched(__pos, n) => Matched(__pos, { Some(n) }),
            Failed => Failed,
        }
    }
}

fn __parse_discard_any<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<Option<&'input str>> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = any_char(__input, __state, __pos);
        match __seq_res {
            Matched(__pos, _) => Matched(__pos, { None }),
            Failed => Failed,
        }
    }
}

fn __parse_arg<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<usize> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = {
            let str_start = __pos;
            match {
                let mut __repeat_pos = __pos;
                let mut __repeat_value = vec![];
                loop {
                    let __pos = __repeat_pos;
                    let __step_res = if __input.len() > __pos {
                        let (__ch, __next) = char_range_at(__input, __pos);
                        match __ch {
                            '0'...'9' => Matched(__next, ()),
                            _ => __state.mark_failure(__pos, "[0-9]"),
                        }
                    } else {
                        __state.mark_failure(__pos, "[0-9]")
                    };
                    match __step_res {
                        Matched(__newpos, __value) => {
                            __repeat_pos = __newpos;
                            __repeat_value.push(__value);
                        }
                        Failed => {
                            break;
                        }
                    }
                }
                if __repeat_value.len() >= 1 {
                    Matched(__repeat_pos, ())
                } else {
                    Failed
                }
            } {
                Matched(__newpos, _) => {
                    Matched(__newpos, &__input[str_start..__newpos])
                }
                Failed => Failed,
            }
        };
        match __seq_res {
            Matched(__pos, n) => Matched(__pos, { n.parse().unwrap() }),
            Failed => Failed,
        }
    }
}

fn __parse_ty<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<&'input str> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = {
            let str_start = __pos;
            match {
                let __choice_res = {
                    let __choice_res = slice_eq(__input, __state, __pos, "x?");
                    match __choice_res {
                        Matched(__pos, __value) => Matched(__pos, __value),
                        Failed => slice_eq(__input, __state, __pos, "X?"),
                    }
                };
                match __choice_res {
                    Matched(__pos, __value) => Matched(__pos, __value),
                    Failed => {
                        let __choice_res = slice_eq(__input, __state, __pos, "o");
                        match __choice_res {
                            Matched(__pos, __value) => Matched(__pos, __value),
                            Failed => {
                                let __choice_res =
                                    slice_eq(__input, __state, __pos, "x");
                                match __choice_res {
                                    Matched(__pos, __value) => Matched(__pos, __value),
                                    Failed => {
                                        let __choice_res =
                                            slice_eq(__input, __state, __pos, "X");
                                        match __choice_res {
                                            Matched(__pos, __value) => {
                                                Matched(__pos, __value)
                                            }
                                            Failed => {
                                                let __choice_res = slice_eq(
                                                    __input, __state, __pos, "p",
                                                );
                                                match __choice_res {
                                                    Matched(__pos, __value) => {
                                                        Matched(__pos, __value)
                                                    }
                                                    Failed => {
                                                        let __choice_res = slice_eq(
                                                            __input, __state, __pos,
                                                            "b",
                                                        );
                                                        match __choice_res {
                                                            Matched(__pos, __value) => {
                                                                Matched(__pos, __value)
                                                            }
                                                            Failed => {
                                                                let __choice_res =
                                                                    slice_eq(
                                                                        __input,
                                                                        __state, __pos,
                                                                        "e",
                                                                    );
                                                                match __choice_res {
                                                                    Matched(
                                                                        __pos,
                                                                        __value,
                                                                    ) => Matched(
                                                                        __pos, __value,
                                                                    ),
                                                                    Failed => {
                                                                        let __choice_res =
                                                                            slice_eq(
                                                                                __input,
                                                                                __state,
                                                                                __pos,
                                                                                "E",
                                                                            );
                                                                        match __choice_res { Matched ( __pos , __value ) => Matched ( __pos , __value ) , Failed => slice_eq ( __input , __state , __pos , "?" ) }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } {
                Matched(__newpos, _) => {
                    Matched(__newpos, &__input[str_start..__newpos])
                }
                Failed => Failed,
            }
        };
        match __seq_res {
            Matched(__pos, n) => Matched(__pos, { n }),
            Failed => Failed,
        }
    }
}

fn __parse_format_spec<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<Option<&'input str>> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = slice_eq(__input, __state, __pos, ":");
        match __seq_res {
            Matched(__pos, _) => {
                let __seq_res = match {
                    let __seq_res = match {
                        let __seq_res = {
                            __state.suppress_fail += 1;
                            let __assert_res = if __input.len() > __pos {
                                let (__ch, __next) = char_range_at(__input, __pos);
                                match __ch {
                                    '<' | '^' | '>' => Matched(__next, ()),
                                    _ => __state.mark_failure(__pos, "[<^>]"),
                                }
                            } else {
                                __state.mark_failure(__pos, "[<^>]")
                            };
                            __state.suppress_fail -= 1;
                            match __assert_res {
                                Failed => Matched(__pos, ()),
                                Matched(..) => Failed,
                            }
                        };
                        match __seq_res {
                            Matched(__pos, _) => any_char(__input, __state, __pos),
                            Failed => Failed,
                        }
                    } {
                        Matched(__newpos, _) => Matched(__newpos, ()),
                        Failed => Matched(__pos, ()),
                    };
                    match __seq_res {
                        Matched(__pos, _) => {
                            if __input.len() > __pos {
                                let (__ch, __next) = char_range_at(__input, __pos);
                                match __ch {
                                    '<' | '^' | '>' => Matched(__next, ()),
                                    _ => __state.mark_failure(__pos, "[<^>]"),
                                }
                            } else {
                                __state.mark_failure(__pos, "[<^>]")
                            }
                        }
                        Failed => Failed,
                    }
                } {
                    Matched(__newpos, _) => Matched(__newpos, ()),
                    Failed => Matched(__pos, ()),
                };
                match __seq_res {
                    Matched(__pos, _) => {
                        let __seq_res = match {
                            let __choice_res = slice_eq(__input, __state, __pos, "+");
                            match __choice_res {
                                Matched(__pos, __value) => Matched(__pos, __value),
                                Failed => slice_eq(__input, __state, __pos, "-"),
                            }
                        } {
                            Matched(__newpos, _) => Matched(__newpos, ()),
                            Failed => Matched(__pos, ()),
                        };
                        match __seq_res {
                            Matched(__pos, _) => {
                                let __seq_res =
                                    match slice_eq(__input, __state, __pos, "#") {
                                        Matched(__newpos, _) => Matched(__newpos, ()),
                                        Failed => Matched(__pos, ()),
                                    };
                                match __seq_res {
                                    Matched(__pos, _) => {
                                        let __seq_res = match {
                                            let __choice_res = {
                                                let __seq_res = {
                                                    let mut __repeat_pos = __pos;
                                                    let mut __repeat_value = vec![];
                                                    loop {
                                                        let __pos = __repeat_pos;
                                                        let __step_res =
                                                            if __input.len() > __pos {
                                                                let (__ch, __next) =
                                                                    char_range_at(
                                                                        __input, __pos,
                                                                    );
                                                                match __ch {
                                                                    'A'...'Z'
                                                                    | 'a'...'z'
                                                                    | '0'...'9'
                                                                    | '_' => Matched(
                                                                        __next,
                                                                        (),
                                                                    ),
                                                                    _ => __state
                                                                        .mark_failure(
                                                                        __pos,
                                                                        "[A-Za-z0-9_]",
                                                                    ),
                                                                }
                                                            } else {
                                                                __state.mark_failure(
                                                                    __pos,
                                                                    "[A-Za-z0-9_]",
                                                                )
                                                            };
                                                        match __step_res {
                                                            Matched(
                                                                __newpos,
                                                                __value,
                                                            ) => {
                                                                __repeat_pos = __newpos;
                                                                __repeat_value
                                                                    .push(__value);
                                                            }
                                                            Failed => {
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    if __repeat_value.len() >= 1 {
                                                        Matched(__repeat_pos, ())
                                                    } else {
                                                        Failed
                                                    }
                                                };
                                                match __seq_res {
                                                    Matched(__pos, _) => slice_eq(
                                                        __input, __state, __pos, "$",
                                                    ),
                                                    Failed => Failed,
                                                }
                                            };
                                            match __choice_res {
                                                Matched(__pos, __value) => {
                                                    Matched(__pos, __value)
                                                }
                                                Failed => {
                                                    let mut __repeat_pos = __pos;
                                                    let mut __repeat_value = vec![];
                                                    loop {
                                                        let __pos = __repeat_pos;
                                                        let __step_res = if __input
                                                            .len()
                                                            > __pos
                                                        {
                                                            let (__ch, __next) =
                                                                char_range_at(
                                                                    __input, __pos,
                                                                );
                                                            match __ch {
                                                                '0'...'9' => {
                                                                    Matched(__next, ())
                                                                }
                                                                _ => __state
                                                                    .mark_failure(
                                                                        __pos, "[0-9]",
                                                                    ),
                                                            }
                                                        } else {
                                                            __state.mark_failure(
                                                                __pos, "[0-9]",
                                                            )
                                                        };
                                                        match __step_res {
                                                            Matched(
                                                                __newpos,
                                                                __value,
                                                            ) => {
                                                                __repeat_pos = __newpos;
                                                                __repeat_value
                                                                    .push(__value);
                                                            }
                                                            Failed => {
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    if __repeat_value.len() >= 1 {
                                                        Matched(__repeat_pos, ())
                                                    } else {
                                                        Failed
                                                    }
                                                }
                                            }
                                        } {
                                            Matched(__newpos, _) => {
                                                Matched(__newpos, ())
                                            }
                                            Failed => Matched(__pos, ()),
                                        };
                                        match __seq_res {
                                            Matched(__pos, _) => {
                                                let __seq_res = match slice_eq(
                                                    __input, __state, __pos, "0",
                                                ) {
                                                    Matched(__newpos, _) => {
                                                        Matched(__newpos, ())
                                                    }
                                                    Failed => Matched(__pos, ()),
                                                };
                                                match __seq_res {
                                                    Matched(__pos, _) => {
                                                        let __seq_res = match {
                                                            let __seq_res = slice_eq(
                                                                __input, __state,
                                                                __pos, ".",
                                                            );
                                                            match __seq_res {
                                                                Matched(__pos, _) => {
                                                                    let __choice_res = {
                                                                        let __seq_res = {
                                                                            let mut
                                                                            __repeat_pos =
                                                                                __pos;
                                                                            let mut
                                                                            __repeat_value =
                                                                                vec![];
                                                                            loop {
                                                                                let __pos = __repeat_pos ;
                                                                                let __step_res = if __input . len ( ) > __pos { let ( __ch , __next ) = char_range_at ( __input , __pos ) ; match __ch { 'A' ... 'Z' | 'a' ... 'z' | '0' ... '9' | '_' => Matched ( __next , ( ) ) , _ => __state . mark_failure ( __pos , "[A-Za-z0-9_]" ) , } } else { __state . mark_failure ( __pos , "[A-Za-z0-9_]" ) } ;
                                                                                match __step_res { Matched ( __newpos , __value ) => { __repeat_pos = __newpos ; __repeat_value . push ( __value ) ; } , Failed => { break ; } }
                                                                            }
                                                                            if __repeat_value . len ( ) >= 1 { Matched ( __repeat_pos , ( ) ) } else { Failed }
                                                                        };
                                                                        match __seq_res { Matched ( __pos , _ ) => { slice_eq ( __input , __state , __pos , "$" ) } Failed => Failed , }
                                                                    };
                                                                    match __choice_res {
                                                                        Matched(
                                                                            __pos,
                                                                            __value,
                                                                        ) => Matched(
                                                                            __pos,
                                                                            __value,
                                                                        ),
                                                                        Failed => {
                                                                            let __choice_res = {
                                                                                let mut __repeat_pos = __pos ;
                                                                                let mut
                                                                                __repeat_value = vec![];
                                                                                loop {
                                                                                    let __pos = __repeat_pos ;
                                                                                    let __step_res = if __input . len ( ) > __pos { let ( __ch , __next ) = char_range_at ( __input , __pos ) ; match __ch { '0' ... '9' => Matched ( __next , ( ) ) , _ => __state . mark_failure ( __pos , "[0-9]" ) , } } else { __state . mark_failure ( __pos , "[0-9]" ) } ;
                                                                                    match __step_res { Matched ( __newpos , __value ) => { __repeat_pos = __newpos ; __repeat_value . push ( __value ) ; } , Failed => { break ; } }
                                                                                }
                                                                                if __repeat_value . len ( ) >= 1 { Matched ( __repeat_pos , ( ) ) } else { Failed }
                                                                            };
                                                                            match __choice_res { Matched ( __pos , __value ) => Matched ( __pos , __value ) , Failed => slice_eq ( __input , __state , __pos , "*" ) }
                                                                        }
                                                                    }
                                                                }
                                                                Failed => Failed,
                                                            }
                                                        } {
                                                            Matched(__newpos, _) => {
                                                                Matched(__newpos, ())
                                                            }
                                                            Failed => {
                                                                Matched(__pos, ())
                                                            }
                                                        };
                                                        match __seq_res {
                                                            Matched(__pos, _) => {
                                                                let __seq_res =
                                                                    match __parse_ty(
                                                                        __input,
                                                                        __state, __pos,
                                                                    ) {
                                                                        Matched(
                                                                            __newpos,
                                                                            __value,
                                                                        ) => Matched(
                                                                            __newpos,
                                                                            Some(
                                                                                __value,
                                                                            ),
                                                                        ),
                                                                        Failed => {
                                                                            Matched(
                                                                                __pos,
                                                                                None,
                                                                            )
                                                                        }
                                                                    };
                                                                match __seq_res {
                                                                    Matched(
                                                                        __pos,
                                                                        n,
                                                                    ) => Matched(
                                                                        __pos,
                                                                        { n },
                                                                    ),
                                                                    Failed => Failed,
                                                                }
                                                            }
                                                            Failed => Failed,
                                                        }
                                                    }
                                                    Failed => Failed,
                                                }
                                            }
                                            Failed => Failed,
                                        }
                                    }
                                    Failed => Failed,
                                }
                            }
                            Failed => Failed,
                        }
                    }
                    Failed => Failed,
                }
            }
            Failed => Failed,
        }
    }
}

fn __parse_all_placeholders<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<Vec<&'input str>> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = {
            let mut __repeat_pos = __pos;
            let mut __repeat_value = vec![];
            loop {
                let __pos = __repeat_pos;
                let __step_res = {
                    let __choice_res = __parse_discard_doubles(__input, __state, __pos);
                    match __choice_res {
                        Matched(__pos, __value) => Matched(__pos, __value),
                        Failed => {
                            let __choice_res =
                                __parse_placeholder_inner(__input, __state, __pos);
                            match __choice_res {
                                Matched(__pos, __value) => Matched(__pos, __value),
                                Failed => __parse_discard_any(__input, __state, __pos),
                            }
                        }
                    }
                };
                match __step_res {
                    Matched(__newpos, __value) => {
                        __repeat_pos = __newpos;
                        __repeat_value.push(__value);
                    }
                    Failed => {
                        break;
                    }
                }
            }
            Matched(__repeat_pos, __repeat_value)
        };
        match __seq_res {
            Matched(__pos, x) => {
                Matched(__pos, { x.into_iter().flat_map(|x| x).collect() })
            }
            Failed => Failed,
        }
    }
}

fn __parse_format<'input>(
    __input: &'input str,
    __state: &mut ParseState<'input>,
    __pos: usize,
) -> RuleResult<(Option<usize>, Option<&'input str>)> {
    #![allow(non_snake_case, unused)]
    {
        let __seq_res = slice_eq(__input, __state, __pos, "{");
        match __seq_res {
            Matched(__pos, _) => {
                let __seq_res = match __parse_arg(__input, __state, __pos) {
                    Matched(__newpos, __value) => Matched(__newpos, Some(__value)),
                    Failed => Matched(__pos, None),
                };
                match __seq_res {
                    Matched(__pos, n) => {
                        let __seq_res =
                            match __parse_format_spec(__input, __state, __pos) {
                                Matched(__newpos, __value) => {
                                    Matched(__newpos, Some(__value))
                                }
                                Failed => Matched(__pos, None),
                            };
                        match __seq_res {
                            Matched(__pos, o) => {
                                let __seq_res = slice_eq(__input, __state, __pos, "}");
                                match __seq_res {
                                    Matched(__pos, _) => {
                                        Matched(__pos, { (n, o.and_then(|x| x)) })
                                    }
                                    Failed => Failed,
                                }
                            }
                            Failed => Failed,
                        }
                    }
                    Failed => Failed,
                }
            }
            Failed => Failed,
        }
    }
}

pub fn all_placeholders<'input>(__input: &'input str) -> ParseResult<Vec<&'input str>> {
    #![allow(non_snake_case, unused)]
    let mut __state = ParseState::new();
    match __parse_all_placeholders(__input, &mut __state, 0) {
        Matched(__pos, __value) => {
            if __pos == __input.len() {
                return Ok(__value);
            }
        }
        _ => {}
    }
    let (__line, __col) = pos_to_line(__input, __state.max_err_pos);
    Err(ParseError {
        line: __line,
        column: __col,
        offset: __state.max_err_pos,
        expected: __state.expected,
    })
}

pub fn format<'input>(
    __input: &'input str,
) -> ParseResult<(Option<usize>, Option<&'input str>)> {
    #![allow(non_snake_case, unused)]
    let mut __state = ParseState::new();
    match __parse_format(__input, &mut __state, 0) {
        Matched(__pos, __value) => {
            if __pos == __input.len() {
                return Ok(__value);
            }
        }
        _ => {}
    }
    let (__line, __col) = pos_to_line(__input, __state.max_err_pos);
    Err(ParseError {
        line: __line,
        column: __col,
        offset: __state.max_err_pos,
        expected: __state.expected,
    })
}
