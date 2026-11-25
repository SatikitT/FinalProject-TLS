RELEASE_flags= -DNDEBUG -O3 -g0 
DEBUG_flags=-std=c99 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined

networking: ext
ext: src/uxncli.c
	@ mkdir -p bin/
	cc -DEXTENSION -DEXT_NETWORKING -DEXT_TLS ${RELEASE_flags} src/uxncli.c -o bin/uxncli -lmbedtls -lmbedx509 -ltfpsacrypto
run: bin/uxncli bin/tictactoe.rom
	@ bin/uxncli bin/tictactoe.rom
test: bin/uxncli-debug tests
	@ -bin/uxncli-debug
	@ -bin/uxncli-debug -v
	@ bin/uxncli-debug bin/opctest.rom
	@ bin/uxncli-debug bin/system.rom
	@ printf "ghi" | bin/uxncli-debug bin/console.rom "abc" "def"
	@ bin/uxncli-debug bin/file.rom
	@ bin/uxncli-debug bin/datetime.rom
format:
	clang-format -i src/uxncli.c
clean:
	rm -fr bin/
install: bin/uxncli
	cp bin/uxncli ~/bin/
grab: 
	mkdir -p etc/
	mkdir -p etc/utils/
	cp ../drifblim/etc/drifloon.rom.txt etc/utils/
	mkdir -p etc/tests/
	cp ../uxn11/etc/tests/* etc/tests/
archive: bin/examples.js
	cp src/uxncli.c ../oscean/etc/uxncli.c.txt
	cp bin/examples.js ../oscean/etc/uxnrepl/src/

.PHONY: ext networking run debug format clean install grab archive

bin/uxncli: src/uxncli.c
	@ mkdir -p bin/
	@ cc ${RELEASE_flags} src/uxncli.c -o bin/uxncli
bin/uxncli-debug: src/uxncli.c
	@ mkdir -p bin/
	@ cc ${DEBUG_flags} src/uxncli.c -o bin/uxncli-debug

# Tools

bin/drifloon.rom: bin/uxncli etc/utils/drifloon.rom.txt
	@ cat etc/utils/drifloon.rom.txt | ./bin/uxncli etc/utils/xh.rom > bin/drifloon.rom

# Tests

tests: bin/opctest.rom bin/system.rom bin/console.rom bin/file.rom bin/datetime.rom

bin/opctest.rom: bin/uxncli bin/drifloon.rom etc/tests/opctest.tal
	@ cat etc/tests/opctest.tal | bin/uxncli bin/drifloon.rom > bin/opctest.rom
bin/system.rom: bin/uxncli bin/drifloon.rom etc/tests/system.tal
	@ cat etc/tests/system.tal | bin/uxncli bin/drifloon.rom > bin/system.rom
bin/console.rom: bin/uxncli bin/drifloon.rom etc/tests/console.tal
	@ cat etc/tests/console.tal | bin/uxncli bin/drifloon.rom > bin/console.rom
bin/file.rom: bin/uxncli bin/drifloon.rom etc/tests/file.tal
	@ cat etc/tests/file.tal | bin/uxncli bin/drifloon.rom > bin/file.rom
bin/datetime.rom: bin/uxncli bin/drifloon.rom etc/tests/datetime.tal
	@ cat etc/tests/datetime.tal | bin/uxncli bin/drifloon.rom > bin/datetime.rom

bin/tictactoe.rom: bin/uxncli bin/drifloon.rom etc/examples/tictactoe.tal
	@ cat etc/examples/tictactoe.tal | bin/uxncli bin/drifloon.rom > bin/tictactoe.rom

# Examples

bin/examples.rom: bin/drifloon.rom etc/examples.tal 
	@ mkdir -p bin/
	@ cat etc/examples.tal | bin/uxncli bin/drifloon.rom > bin/examples.rom

bin/examples.js: bin/examples.rom examples/*
	@ mkdir -p bin/
	@ bin/uxncli bin/examples.rom examples/hello_world.tal examples/stack.tal examples/numbers.tal examples/functions.tal examples/variables.tal examples/if_else.tal examples/loops.tal examples/enums.tal examples/macros.tal examples/objects.tal examples/debugging.tal examples/fibonacci.tal examples/print_time.tal examples/fizzbuzz.tal examples/sierpinski.tal examples/mandelbrot.tal > bin/examples.js
