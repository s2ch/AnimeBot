SRCS = animebot.c
OBJS = $(SRCS:.c=.o)
CFLAGS = -g -O2 -std=gnu99 `pkg-config --cflags json-glib-1.0`
LDFLAGS = -lcurl `pkg-config --libs json-glib-1.0`

compile:$(OBJS)
	gcc $(OBJS) $(CFLAGS) $(LDFLAGS)

.c.o:
	gcc $(CFLAGS) -c $< -o $@

run:
	make compile
	./animebot

clean:
	rm animebot

test:
	make compile
	valgrind ./animebot
