use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use ast::source_atom_set::SourceAtomSetIndex;

/// Struct for emitting bytecode for a property where its name is string.
///
/// If the property name is a string representing a number, it is considered an
/// index. In this case, NamePropertyEmitter falls back internally to
/// IndexPropertyEmitter
pub struct NamePropertyEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub state: &'a mut ObjectEmitterState,
    pub key: SourceAtomSetIndex,
    pub value: F,
}

impl<'a, F> NamePropertyEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack] OBJ

        match self.to_property_index(emitter, self.key) {
            Some(value) => {
                IndexPropertyEmitter {
                    state: self.state,
                    key: value as f64,
                    value: self.value,
                }
                .emit(emitter)?;
                //      [stack] OBJ
            }
            None => {
                let name_index = emitter.emit.get_atom_index(self.key);

                (self.value)(emitter)?;
                //      [stack] OBJ VALUE

                emitter.emit.init_prop(name_index);
                //      [stack] OBJ
            }
        }
        Ok(())
    }

    fn to_property_index(
        &self,
        emitter: &mut AstEmitter,
        index: SourceAtomSetIndex,
    ) -> Option<u32> {
        let s = emitter.compilation_info.atoms.get(index);
        s.parse::<u32>().ok()
    }
}

/// Struct for emitting bytecode for a property where its name is number.
pub struct IndexPropertyEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub state: &'a mut ObjectEmitterState,
    pub key: f64,
    pub value: F,
}

impl<'a, F> IndexPropertyEmitter<'a, F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack] OBJ

        emitter.emit.numeric(self.key);
        //              [stack] OBJ KEY

        (self.value)(emitter)?;
        //              [stack] OBJ KEY VALUE

        emitter.emit.init_elem();
        //              [stack] OBJ

        Ok(())
    }
}

/// Struct for emitting bytecode for a computed property.
pub struct ComputedPropertyEmitter<'a, F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub state: &'a mut ObjectEmitterState,
    pub key: F1,
    pub value: F2,
}

impl<'a, F1, F2> ComputedPropertyEmitter<'a, F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack] OBJ

        (self.key)(emitter)?;
        //              [stack] OBJ KEY

        (self.value)(emitter)?;
        //              [stack] OBJ KEY VALUE

        emitter.emit.init_elem();
        //              [stack] OBJ

        Ok(())
    }
}

/// No state so far.
struct ObjectEmitterStateInternal {}

/// Opaque struct that can be created only by ObjectEmitter.
/// This guarantees that *PropertyEmitter structs cannot be used outside
/// of ObjectEmitter callback.
pub struct ObjectEmitterState(ObjectEmitterStateInternal);

impl ObjectEmitterState {
    fn new() -> Self {
        Self(ObjectEmitterStateInternal {})
    }
}

pub struct ObjectEmitter<'a, PropT, PropF>
where
    PropF: Fn(&mut AstEmitter, &mut ObjectEmitterState, &PropT) -> Result<(), EmitError>,
{
    pub properties: std::slice::Iter<'a, PropT>,
    pub prop: PropF,
}

impl<'a, PropT, PropF> ObjectEmitter<'a, PropT, PropF>
where
    PropF: Fn(&mut AstEmitter, &mut ObjectEmitterState, &PropT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        emitter.emit.new_init();
        //              [stack] OBJ

        let mut state = ObjectEmitterState::new();

        for prop in self.properties {
            (self.prop)(emitter, &mut state, prop)?;
            //          [stack] OBJ
        }

        Ok(())
    }
}
