proja: tunnel.o proja.o
	gcc -o proja -g tunnel.o proja.o -lrt -lm

tunnel.o: tunnel.c tunnel.h
	gcc -c tunnel.c

proja.o: proja.c
	gcc -g -c -Wall proja.c

clean:
	rm -f *.o proja