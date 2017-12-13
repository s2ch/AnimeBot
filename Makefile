compile:
	gcc -o animebot animebot.c -g -std=gnu99 -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -fPIE -ftrapv -O2 -lcurl `pkg-config --cflags --libs json-glib-1.0` 

run:
	make compile
	./animebot

clean:
	rm animebot

test:
	make compile
	valgrind ./animebot
