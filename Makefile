
chnode: *.c
	cc \
		-fsanitize=undefined,nullability,integer \
		-Wall \
		-pedantic \
		-o $@ \
		$(shell curl-config --libs) \
		$^

clean:
	-rm -f *.o
