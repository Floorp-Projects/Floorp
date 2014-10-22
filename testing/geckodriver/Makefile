all: .version

.version: Cargo.lock
	grep -E -m1 '^version\s*=\s*"[^"]*"$$' Cargo.lock | grep -Eo '[0-9\.]+' | tr -d '\n' > $@

.PHONY = clean
clean:
	rm -f .version
