FROM rustlang/rust:nightly
WORKDIR /image-png
COPY . .
RUN cargo install cargo-fuzz
RUN cargo fuzz run decode -- -runs=0
