set -e

echo "\n\nRun device-changed tests\n===================="

if [[ -z "${RUST_BACKTRACE}" ]]; then
    # Display backtrace for debugging
    export RUST_BACKTRACE=1
fi
echo "RUST_BACKTRACE is set to ${RUST_BACKTRACE}\n"

cargo test test_switch_device -- --ignored --nocapture

cargo test test_plug_and_unplug_device -- --ignored --nocapture

cargo test test_register_device_changed_callback_to_check_default_device_changed_input -- --ignored --nocapture
cargo test test_register_device_changed_callback_to_check_default_device_changed_output -- --ignored --nocapture
cargo test test_register_device_changed_callback_to_check_default_device_changed_duplex -- --ignored --nocapture
cargo test test_register_device_changed_callback_to_check_input_alive_changed_input -- --ignored --nocapture
cargo test test_register_device_changed_callback_to_check_input_alive_changed_duplex -- --ignored --nocapture

cargo test test_destroy_input_stream_after_unplugging_a_nondefault_input_device -- --ignored --nocapture
cargo test test_suspend_input_stream_by_unplugging_a_nondefault_input_device -- --ignored --nocapture

cargo test test_destroy_input_stream_after_unplugging_a_default_input_device -- --ignored --nocapture
cargo test test_reinit_input_stream_by_unplugging_a_default_input_device -- --ignored --nocapture

cargo test test_destroy_output_stream_after_unplugging_a_nondefault_output_device -- --ignored --nocapture
cargo test test_suspend_output_stream_by_unplugging_a_nondefault_output_device -- --ignored --nocapture

cargo test test_destroy_output_stream_after_unplugging_a_default_output_device -- --ignored --nocapture
cargo test test_reinit_output_stream_by_unplugging_a_default_output_device -- --ignored --nocapture

cargo test test_destroy_duplex_stream_after_unplugging_a_nondefault_input_device -- --ignored --nocapture
cargo test test_suspend_duplex_stream_by_unplugging_a_nondefault_input_device -- --ignored --nocapture

cargo test test_destroy_duplex_stream_after_unplugging_a_nondefault_output_device -- --ignored --nocapture
cargo test test_suspend_duplex_stream_by_unplugging_a_nondefault_output_device -- --ignored --nocapture

cargo test test_destroy_duplex_stream_after_unplugging_a_default_input_device -- --ignored --nocapture
cargo test test_reinit_duplex_stream_by_unplugging_a_default_input_device -- --ignored --nocapture

cargo test test_destroy_duplex_stream_after_unplugging_a_default_output_device -- --ignored --nocapture
cargo test test_reinit_duplex_stream_by_unplugging_a_default_output_device -- --ignored --nocapture
