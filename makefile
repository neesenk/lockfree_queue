CFLAGS=-Wall -O2 -g

all:
	gcc -Wall -O3 -g mqueue.c t1.c -o t1 -lpthread -Wno-strict-aliasing
	gcc -Wall -O3 -g mqueue.c t2.c -o t2 -lpthread -Wno-strict-aliasing
	gcc -Wall -O3 -g squeue.c t3.c -o t3 -lpthread
	gcc -Wall -O3 -g ringbuffer.c t4.c -o t4 -lpthread -Wno-strict-aliasing

clean:
	rm -rf *.o t1 t2 t3 t4

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^
