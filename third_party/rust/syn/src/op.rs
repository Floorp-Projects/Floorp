#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum BinOp {
    /// The `+` operator (addition)
    Add,
    /// The `-` operator (subtraction)
    Sub,
    /// The `*` operator (multiplication)
    Mul,
    /// The `/` operator (division)
    Div,
    /// The `%` operator (modulus)
    Rem,
    /// The `&&` operator (logical and)
    And,
    /// The `||` operator (logical or)
    Or,
    /// The `^` operator (bitwise xor)
    BitXor,
    /// The `&` operator (bitwise and)
    BitAnd,
    /// The `|` operator (bitwise or)
    BitOr,
    /// The `<<` operator (shift left)
    Shl,
    /// The `>>` operator (shift right)
    Shr,
    /// The `==` operator (equality)
    Eq,
    /// The `<` operator (less than)
    Lt,
    /// The `<=` operator (less than or equal to)
    Le,
    /// The `!=` operator (not equal to)
    Ne,
    /// The `>=` operator (greater than or equal to)
    Ge,
    /// The `>` operator (greater than)
    Gt,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum UnOp {
    /// The `*` operator for dereferencing
    Deref,
    /// The `!` operator for logical inversion
    Not,
    /// The `-` operator for negation
    Neg,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    named!(pub binop -> BinOp, alt!(
        punct!("&&") => { |_| BinOp::And }
        |
        punct!("||") => { |_| BinOp::Or }
        |
        punct!("<<") => { |_| BinOp::Shl }
        |
        punct!(">>") => { |_| BinOp::Shr }
        |
        punct!("==") => { |_| BinOp::Eq }
        |
        punct!("<=") => { |_| BinOp::Le }
        |
        punct!("!=") => { |_| BinOp::Ne }
        |
        punct!(">=") => { |_| BinOp::Ge }
        |
        punct!("+") => { |_| BinOp::Add }
        |
        punct!("-") => { |_| BinOp::Sub }
        |
        punct!("*") => { |_| BinOp::Mul }
        |
        punct!("/") => { |_| BinOp::Div }
        |
        punct!("%") => { |_| BinOp::Rem }
        |
        punct!("^") => { |_| BinOp::BitXor }
        |
        punct!("&") => { |_| BinOp::BitAnd }
        |
        punct!("|") => { |_| BinOp::BitOr }
        |
        punct!("<") => { |_| BinOp::Lt }
        |
        punct!(">") => { |_| BinOp::Gt }
    ));

    #[cfg(feature = "full")]
    named!(pub assign_op -> BinOp, alt!(
        punct!("+=") => { |_| BinOp::Add }
        |
        punct!("-=") => { |_| BinOp::Sub }
        |
        punct!("*=") => { |_| BinOp::Mul }
        |
        punct!("/=") => { |_| BinOp::Div }
        |
        punct!("%=") => { |_| BinOp::Rem }
        |
        punct!("^=") => { |_| BinOp::BitXor }
        |
        punct!("&=") => { |_| BinOp::BitAnd }
        |
        punct!("|=") => { |_| BinOp::BitOr }
        |
        punct!("<<=") => { |_| BinOp::Shl }
        |
        punct!(">>=") => { |_| BinOp::Shr }
    ));

    named!(pub unop -> UnOp, alt!(
        punct!("*") => { |_| UnOp::Deref }
        |
        punct!("!") => { |_| UnOp::Not }
        |
        punct!("-") => { |_| UnOp::Neg }
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{Tokens, ToTokens};

    impl BinOp {
        pub fn op(&self) -> &'static str {
            match *self {
                BinOp::Add => "+",
                BinOp::Sub => "-",
                BinOp::Mul => "*",
                BinOp::Div => "/",
                BinOp::Rem => "%",
                BinOp::And => "&&",
                BinOp::Or => "||",
                BinOp::BitXor => "^",
                BinOp::BitAnd => "&",
                BinOp::BitOr => "|",
                BinOp::Shl => "<<",
                BinOp::Shr => ">>",
                BinOp::Eq => "==",
                BinOp::Lt => "<",
                BinOp::Le => "<=",
                BinOp::Ne => "!=",
                BinOp::Ge => ">=",
                BinOp::Gt => ">",
            }
        }

        pub fn assign_op(&self) -> Option<&'static str> {
            match *self {
                BinOp::Add => Some("+="),
                BinOp::Sub => Some("-="),
                BinOp::Mul => Some("*="),
                BinOp::Div => Some("/="),
                BinOp::Rem => Some("%="),
                BinOp::BitXor => Some("^="),
                BinOp::BitAnd => Some("&="),
                BinOp::BitOr => Some("|="),
                BinOp::Shl => Some("<<="),
                BinOp::Shr => Some(">>="),
                _ => None,
            }
        }
    }

    impl ToTokens for BinOp {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append(self.op());
        }
    }

    impl UnOp {
        pub fn op(&self) -> &'static str {
            match *self {
                UnOp::Deref => "*",
                UnOp::Not => "!",
                UnOp::Neg => "-",
            }
        }
    }

    impl ToTokens for UnOp {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append(self.op());
        }
    }
}
