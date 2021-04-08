#![allow(dead_code)]

#[cfg(not(any(
    feature = "profile-with-optick",
    feature = "profile-with-puffin",
    feature = "profile-with-superluminal",
    feature = "profile-with-tracing",
    feature = "profile-with-tracy",
)))]
fn main() {
    panic!("No profiler feature flags were enabled. Since this is an example, this is probably a mistake.");
}

// Just check that one of these features was enabled because otherwise, nothing interesting will happen
#[cfg(any(
    feature = "profile-with-optick",
    feature = "profile-with-puffin",
    feature = "profile-with-superluminal",
    feature = "profile-with-tracing",
    feature = "profile-with-tracy",
))]
fn main() {
    // Good to call this on any threads that are created to get clearer profiling results
    profiling::register_thread!("Main Thread");

    // Set up the tracy layer in the tracing crate. This is just an example using tracing. This is
    // not necessary if using tracy directly. (profile-with-tracy)
    #[cfg(feature = "profile-with-tracing")]
    {
        use tracing_subscriber::layer::SubscriberExt;
        tracing::subscriber::set_global_default(
            tracing_subscriber::registry().with(tracing_tracy::TracyLayer::new()),
        )
        .unwrap();
    }

    // Turn on tracing for puffin (you would still need to render/save this somehow!)
    #[cfg(feature = "profile-with-puffin")]
    profiling::puffin::set_scopes_on(true);

    println!("Starting loop, profiler can now be attached");

    loop {
        // Generate some profiling info
        profiling::scope!("Main Thread");
        some_function();
        some_other_function(10);

        println!("frame complete");

        // Finish the frame.
        profiling::finish_frame!();
    }
}

fn burn_time(millis: u128) {
    let start_time = std::time::Instant::now();
    loop {
        if (std::time::Instant::now() - start_time).as_millis() > millis {
            break;
        }
    }
}

// This `profiling::function` attribute is equivalent to profiling::scope!(function_name)
#[profiling::function]
fn some_function() {
    burn_time(5);
}

fn some_other_function(iterations: usize) {
    profiling::scope!("some_other_function");
    burn_time(5);

    {
        profiling::scope!("do iterations");
        for i in 0..iterations {
            profiling::scope!(
                "some_inner_function_that_sleeps",
                format!("other data {}", i).as_str()
            );

            // Mixing general profiling API calls with profiler-specific API calls is allowed
            #[cfg(feature = "profile-with-optick")]
            profiling::optick::tag!("extra_data", "MORE DATA");

            some_inner_function(i);
            burn_time(1);
        }
    }
}

#[profiling::function]
fn some_inner_function(_iteration_index: usize) {
    burn_time(10);
}
