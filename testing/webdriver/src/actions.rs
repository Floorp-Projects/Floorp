use command::Parameters;
use common::{Nullable, WebElement};
use error::{WebDriverResult, WebDriverError, ErrorStatus};
use rustc_serialize::json::{ToJson, Json};
use unicode_segmentation::UnicodeSegmentation;
use std::collections::BTreeMap;
use std::default::Default;

#[derive(Debug, PartialEq)]
pub struct ActionSequence {
    pub id: Nullable<String>,
    pub actions: ActionsType
}

impl Parameters for ActionSequence {
    fn from_json(body: &Json) -> WebDriverResult<ActionSequence> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Actions chain was not an object");

        let type_name = try_opt!(try_opt!(data.get("type"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing type parameter").as_string(),
                                 ErrorStatus::InvalidArgument,
                                 "Parameter ;type' was not a string");

        let id = match data.get("id") {
            Some(x) => Some(try_opt!(x.as_string(),
                                     ErrorStatus::InvalidArgument,
                                     "Parameter 'id' was not a string").to_owned()),
            None => None
        };


        // Note that unlike the spec we get the pointer parameters in ActionsType::from_json

        let actions = match type_name {
            "none" | "key" | "pointer" => try!(ActionsType::from_json(&body)),
            _ => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                "Invalid action type"))
        };

        Ok(ActionSequence {
            id: id.into(),
            actions: actions
        })
    }
}

impl ToJson for ActionSequence {
    fn to_json(&self) -> Json {
        let mut data: BTreeMap<String, Json> = BTreeMap::new();
        data.insert("id".into(), self.id.to_json());
        let (action_type, actions) = match self.actions {
            ActionsType::Null(ref actions) => {
                ("none",
                 actions.iter().map(|x| x.to_json()).collect::<Vec<Json>>())
            }
            ActionsType::Key(ref actions) => {
                ("key",
                 actions.iter().map(|x| x.to_json()).collect::<Vec<Json>>())
            }
            ActionsType::Pointer(ref parameters, ref actions) => {
                data.insert("parameters".into(), parameters.to_json());
                ("pointer",
                 actions.iter().map(|x| x.to_json()).collect::<Vec<Json>>())
            }
        };
        data.insert("type".into(), action_type.to_json());
        data.insert("actions".into(), actions.to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub enum ActionsType {
    Null(Vec<NullActionItem>),
    Key(Vec<KeyActionItem>),
    Pointer(PointerActionParameters, Vec<PointerActionItem>)
}

impl Parameters for ActionsType {
    fn from_json(body: &Json) -> WebDriverResult<ActionsType> {
        // These unwraps are OK as long as this is only called from ActionSequence::from_json
        let data = body.as_object().expect("Body should be a JSON Object");
        let actions_type = body.find("type").and_then(|x| x.as_string()).expect("Type should be a string");
        let actions_chain = try_opt!(try_opt!(data.get("actions"),
                                              ErrorStatus::InvalidArgument,
                                              "Missing actions parameter").as_array(),
                                     ErrorStatus::InvalidArgument,
                                     "Parameter 'actions' was not an array");
        match actions_type {
            "none" => {
                let mut actions = Vec::with_capacity(actions_chain.len());
                for action_body in actions_chain.iter() {
                    actions.push(try!(NullActionItem::from_json(action_body)));
                };
                Ok(ActionsType::Null(actions))
            },
            "key" => {
                let mut actions = Vec::with_capacity(actions_chain.len());
                for action_body in actions_chain.iter() {
                    actions.push(try!(KeyActionItem::from_json(action_body)));
                };
                Ok(ActionsType::Key(actions))
            },
            "pointer" => {
                let mut actions = Vec::with_capacity(actions_chain.len());
                let parameters = match data.get("parameters") {
                    Some(x) => try!(PointerActionParameters::from_json(x)),
                    None => Default::default()
                };

                for action_body in actions_chain.iter() {
                    actions.push(try!(PointerActionItem::from_json(action_body)));
                }
                Ok(ActionsType::Pointer(parameters, actions))
            }
            _ => panic!("Got unexpected action type after checking type")
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum PointerType {
    Mouse,
    Pen,
    Touch,
}

impl Parameters for PointerType {
    fn from_json(body: &Json) -> WebDriverResult<PointerType> {
        match body.as_string() {
            Some("mouse") => Ok(PointerType::Mouse),
            Some("pen") => Ok(PointerType::Pen),
            Some("touch") => Ok(PointerType::Touch),
            Some(_) => Err(WebDriverError::new(
                ErrorStatus::InvalidArgument,
                "Unsupported pointer type"
            )),
            None => Err(WebDriverError::new(
                ErrorStatus::InvalidArgument,
                "Pointer type was not a string"
            ))
        }
    }
}

impl ToJson for PointerType {
    fn to_json(&self) -> Json {
        match *self {
            PointerType::Mouse => "mouse".to_json(),
            PointerType::Pen => "pen".to_json(),
            PointerType::Touch => "touch".to_json(),
        }.to_json()
    }
}

impl Default for PointerType {
    fn default() -> PointerType {
        PointerType::Mouse
    }
}

#[derive(Debug, Default, PartialEq)]
pub struct PointerActionParameters {
    pub pointer_type: PointerType
}

impl Parameters for PointerActionParameters {
    fn from_json(body: &Json) -> WebDriverResult<PointerActionParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Parameter 'parameters' was not an object");
        let pointer_type = match data.get("pointerType") {
            Some(x) => try!(PointerType::from_json(x)),
            None => PointerType::default()
        };
        Ok(PointerActionParameters {
            pointer_type: pointer_type
        })
    }
}

impl ToJson for PointerActionParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("pointerType".to_owned(),
                    self.pointer_type.to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub enum NullActionItem {
    General(GeneralAction)
}

impl Parameters for NullActionItem {
    fn from_json(body: &Json) -> WebDriverResult<NullActionItem> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Actions chain was not an object");
        let type_name = try_opt!(
            try_opt!(data.get("type"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'type' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "Parameter 'type' was not a string");
        match type_name {
            "pause" => Ok(NullActionItem::General(
                try!(GeneralAction::from_json(body)))),
            _ => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                "Invalid type attribute"))
        }
    }
}

impl ToJson for NullActionItem {
    fn to_json(&self) -> Json {
        match self {
            &NullActionItem::General(ref x) => x.to_json(),
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum KeyActionItem {
    General(GeneralAction),
    Key(KeyAction)
}

impl Parameters for KeyActionItem {
    fn from_json(body: &Json) -> WebDriverResult<KeyActionItem> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Key action item was not an object");
        let type_name = try_opt!(
            try_opt!(data.get("type"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'type' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "Parameter 'type' was not a string");
        match type_name {
            "pause" => Ok(KeyActionItem::General(
                try!(GeneralAction::from_json(body)))),
            _ => Ok(KeyActionItem::Key(
                try!(KeyAction::from_json(body))))
        }
    }
}

impl ToJson for KeyActionItem {
    fn to_json(&self) -> Json {
        match *self {
            KeyActionItem::General(ref x) => x.to_json(),
            KeyActionItem::Key(ref x) => x.to_json()
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum PointerActionItem {
    General(GeneralAction),
    Pointer(PointerAction)
}

impl Parameters for PointerActionItem {
    fn from_json(body: &Json) -> WebDriverResult<PointerActionItem> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Pointer action item was not an object");
        let type_name = try_opt!(
            try_opt!(data.get("type"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'type' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "Parameter 'type' was not a string");

        match type_name {
            "pause" => Ok(PointerActionItem::General(try!(GeneralAction::from_json(body)))),
            _ => Ok(PointerActionItem::Pointer(try!(PointerAction::from_json(body))))
        }
    }
}

impl ToJson for PointerActionItem {
    fn to_json(&self) -> Json {
        match self {
            &PointerActionItem::General(ref x) => x.to_json(),
            &PointerActionItem::Pointer(ref x) => x.to_json()
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum GeneralAction {
    Pause(PauseAction)
}

impl Parameters for GeneralAction {
    fn from_json(body: &Json) -> WebDriverResult<GeneralAction> {
        match body.find("type").and_then(|x| x.as_string()) {
            Some("pause") => Ok(GeneralAction::Pause(try!(PauseAction::from_json(body)))),
            _ => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                         "Invalid or missing type attribute"))
        }
    }
}

impl ToJson for GeneralAction {
    fn to_json(&self) -> Json {
        match self {
            &GeneralAction::Pause(ref x) => x.to_json()
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct PauseAction {
    pub duration: u64
}

impl Parameters for PauseAction {
    fn from_json(body: &Json) -> WebDriverResult<PauseAction> {
        let default = Json::U64(0);
        Ok(PauseAction {
            duration: try_opt!(body.find("duration").unwrap_or(&default).as_u64(),
                               ErrorStatus::InvalidArgument,
                               "Parameter 'duration' was not a positive integer")
        })
    }
}

impl ToJson for PauseAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(),
                    "pause".to_json());
        data.insert("duration".to_owned(),
                    self.duration.to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub enum KeyAction {
    Up(KeyUpAction),
    Down(KeyDownAction)
}

impl Parameters for KeyAction {
    fn from_json(body: &Json) -> WebDriverResult<KeyAction> {
        match body.find("type").and_then(|x| x.as_string()) {
            Some("keyDown") => Ok(KeyAction::Down(try!(KeyDownAction::from_json(body)))),
            Some("keyUp") => Ok(KeyAction::Up(try!(KeyUpAction::from_json(body)))),
            Some(_) | None => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                      "Invalid type attribute value for key action"))
        }
    }
}

impl ToJson for KeyAction {
    fn to_json(&self) -> Json {
        match self {
            &KeyAction::Down(ref x) => x.to_json(),
            &KeyAction::Up(ref x) => x.to_json(),
        }
    }
}

fn validate_key_value(value_str: &str) -> WebDriverResult<String> {
    let mut graphemes = value_str.graphemes(true);
    let value = if let Some(g) = graphemes.next() {
        g
    } else {
        return Err(WebDriverError::new(
            ErrorStatus::InvalidArgument,
            "Parameter 'value' was an empty string"))
    };
    if graphemes.next().is_some() {
        return Err(WebDriverError::new(
            ErrorStatus::InvalidArgument,
            "Parameter 'value' contained multiple graphemes"))
    };
    Ok(value.to_string())
}

#[derive(Debug, PartialEq)]
pub struct KeyUpAction {
    pub value: String
}

impl Parameters for KeyUpAction {
    fn from_json(body: &Json) -> WebDriverResult<KeyUpAction> {
        let value_str = try_opt!(
                try_opt!(body.find("value"),
                         ErrorStatus::InvalidArgument,
                         "Missing value parameter").as_string(),
                ErrorStatus::InvalidArgument,
            "Parameter 'value' was not a string");

        let value = try!(validate_key_value(value_str));
        Ok(KeyUpAction {
            value: value
        })
    }
}

impl ToJson for KeyUpAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(),
                    "keyUp".to_json());
        data.insert("value".to_string(),
                    self.value.to_string().to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub struct KeyDownAction {
    pub value: String
}

impl Parameters for KeyDownAction {
    fn from_json(body: &Json) -> WebDriverResult<KeyDownAction> {
        let value_str = try_opt!(
                try_opt!(body.find("value"),
                         ErrorStatus::InvalidArgument,
                         "Missing value parameter").as_string(),
                ErrorStatus::InvalidArgument,
            "Parameter 'value' was not a string");
        let value = try!(validate_key_value(value_str));
        Ok(KeyDownAction {
            value: value
        })
    }
}

impl ToJson for KeyDownAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(),
                    "keyDown".to_json());
        data.insert("value".to_owned(),
                    self.value.to_string().to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub enum PointerOrigin {
    Viewport,
    Pointer,
    Element(WebElement),
}

impl Parameters for PointerOrigin {
    fn from_json(body: &Json) -> WebDriverResult<PointerOrigin> {
        match *body {
            Json::String(ref x) => {
                match &**x {
                    "viewport" => Ok(PointerOrigin::Viewport),
                    "pointer" => Ok(PointerOrigin::Pointer),
                    _ => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                 "Unknown pointer origin"))
                }
            },
            Json::Object(_) => Ok(PointerOrigin::Element(try!(WebElement::from_json(body)))),
            _ => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                        "Pointer origin was not a string or an object"))
        }
    }
}

impl ToJson for PointerOrigin {
    fn to_json(&self) -> Json {
        match *self {
            PointerOrigin::Viewport => "viewport".to_json(),
            PointerOrigin::Pointer => "pointer".to_json(),
            PointerOrigin::Element(ref x) => x.to_json(),
        }
    }
}

impl Default for PointerOrigin {
    fn default() -> PointerOrigin {
        PointerOrigin::Viewport
    }
}

#[derive(Debug, PartialEq)]
pub enum PointerAction {
    Up(PointerUpAction),
    Down(PointerDownAction),
    Move(PointerMoveAction),
    Cancel
}

impl Parameters for PointerAction {
    fn from_json(body: &Json) -> WebDriverResult<PointerAction> {
        match body.find("type").and_then(|x| x.as_string()) {
            Some("pointerUp") => Ok(PointerAction::Up(try!(PointerUpAction::from_json(body)))),
            Some("pointerDown") => Ok(PointerAction::Down(try!(PointerDownAction::from_json(body)))),
            Some("pointerMove") => Ok(PointerAction::Move(try!(PointerMoveAction::from_json(body)))),
            Some("pointerCancel") => Ok(PointerAction::Cancel),
            Some(_) | None => Err(WebDriverError::new(
                ErrorStatus::InvalidArgument,
                "Missing or invalid type argument for pointer action"))
        }
    }
}

impl ToJson for PointerAction {
    fn to_json(&self) -> Json {
        match self {
            &PointerAction::Down(ref x) => x.to_json(),
            &PointerAction::Up(ref x) => x.to_json(),
            &PointerAction::Move(ref x) => x.to_json(),
            &PointerAction::Cancel => {
                let mut data = BTreeMap::new();
                data.insert("type".to_owned(),
                            "pointerCancel".to_json());
                Json::Object(data)
            }
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct PointerUpAction {
    pub button: u64,
}

impl Parameters for PointerUpAction {
    fn from_json(body: &Json) -> WebDriverResult<PointerUpAction> {
        let button = try_opt!(
            try_opt!(body.find("button"),
                     ErrorStatus::InvalidArgument,
                     "Missing button parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "Parameter 'button' was not a positive integer");

        Ok(PointerUpAction {
            button: button
        })
    }
}

impl ToJson for PointerUpAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(),
                    "pointerUp".to_json());
        data.insert("button".to_owned(), self.button.to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub struct PointerDownAction {
    pub button: u64,
}

impl Parameters for PointerDownAction {
    fn from_json(body: &Json) -> WebDriverResult<PointerDownAction> {
        let button = try_opt!(
            try_opt!(body.find("button"),
                     ErrorStatus::InvalidArgument,
                     "Missing button parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "Parameter 'button' was not a positive integer");

        Ok(PointerDownAction {
            button: button
        })
    }
}

impl ToJson for PointerDownAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(),
                    "pointerDown".to_json());
        data.insert("button".to_owned(), self.button.to_json());
        Json::Object(data)
    }
}

#[derive(Debug, PartialEq)]
pub struct PointerMoveAction {
    pub duration: Nullable<u64>,
    pub origin: PointerOrigin,
    pub x: Nullable<i64>,
    pub y: Nullable<i64>
}

impl Parameters for PointerMoveAction {
    fn from_json(body: &Json) -> WebDriverResult<PointerMoveAction> {
        let duration = match body.find("duration") {
            Some(duration) => Some(try_opt!(duration.as_u64(),
                                            ErrorStatus::InvalidArgument,
                                            "Parameter 'duration' was not a positive integer")),
            None => None

        };

        let origin = match body.find("origin") {
            Some(o) => try!(PointerOrigin::from_json(o)),
            None => PointerOrigin::default()
        };

        let x = match body.find("x") {
            Some(x) => {
                Some(try_opt!(x.as_i64(),
                              ErrorStatus::InvalidArgument,
                              "Parameter 'x' was not an integer"))
            },
            None => None
        };

        let y = match body.find("y") {
            Some(y) => {
                Some(try_opt!(y.as_i64(),
                              ErrorStatus::InvalidArgument,
                              "Parameter 'y' was not an integer"))
            },
            None => None
        };

        Ok(PointerMoveAction {
            duration: duration.into(),
            origin: origin.into(),
            x: x.into(),
            y: y.into(),
        })
    }
}

impl ToJson for PointerMoveAction {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_owned(), "pointerMove".to_json());
        if self.duration.is_value() {
            data.insert("duration".to_owned(),
                        self.duration.to_json());
        }

        data.insert("origin".to_owned(), self.origin.to_json());

        if self.x.is_value() {
            data.insert("x".to_owned(), self.x.to_json());
        }
        if self.y.is_value() {
            data.insert("y".to_owned(), self.y.to_json());
        }
        Json::Object(data)
    }
}
