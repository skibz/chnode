
chnode: *.c
	cc \
		-fsanitize=undefined,nullability,integer \
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
		-Wall \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o chnode
	cp -r chnode $(PREFIX)/bin

clean:
	-rm -f *.o *.out chnode
	-rm -rf $$HOME/.chnode
