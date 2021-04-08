#[path = "../../tracing-futures/tests/support.rs"]
// we don't use some of the test support functions, but `tracing-futures` does.
#[allow(dead_code)]
mod support;
use support::*;

use tracing::subscriber::with_default;
use tracing_attributes::instrument;

#[instrument]
async fn test_async_fn(polls: usize) -> Result<(), ()> {
    let future = PollN::new_ok(polls);
    tracing::trace!(awaiting = true);
    future.await
}

#[test]
fn async_fn_only_enters_for_polls() {
    let (subscriber, handle) = subscriber::mock()
        .new_span(span::mock().named("test_async_fn"))
        .enter(span::mock().named("test_async_fn"))
        .event(event::mock().with_fields(field::mock("awaiting").with_value(&true)))
        .exit(span::mock().named("test_async_fn"))
        .enter(span::mock().named("test_async_fn"))
        .exit(span::mock().named("test_async_fn"))
        .drop_span(span::mock().named("test_async_fn"))
        .done()
        .run_with_handle();
    with_default(subscriber, || {
        block_on_future(async { test_async_fn(2).await }).unwrap();
    });
    handle.assert_finished();
}

#[test]
fn async_fn_nested() {
    #[instrument]
    async fn test_async_fns_nested() {
        test_async_fns_nested_other().await
    }

    #[instrument]
    async fn test_async_fns_nested_other() {
        tracing::trace!(nested = true);
    }

    let span = span::mock().named("test_async_fns_nested");
    let span2 = span::mock().named("test_async_fns_nested_other");
    let (subscriber, handle) = subscriber::mock()
        .new_span(span.clone())
        .enter(span.clone())
        .new_span(span2.clone())
        .enter(span2.clone())
        .event(event::mock().with_fields(field::mock("nested").with_value(&true)))
        .exit(span2.clone())
        .drop_span(span2)
        .exit(span.clone())
        .drop_span(span)
        .done()
        .run_with_handle();

    with_default(subscriber, || {
        block_on_future(async { test_async_fns_nested().await });
    });

    handle.assert_finished();
}

#[test]
fn async_fn_with_async_trait() {
    use async_trait::async_trait;

    // test the correctness of the metadata obtained by #[instrument]
    // (function name, functions parameters) when async-trait is used
    #[async_trait]
    pub trait TestA {
        async fn foo(&mut self, v: usize);
    }

    // test nesting of async fns with aync-trait
    #[async_trait]
    pub trait TestB {
        async fn bar(&self);
    }

    // test skip(self) with async-await
    #[async_trait]
    pub trait TestC {
        async fn baz(&self);
    }

    #[derive(Debug)]
    struct TestImpl(usize);

    #[async_trait]
    impl TestA for TestImpl {
        #[instrument]
        async fn foo(&mut self, v: usize) {
            self.baz().await;
            self.0 = v;
            self.bar().await
        }
    }

    #[async_trait]
    impl TestB for TestImpl {
        #[instrument]
        async fn bar(&self) {
            tracing::trace!(val = self.0);
        }
    }

    #[async_trait]
    impl TestC for TestImpl {
        #[instrument(skip(self))]
        async fn baz(&self) {
            tracing::trace!(val = self.0);
        }
    }

    let span = span::mock().named("foo");
    let span2 = span::mock().named("bar");
    let span3 = span::mock().named("baz");
    let (subscriber, handle) = subscriber::mock()
        .new_span(
            span.clone()
                .with_field(field::mock("self"))
                .with_field(field::mock("v")),
        )
        .enter(span.clone())
        .new_span(span3.clone())
        .enter(span3.clone())
        .event(event::mock().with_fields(field::mock("val").with_value(&2u64)))
        .exit(span3.clone())
        .drop_span(span3)
        .new_span(span2.clone().with_field(field::mock("self")))
        .enter(span2.clone())
        .event(event::mock().with_fields(field::mock("val").with_value(&5u64)))
        .exit(span2.clone())
        .drop_span(span2)
        .exit(span.clone())
        .drop_span(span)
        .done()
        .run_with_handle();

    with_default(subscriber, || {
        let mut test = TestImpl(2);
        block_on_future(async { test.foo(5).await });
    });

    handle.assert_finished();
}

#[test]
fn async_fn_with_async_trait_and_fields_expressions() {
    use async_trait::async_trait;

    #[async_trait]
    pub trait Test {
        async fn call(&mut self, v: usize);
    }

    #[derive(Clone, Debug)]
    struct TestImpl;

    impl TestImpl {
        fn foo(&self) -> usize {
            42
        }
    }

    #[async_trait]
    impl Test for TestImpl {
        // check that self is correctly handled, even when using async_trait
        #[instrument(fields(val=self.foo(), test=%v+5))]
        async fn call(&mut self, v: usize) {}
    }

    let span = span::mock().named("call");
    let (subscriber, handle) = subscriber::mock()
        .new_span(
            span.clone().with_field(
                field::mock("v")
                    .with_value(&tracing::field::debug(5))
                    .and(field::mock("test").with_value(&tracing::field::debug(10)))
                    .and(field::mock("val").with_value(&42u64)),
            ),
        )
        .enter(span.clone())
        .exit(span.clone())
        .drop_span(span)
        .done()
        .run_with_handle();

    with_default(subscriber, || {
        block_on_future(async { TestImpl.call(5).await });
    });

    handle.assert_finished();
}

#[test]
fn async_fn_with_async_trait_and_fields_expressions_with_generic_parameter() {
    use async_trait::async_trait;

    #[async_trait]
    pub trait Test {
        async fn call();
        async fn call_with_self(&self);
        async fn call_with_mut_self(&mut self);
    }

    #[derive(Clone, Debug)]
    struct TestImpl;

    #[async_trait]
    impl Test for TestImpl {
        // instrumenting this is currently not possible, see https://github.com/tokio-rs/tracing/issues/864#issuecomment-667508801
        //#[instrument(fields(Self=std::any::type_name::<Self>()))]
        async fn call() {}

        #[instrument(fields(Self=std::any::type_name::<Self>()))]
        async fn call_with_self(&self) {}

        #[instrument(fields(Self=std::any::type_name::<Self>()))]
        async fn call_with_mut_self(&mut self) {}
    }

    //let span = span::mock().named("call");
    let span2 = span::mock().named("call_with_self");
    let span3 = span::mock().named("call_with_mut_self");
    let (subscriber, handle) = subscriber::mock()
        /*.new_span(span.clone()
            .with_field(
                field::mock("Self").with_value(&"TestImpler")))
        .enter(span.clone())
        .exit(span.clone())
        .drop_span(span)*/
        .new_span(
            span2
                .clone()
                .with_field(field::mock("Self").with_value(&std::any::type_name::<TestImpl>())),
        )
        .enter(span2.clone())
        .exit(span2.clone())
        .drop_span(span2)
        .new_span(
            span3
                .clone()
                .with_field(field::mock("Self").with_value(&std::any::type_name::<TestImpl>())),
        )
        .enter(span3.clone())
        .exit(span3.clone())
        .drop_span(span3)
        .done()
        .run_with_handle();

    with_default(subscriber, || {
        block_on_future(async {
            TestImpl::call().await;
            TestImpl.call_with_self().await;
            TestImpl.call_with_mut_self().await
        });
    });

    handle.assert_finished();
}
