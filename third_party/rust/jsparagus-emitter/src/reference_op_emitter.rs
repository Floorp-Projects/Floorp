use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use crate::script_atom_set::ScriptAtomSetIndex;
use ast::source_atom_set::SourceAtomSetIndex;

#[derive(Debug, PartialEq)]
enum AssignmentReferenceKind {
    GlobalName(ScriptAtomSetIndex),
    #[allow(dead_code)]
    Prop(ScriptAtomSetIndex),
    #[allow(dead_code)]
    Elem,
}

// See AssignmentReferenceEmitter.
// This uses struct to hide the details from the consumer.
#[derive(Debug)]
#[must_use]
pub struct AssignmentReference {
    kind: AssignmentReferenceKind,
}
impl AssignmentReference {
    fn new(kind: AssignmentReferenceKind) -> Self {
        Self { kind }
    }

    fn stack_slots(&self) -> usize {
        match self.kind {
            AssignmentReferenceKind::GlobalName(_) => 1,
            AssignmentReferenceKind::Prop(_) => 1,
            AssignmentReferenceKind::Elem => 2,
        }
    }
}

#[derive(Debug, PartialEq)]
enum CallKind {
    Normal,
    // FIXME: Support eval, Function#call, Function#apply etc.
}

// See *ReferenceEmitter.
// This uses struct to hide the details from the consumer.
#[derive(Debug)]
#[must_use]
pub struct CallReference {
    kind: CallKind,
}
impl CallReference {
    fn new(kind: CallKind) -> Self {
        Self { kind }
    }
}

// Struct for emitting bytecode for get `name` operation.
pub struct GetNameEmitter {
    pub name: SourceAtomSetIndex,
}
impl GetNameEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) {
        let name_index = emitter.emit.get_atom_index(self.name);

        //              [stack]

        // FIXME: Support non-global case.
        emitter.emit.get_g_name(name_index);
        //              [stack] VAL
    }
}

// Struct for emitting bytecode for get `obj.key` operation.
pub struct GetPropEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub obj: F,
    pub key: SourceAtomSetIndex,
}
impl<F> GetPropEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        let key_index = emitter.emit.get_atom_index(self.key);

        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ

        emitter.emit.get_prop(key_index);
        //              [stack] VAL

        Ok(())
    }
}

// Struct for emitting bytecode for get `super.key` operation.
pub struct GetSuperPropEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub this: F,
    pub key: SourceAtomSetIndex,
}
impl<F> GetSuperPropEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        let key_index = emitter.emit.get_atom_index(self.key);

        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.this)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS

        emitter.emit.callee();
        //              [stack] THIS CALLEE

        emitter.emit.super_base();
        //              [stack] THIS OBJ

        emitter.emit.get_prop_super(key_index);
        //              [stack] VAL

        Ok(())
    }
}

// Struct for emitting bytecode for get `obj[key]` operation.
pub struct GetElemEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub obj: F1,
    pub key: F2,
}
impl<F1, F2> GetElemEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ

        let depth = emitter.emit.stack_depth();
        (self.key)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ KEY

        emitter.emit.get_elem();
        //              [stack] VAL

        Ok(())
    }
}

// Struct for emitting bytecode for get `super[key]` operation.
pub struct GetSuperElemEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub this: F1,
    pub key: F2,
}
impl<F1, F2> GetSuperElemEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.this)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS

        let depth = emitter.emit.stack_depth();
        (self.key)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS KEY

        emitter.emit.callee();
        //              [stack] THIS KEY CALLEE

        emitter.emit.super_base();
        //              [stack] THIS KEY OBJ

        emitter.emit.get_elem_super();
        //              [stack] VAL

        Ok(())
    }
}

// Struct for emitting bytecode for `name` reference.
pub struct NameReferenceEmitter {
    pub name: SourceAtomSetIndex,
}
impl NameReferenceEmitter {
    pub fn emit_for_call(self, emitter: &mut AstEmitter) -> CallReference {
        let name_index = emitter.emit.get_atom_index(self.name);

        //              [stack]

        // FIXME: Support non-global case.
        emitter.emit.get_g_name(name_index);
        //              [stack] CALLEE

        // FIXME: Support non-global cases.
        emitter.emit.g_implicit_this(name_index);
        //              [stack] CALLEE THIS

        CallReference::new(CallKind::Normal)
    }

    pub fn emit_for_assignment(self, emitter: &mut AstEmitter) -> AssignmentReference {
        let name_index = emitter.emit.get_atom_index(self.name);

        //              [stack]

        // FIXME: Support non-global case.
        emitter.emit.bind_g_name(name_index);
        //              [stack] GLOBAL

        AssignmentReference::new(AssignmentReferenceKind::GlobalName(name_index))
    }
}

// Struct for emitting bytecode for `obj.key` reference.
pub struct PropReferenceEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub obj: F,
    pub key: SourceAtomSetIndex,
}
impl<F> PropReferenceEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit_for_call(self, emitter: &mut AstEmitter) -> Result<CallReference, EmitError> {
        let key_index = emitter.emit.get_atom_index(self.key);

        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS

        emitter.emit.dup();
        //              [stack] THIS THIS

        // FIXME: Support super.
        emitter.emit.call_prop(key_index);
        //              [stack] THIS CALLEE

        emitter.emit.swap();
        //              [stack] CALLEE THIS

        Ok(CallReference::new(CallKind::Normal))
    }

    #[allow(dead_code)]
    pub fn emit_for_assignment(
        self,
        emitter: &mut AstEmitter,
    ) -> Result<AssignmentReference, EmitError> {
        let key_index = emitter.emit.get_atom_index(self.key);

        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ

        Ok(AssignmentReference::new(AssignmentReferenceKind::Prop(
            key_index,
        )))
    }
}

// Struct for emitting bytecode for `obj[key]` reference.
pub struct ElemReferenceEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub obj: F1,
    pub key: F2,
}
impl<F1, F2> ElemReferenceEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit_for_call(self, emitter: &mut AstEmitter) -> Result<CallReference, EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS

        emitter.emit.dup();
        //              [stack] THIS THIS

        let depth = emitter.emit.stack_depth();
        (self.key)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] THIS THIS KEY

        // FIXME: Support super.
        emitter.emit.call_elem();
        //              [stack] THIS CALLEE

        emitter.emit.swap();
        //              [stack] CALLEE THIS

        Ok(CallReference::new(CallKind::Normal))
    }

    #[allow(dead_code)]
    pub fn emit_for_assignment(
        self,
        emitter: &mut AstEmitter,
    ) -> Result<AssignmentReference, EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.obj)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ

        let depth = emitter.emit.stack_depth();
        (self.key)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] OBJ KEY

        Ok(AssignmentReference::new(AssignmentReferenceKind::Elem))
    }
}

// Struct for emitting bytecode for call `callee(arguments)` operation.
pub struct CallEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<CallReference, EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<usize, EmitError>,
{
    pub callee: F1,
    pub arguments: F2,
}
impl<F1, F2> CallEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<CallReference, EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<usize, EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        let reference = (self.callee)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 2);
        //              [stack] CALLEE THIS

        // FIXME: Support spread.
        let depth = emitter.emit.stack_depth();
        let len = (self.arguments)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + len);
        //              [stack] CALLEE THIS ARGS...

        match reference.kind {
            CallKind::Normal => {
                emitter.emit.call(len as u16);
                //      [stack] VAL
            }
        }

        Ok(())
    }
}

// Struct for emitting bytecode for `new callee(arguments)` operation.
pub struct NewEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<usize, EmitError>,
{
    pub callee: F1,
    pub arguments: F2,
}
impl<F1, F2> NewEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<(), EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<usize, EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        (self.callee)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] CALLEE

        emitter.emit.is_constructing();
        //              [stack] CALLEE JS_IS_CONSTRUCTING

        // FIXME: Support spread.
        let depth = emitter.emit.stack_depth();
        let len = (self.arguments)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + len);
        //              [stack] CALLEE JS_IS_CONSTRUCTING ARGS...

        emitter.emit.dup_at(len as u32 + 1);
        //              [stack] CALLEE JS_IS_CONSTRUCTING ARGS... CALLEE

        emitter.emit.new_(len as u16);
        //              [stack] VAL

        Ok(())
    }
}

// Struct for emitting bytecode for assignment `lhs = rhs` operation.
pub struct AssignmentEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<AssignmentReference, EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub lhs: F1,
    pub rhs: F2,
}
impl<F1, F2> AssignmentEmitter<F1, F2>
where
    F1: Fn(&mut AstEmitter) -> Result<AssignmentReference, EmitError>,
    F2: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        let depth = emitter.emit.stack_depth();
        let reference = (self.lhs)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + reference.stack_slots());
        //              [stack] REF...

        let depth = emitter.emit.stack_depth();
        (self.rhs)(emitter)?;
        debug_assert_eq!(emitter.emit.stack_depth(), depth + 1);
        //              [stack] REF... VAL

        match reference.kind {
            AssignmentReferenceKind::GlobalName(name_index) => {
                //      [stack] GLOBAL VAL

                // FIXME: Support non-global cases.
                emitter.emit.set_g_name(name_index);
                //      [stack] VAL
            }
            AssignmentReferenceKind::Prop(key_index) => {
                //      [stack] OBJ VAL

                // FIXME: Support strict mode and super.
                emitter.emit.set_prop(key_index);
                //      [stack] VAL
            }
            AssignmentReferenceKind::Elem => {
                //      [stack] OBJ KEY VAL

                // FIXME: Support strict mode and super.
                emitter.emit.set_elem();
                //      [stack] VAL
            }
        }

        Ok(())
    }

    // FIXME: Support compound assignment
}

// FIXME: Add increment
