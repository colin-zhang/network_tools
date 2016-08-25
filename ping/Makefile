all:ping1 ping2
	
ping1:ping_main.c
	gcc -o $@ $^ -g -Wall
ping2:ping2.c
	gcc -o $@ $^ -g -Wall
clean:
	rm -rf ping1 ping2 *.o
