// use debug::Logger;
use cc::{CongruenceClosure, Key, KeyKind, Token};
use self::TypeStruct::*;

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum TypeStruct {
    // e.g., `<T as Iterator>::Item` would be `Assoc(Iterator::Item, vec![T])`
    Assoc(&'static str, Vec<Type>),

    // skolemized version of in-scope generic, e.g., the `T` when checking `fn foo<T>`
    Skolem(u32),

    // inference variable (existentially quantified)
    Variable(Token),

    // a nominal type applied to arguments, e.g. `i32` or `Vec<T>`
    Nominal(&'static str, Vec<Type>),
}

type Type = Box<TypeStruct>;

impl Key for Type {
    fn to_token(&self) -> Option<Token> {
        match **self {
            TypeStruct::Variable(t) => Some(t),
            _ => None,
        }
    }

    fn key_kind(&self) -> KeyKind {
        match **self {
            TypeStruct::Assoc(..) |
            TypeStruct::Variable(_) |
            TypeStruct::Skolem(_) =>
                KeyKind::Applicative,

            TypeStruct::Nominal(..) =>
                KeyKind::Generative,
        }
    }

    fn shallow_eq(&self, key: &Type) -> bool {
        match (&**self, &**key) {
            (&Assoc(i, _), &Assoc(j, _)) => i == j,
            (&Skolem(i), &Skolem(j)) => i == j,
            (&Nominal(i, _), &Nominal(j, _)) => i == j,
            _ => false,
        }
    }

    fn successors(&self) -> Vec<Self> {
        match **self {
            Assoc(_, ref s) => s.clone(),
            Skolem(_) => vec![],
            Variable(_) => vec![],
            Nominal(_, ref s) => s.clone(),
        }
    }
}

fn skolem(x: u32) -> Type {
    Box::new(Skolem(x))
}

fn iterator_item(t: Type) -> Type {
    Box::new(Assoc("Iterator::Item", vec![t]))
}

fn integer() -> Type {
    Box::new(Nominal("integer", vec![]))
}

fn character() -> Type {
    Box::new(Nominal("char", vec![]))
}

fn vec(t: Type) -> Type {
    Box::new(Nominal("Vec", vec![t]))
}

fn inference_var<'tcx>(cc: &mut CongruenceClosure<Type>) -> Type {
    let token = cc.new_token(KeyKind::Applicative,
                             move |token| Box::new(TypeStruct::Variable(token)));
    cc.key(token).clone()
}

#[test]
fn simple_as_it_gets() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();
    assert!(cc.merged(skolem(0), skolem(0)));
    assert!(!cc.merged(skolem(0), skolem(1)));
    assert!(cc.merged(skolem(1), skolem(1)));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(0))));
    assert!(!cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
    assert!(cc.merged(iterator_item(skolem(1)), iterator_item(skolem(1))));
}

#[test]
fn union_vars() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();
    cc.merge(skolem(0), skolem(1));
    assert!(cc.merged(skolem(0), skolem(1)));
}

#[test]
fn union_iterator_item_then_test_var() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();
    cc.merge(skolem(0), skolem(1));
    assert!(cc.merged(skolem(0), skolem(1)));
}

#[test]
fn union_direct() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.add(iterator_item(skolem(0)));
    cc.add(iterator_item(skolem(1)));
    cc.add(skolem(0));
    cc.add(skolem(1));

    cc.merge(skolem(0), skolem(1));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

macro_rules! indirect_test {
    ($test_name:ident: $a:expr, $b:expr; $c:expr, $d:expr) => {
        #[test]
        fn $test_name() {
            // Variant 1: call `add` explicitly
            //
            // This caused bugs because nodes were pre-existing.
            {
                let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

                cc.add(iterator_item(skolem(0)));
                cc.add(iterator_item(skolem(2)));
                cc.add(skolem(0));
                cc.add(skolem(1));
                cc.add(skolem(2));

                cc.merge($a, $b);
                cc.merge($c, $d);
                assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(2))));
            }

            // Variant 2: never call `add` explicitly
            //
            // This is more how we expect library to be used in practice.
            {
                let mut cc2: CongruenceClosure<Type> = CongruenceClosure::new();
                cc2.merge($a, $b);
                cc2.merge($c, $d);
                assert!(cc2.merged(iterator_item(skolem(0)), iterator_item(skolem(2))));
            }
        }
    }
}

// The indirect tests test for the case where we merge V0 and V1, and
// we merged V1 and V2, and we want to use this to conclude that
// Assoc(V0) and Assoc(V2) are merged -- but there is no node created for
// Assoc(V1).
indirect_test! { indirect_test_1: skolem(1), skolem(2); skolem(1), skolem(0) }
indirect_test! { indirect_test_2: skolem(2), skolem(1); skolem(1), skolem(0) }
indirect_test! { indirect_test_3: skolem(1), skolem(2); skolem(0), skolem(1) }
indirect_test! { indirect_test_4: skolem(2), skolem(1); skolem(0), skolem(1) }

// Here we determine that `Assoc(V0) == Assoc(V1)` because `V0==V1`,
// but we never add nodes for `Assoc(_)`.
#[test]
fn merged_no_add() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(skolem(0), skolem(1));

    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

// Here we determine that `Assoc(V0) == Assoc(V2)` because `V0==V1==V2`,
// but we never add nodes for `Assoc(_)`.
#[test]
fn merged_no_add_indirect() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(skolem(0), skolem(1));
    cc.merge(skolem(1), skolem(2));

    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(2))));
}

// Here we determine that `Assoc(V0) == Assoc(V2)` because `V0==V1==V2`,
// but we never add nodes for `Assoc(_)`.
#[test]
fn iterator_item_not_merged() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(iterator_item(skolem(0)), iterator_item(skolem(1)));

    assert!(!cc.merged(skolem(0), skolem(1)));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

// Here we show that merging `Assoc(V1) == Assoc(V2)` does NOT imply that
// `V1 == V2`.
#[test]
fn merge_fns_not_inputs() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(iterator_item(skolem(0)), iterator_item(skolem(1)));

    assert!(!cc.merged(skolem(0), skolem(1)));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

#[test]
fn inf_var_union() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    let v0 = inference_var(&mut cc);
    let v1 = inference_var(&mut cc);
    let v2 = inference_var(&mut cc);
    let iterator_item_v0 = iterator_item(v0.clone());
    let iterator_item_v1 = iterator_item(v1.clone());
    let iterator_item_v2 = iterator_item(v2.clone());

    cc.merge(v0.clone(), v1.clone());

    assert!(cc.map.is_empty()); // inf variables don't take up map slots

    assert!(cc.merged(iterator_item_v0.clone(), iterator_item_v1.clone()));
    assert!(!cc.merged(iterator_item_v0.clone(), iterator_item_v2.clone()));

    cc.merge(iterator_item_v0.clone(), iterator_item_v2.clone());
    assert!(cc.merged(iterator_item_v0.clone(), iterator_item_v2.clone()));
    assert!(cc.merged(iterator_item_v1.clone(), iterator_item_v2.clone()));

    assert_eq!(cc.map.len(), 3); // each iterator_item needs an entry
}

#[test]
fn skolem_union_no_add() {

    // This particular pattern of unifications exploits a potentially
    // subtle bug:
    // - We merge `skolem(0)` and `skolem(1)`
    //   and then merge `Assoc(skolem(0))` and `Assoc(skolem(2))`.
    // - From this we should be able to deduce that `Assoc(skolem(1)) == Assoc(skolem(2))`.
    // - However, if we are not careful with accounting for
    //   predecessors and so forth, this fails. For example, when
    //   adding `Assoc(skolem(1))`, we have to consider `Assoc(skolem(0))`
    //   to be a predecessor of `skolem(1)`.

    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(skolem(0), skolem(1));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
    assert!(!cc.merged(iterator_item(skolem(0)), iterator_item(skolem(2))));

    cc.merge(iterator_item(skolem(0)), iterator_item(skolem(2)));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(2))));
    assert!(cc.merged(iterator_item(skolem(1)), iterator_item(skolem(2))));
}

#[test]
fn merged_keys() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(skolem(0), skolem(1));
    cc.merge(iterator_item(skolem(0)), iterator_item(skolem(2)));

    // Here we don't yet see `iterator_item(skolem(1))` because it has no
    // corresponding node:
    let keys: Vec<Type> = cc.merged_keys(iterator_item(skolem(2))).collect();
    assert_eq!(&keys[..], &[iterator_item(skolem(2)), iterator_item(skolem(0))]);

    // But of course `merged` returns true (and adds a node):
    assert!(cc.merged(iterator_item(skolem(1)), iterator_item(skolem(2))));

    // So now we see it:
    let keys: Vec<Type> = cc.merged_keys(iterator_item(skolem(2))).collect();
    assert_eq!(&keys[..], &[iterator_item(skolem(2)),
                            iterator_item(skolem(1)),
                            iterator_item(skolem(0))]);
}

// Here we show that merging `Vec<V1> == Vec<V2>` DOES imply that
// `V1 == V2`.
#[test]
fn merge_vecs() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(vec(skolem(0)), vec(skolem(1)));

    assert!(cc.merged(skolem(0), skolem(1)));
    assert!(cc.merged(vec(skolem(0)), vec(skolem(1))));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

// Here we show that merging `Vec<V1::Item> == Vec<V2::Item>` does NOT imply that
// `V1 == V2`.
#[test]
fn merge_vecs_of_items() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(vec(iterator_item(skolem(0))),
             vec(iterator_item(skolem(1))));

    assert!(!cc.merged(skolem(0), skolem(1)));
    assert!(!cc.merged(vec(skolem(0)), vec(skolem(1))));
    assert!(cc.merged(vec(iterator_item(skolem(0))),
                      vec(iterator_item(skolem(1)))));
    assert!(cc.merged(iterator_item(vec(iterator_item(skolem(0)))),
                      iterator_item(vec(iterator_item(skolem(1))))));
    assert!(cc.merged(iterator_item(iterator_item(vec(iterator_item(skolem(0))))),
                      iterator_item(iterator_item(vec(iterator_item(skolem(1)))))));
    assert!(cc.merged(iterator_item(skolem(0)), iterator_item(skolem(1))));
}

// Here we merge `Vec<Int>::Item` with `Int` and then merge that later
// with an inference variable, and show that we concluded that the
// variable is (indeed) `Int`.
#[test]
fn merge_iterator_item_generative() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();
    cc.merge(iterator_item(vec(integer())), integer());
    let v0 = inference_var(&mut cc);
    cc.merge(iterator_item(vec(integer())), v0.clone());
    assert!(cc.merged(v0.clone(), integer()));
    assert!(cc.merged(vec(iterator_item(vec(integer()))), vec(integer())));
}

#[test]
fn merge_ripple() {
    let mut cc: CongruenceClosure<Type> = CongruenceClosure::new();

    cc.merge(iterator_item(skolem(1)), vec(skolem(0)));
    cc.merge(iterator_item(skolem(2)), vec(integer()));

    assert!(!cc.merged(iterator_item(skolem(1)), iterator_item(skolem(2))));

    println!("------------------------------");
    cc.merge(skolem(0), integer());

    println!("------------------------------");
    assert!(cc.merged(iterator_item(skolem(1)),
                      iterator_item(skolem(2))));
    assert!(cc.merged(iterator_item(iterator_item(skolem(1))),
                      iterator_item(iterator_item(skolem(2)))));
}
