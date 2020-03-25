
chnode: *.c
	cc \
		-fsanitize=undefined,nullability,integer \
		-std=c18 \
		-Wall \
		-D PREFIX="\"$(shell echo $$HOME)\"" \
		-D TRACE \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o $@

.PHONY: install
install: *.c
	cc \
		-O3 \
		-D_FORTIFY_SOURCE=2 \
		-D PREFIX="\"$(PREFIX)\"" \
		-std=c18 \
		-Wall \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o chnode
	cp -r chnode $(PREFIX)/bin

.PHONY: zig
zig:
	zig \
		build-exe \
		--c-source chnode.c \
		--library curl \
		-D PREFIX="\"$(shell echo $$HOME)\"" \
		-D TRACE=1

clean:
	-rm -f *.o *.out chnode
	-rm -rf $$HOME/.chnode zig-cache
