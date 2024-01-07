DEBUG=-g
LIBS=-luring
BINARIES=\
	echoserver \
	io_uring  \
	io_uring2 \

all:	bin $(BINARIES:%=bin/%) 

bin:
	mkdir bin; cp echo-client.py ./bin

bin/%:	%.c
	gcc $(DEBUG) -o $@ $< $(LIBS)

clean:	
	rm -rf bin/

