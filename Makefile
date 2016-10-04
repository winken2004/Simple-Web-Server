all:
	gcc webserver.c -o webserver
clean:
	rm webserver
debug:
	gcc webserver.c -D__DEBUG__ -o webserver
