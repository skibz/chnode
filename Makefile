
chnode: *.c
	cc \
		-fsanitize=undefined,nullability,integer \
		-Wall \
		-pedantic \
		$^ \
		$(shell curl-config --libs) \
		-o $@

clean:
	-rm -f *.o *.out chnode
	-rm -rf $$HOME/.chnode
