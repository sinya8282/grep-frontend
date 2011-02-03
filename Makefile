OPT=-O3 -Wall #-B/usr/local

all: oniguruma re2

re2:
	g++ ${OPT} -lre2 gre2p.cc -o gre2p

oniguruma:
	gcc ${OPT} -lonig onigrep.c -o onigrep

clean:
	rm -rf *.o gre2p onigrep
