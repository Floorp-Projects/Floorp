use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;

/// Struct for emitting bytecode for an array element.
pub struct ArrayElementEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub state: &'a mut ArrayEmitterState,
    pub elem: F,
}

impl<'a, F> ArrayElementEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack] ARRAY INDEX?

        (self.elem)(emitter)?;
        //              [stack] ARRAY INDEX? ELEM

        match &mut self.state.0 {
            ArrayEmitterStateInternal::BeforeSpread { ref mut index } => {
                emitter.emit.init_elem_array(*index);
                //      [stack] ARRAY

                *index += 1;
            }
            ArrayEmitterStateInternal::AfterSpread => {
                emitter.emit.init_elem_inc();
                //      [stack] ARRAY INDEX+1
            }
        }

        //              [stack] ARRAY INDEX?

        Ok(())
    }
}

/// Struct for emitting bytecode for an array element with hole.
pub struct ArrayElisionEmitter<'a> {
    pub state: &'a mut ArrayEmitterState,
}

impl<'a> ArrayElisionEmitter<'a> {
    pub fn emit(self, emitter: &mut AstEmitter) {
        //              [stack] ARRAY INDEX?

        emitter.emit.hole();
        //              [stack] ARRAY INDEX? HOLE

        match &mut self.state.0 {
            ArrayEmitterStateInternal::BeforeSpread { ref mut index } => {
                emitter.emit.init_elem_array(*index);
                //      [stack] ARRAY

                *index += 1;
            }
            ArrayEmitterStateInternal::AfterSpread => {
                emitter.emit.init_elem_inc();
                //      [stack] ARRAY INDEX+1
            }
        }

        //              [stack] ARRAY INDEX?
    }
}

/// Struct for emitting bytecode for an array element with spread.
pub struct ArraySpreadEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub state: &'a mut ArrayEmitterState,
    pub elem: F,
}

impl<'a, F> ArraySpreadEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack] ARRAY INDEX?

        match self.state.0 {
            ArrayEmitterStateInternal::BeforeSpread { index } => {
                emitter.emit.numeric(index as f64);
            }
            _ => {}
        }
        self.state.0 = ArrayEmitterStateInternal::AfterSpread;

        //              [stack] ARRAY INDEX

        Err(EmitError::NotImplemented("TODO: spread element"))
    }
}

enum ArrayEmitterStateInternal {
    BeforeSpread { index: u32 },
    AfterSpread,
}

/// Opaque struct that can be created only by ArrayEmitter.
/// This guarantees that Array*Emitter structs cannot be used outside
/// of ArrayEmitter callback.
pub struct ArrayEmitterState(ArrayEmitterStateInternal);

impl ArrayEmitterState {
    fn new() -> Self {
        Self(ArrayEmitterStateInternal::BeforeSpread { index: 0 })
    }
}

pub enum ArrayElementKind {
    Normal,
    Elision,
    Spread,
}

/// Struct for emitting bytecode for an array expression.
pub struct ArrayEmitter<'a, ElemT, ElemKindF, ElemF>
where
    ElemKindF: Fn(&ElemT) -> ArrayElementKind,
    ElemF: Fn(&mut AstEmitter, &mut ArrayEmitterState, &'a ElemT) -> Result<(), EmitError>,
{
    pub elements: std::slice::Iter<'a, ElemT>,
    pub elem_kind: ElemKindF,
    pub elem: ElemF,
}

impl<'a, ElemT, ElemKindF, ElemF> ArrayEmitter<'a, ElemT, ElemKindF, ElemF>
where
    ElemKindF: Fn(&ElemT) -> ArrayElementKind,
    ElemF: Fn(&mut AstEmitter, &mut ArrayEmitterState, &'a ElemT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        // Initialize the array to its minimum possible length.
        let min_length = self
            .elements
            .clone()
            .map(|e| match (self.elem_kind)(e) {
                ArrayElementKind::Normal => 1,
                ArrayElementKind::Elision => 1,
                ArrayElementKind::Spread => 0,
            })
            .sum::<u32>();

        emitter.emit.new_array(min_length);
        //              [stack] ARRAY

        let mut state = ArrayEmitterState::new();
        for element in self.elements {
            (self.elem)(emitter, &mut state, element)?;
            //          [stack] ARRAY INDEX?
        }

        match state.0 {
            ArrayEmitterStateInternal::AfterSpread => {
                //      [stack] ARRAY INDEX

                emitter.emit.pop();
                //      [stack] ARRAY
            }
            _ => {}
        }

        //              [stack] ARRAY

        Ok(())
    }
}
