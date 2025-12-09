all:
	mkdir bin
	(cd src; make)
	(cd test; make)

install:
	mkdir -p /usr/local/bin/
	install ./bin/tec /usr/local/bin/
	install ./bin/tasm /usr/local/bin/

clean:
	rm -f bin/*
	rmdir bin