#![allow(deprecated)]

mod argument {
    use pin_project::{pin_project, project};

    #[pin_project]
    struct A(#[pin] ());

    #[project]
    fn unexpected_local1() {
        let mut x = A(());
        #[project()] //~ ERROR unexpected token
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    fn unexpected_local1() {
        let mut x = A(());
        #[project(foo)] //~ ERROR unexpected token
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    fn unexpected_expr1() {
        let mut x = A(());
        #[project()] //~ ERROR unexpected token
        match Pin::new(&mut x).project() {
            A(_) => {}
        }
    }

    #[project]
    fn unexpected_expr1() {
        let mut x = A(());
        #[project(foo)] //~ ERROR unexpected token
        match Pin::new(&mut x).project() {
            A(_) => {}
        }
    }

    #[project()] // Ok
    fn unexpected_item1() {}

    #[project(foo)] //~ ERROR unexpected token
    fn unexpected_item2() {}
}

mod attribute {
    use pin_project::{pin_project, project, project_ref, project_replace};

    #[pin_project(project_replace)]
    struct A(#[pin] ());

    #[project]
    fn duplicate_stmt_project() {
        let mut x = A(());
        #[project]
        #[project] //~ ERROR duplicate #[project] attribute
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_ref]
    fn duplicate_stmt_project_ref() {
        let mut x = A(());
        #[project_ref]
        #[project_ref] //~ ERROR duplicate #[project_ref] attribute
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_replace]
    fn duplicate_stmt_project_replace() {
        let mut x = A(());
        #[project_replace]
        #[project_replace] //~ ERROR duplicate #[project_replace] attribute
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    fn combine_stmt_project1() {
        let mut x = A(());
        #[project]
        #[project_ref] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    fn combine_stmt_project2() {
        let mut x = A(());
        #[project]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    fn combine_stmt_project3() {
        let mut x = A(());
        #[project_ref]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_ref]
    fn combine_stmt_project_ref1() {
        let mut x = A(());
        #[project]
        #[project_ref] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_ref]
    fn combine_stmt_project_ref2() {
        let mut x = A(());
        #[project]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_ref]
    fn combine_stmt_project_ref3() {
        let mut x = A(());
        #[project_ref]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_replace]
    fn combine_stmt_project_replace1() {
        let mut x = A(());
        #[project]
        #[project_ref] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_replace]
    fn combine_stmt_project_replace2() {
        let mut x = A(());
        #[project]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project_replace]
    fn combine_stmt_project_replace3() {
        let mut x = A(());
        #[project_ref]
        #[project_replace] //~ ERROR are mutually exclusive
        let A(_) = Pin::new(&mut x).project();
    }

    #[project]
    #[project] //~ ERROR duplicate #[project] attribute
    fn duplicate_fn_project() {}

    #[project_ref]
    #[project_ref] //~ ERROR duplicate #[project_ref] attribute
    fn duplicate_fn_project_ref() {}

    #[project_replace]
    #[project_replace] //~ ERROR duplicate #[project_replace] attribute
    fn duplicate_fn_project_replace() {}

    #[project]
    #[project] //~ ERROR duplicate #[project] attribute
    impl A {}

    #[project_ref]
    #[project_ref] //~ ERROR duplicate #[project_ref] attribute
    impl A {}

    #[project_replace]
    #[project_replace] //~ ERROR duplicate #[project_replace] attribute
    impl A {}

    #[allow(unused_imports)]
    mod use_ {
        use pin_project::{project, project_ref, project_replace};

        #[project]
        #[project] //~ ERROR duplicate #[project] attribute
        use super::A;

        #[project_ref]
        #[project_ref] //~ ERROR duplicate #[project_ref] attribute
        use super::A;

        #[project_replace]
        #[project_replace] //~ ERROR duplicate #[project_replace] attribute
        use super::A;
    }
}

fn main() {}
