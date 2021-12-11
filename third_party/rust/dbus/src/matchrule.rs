use crate::{Message, MessageType, BusName, Path, Interface, Member};


#[derive(Clone, Debug, Default)]
/// A "match rule", that can match Messages on its headers.
///
/// A field set to "None" means no filter for that header, 
/// a field set to "Some(_)" must match exactly.
pub struct MatchRule<'a> {
    /// Match on message type (you typically want to do this)
    pub msg_type: Option<MessageType>,
    /// Match on message sender
    pub sender: Option<BusName<'a>>,
    /// Match on message object path
    pub path: Option<Path<'a>>,
    /// Match on message interface
    pub interface: Option<Interface<'a>>,
    /// Match on message member (signal or method name)
    pub member: Option<Member<'a>>,
    _more_fields_may_come: (),
}

fn msg_type_str(m: MessageType) -> &'static str {
    use MessageType::*;
    match m {
        Signal => "signal",
        MethodCall => "method_call",
        MethodReturn => "method_return",
        Error => "error",
        Invalid => unreachable!(),
    }
}


impl<'a> MatchRule<'a> {
    /// Make a string which you can use in the call to "add_match".
    ///
    /// Panics: if msg_type is set to Some(MessageType::Invalid)
    pub fn match_str(&self) -> String {
        let mut v = vec!();
        if let Some(x) = self.msg_type { v.push(("type", msg_type_str(x))) };
        if let Some(ref x) = self.sender { v.push(("sender", &x)) };
        if let Some(ref x) = self.path { v.push(("path", &x)) };
        if let Some(ref x) = self.interface { v.push(("interface", &x)) };
        if let Some(ref x) = self.member { v.push(("member", &x)) };

        // For now we don't need to worry about internal quotes in strings as those are not valid names. 
        // If we start matching against arguments, we need to worry.
        let v: Vec<_> = v.into_iter().map(|(k, v)| format!("{}='{}'", k, v)).collect();
        v.join(",")
    }

    /// Returns whether or not the message matches the rule.
    pub fn matches(&self, msg: &Message) -> bool {
        if let Some(x) = self.msg_type { if x != msg.msg_type() { return false; }};
        if self.sender.is_some() && msg.sender() != self.sender { return false };
        if self.path.is_some() && msg.path() != self.path { return false };
        if self.interface.is_some() && msg.interface() != self.interface { return false };
        if self.member.is_some() && msg.member() != self.member { return false };
        true
    }

    /// Create a new struct which matches every message.
    pub fn new() -> Self { Default::default() }

    /// Returns a clone with no static references
    pub fn into_static(&self) -> MatchRule<'static> {
        MatchRule {
            msg_type: self.msg_type,
            sender: self.sender.as_ref().map(|x| x.clone().into_static()),
            path: self.path.as_ref().map(|x| x.clone().into_static()),
            interface: self.interface.as_ref().map(|x| x.clone().into_static()),
            member: self.member.as_ref().map(|x| x.clone().into_static()),
            _more_fields_may_come: (),
        }
    }
} 
