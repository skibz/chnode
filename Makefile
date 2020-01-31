
chnode: *.c
	cc \
		-fsanitize=undefined,nullability,integer \
		-Wall \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o $@

.PHONY: install
install: *.c
	cc \
		-O3 \
		-D_FORTIFY_SOURCE=2 \
		-Wall \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o chnode
	cp -r chnode $(PREFIX)/bin

clean:
	-rm -f *.o *.out chnode
	-rm -rf $$HOME/.chnode
