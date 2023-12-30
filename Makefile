CC = gcc
CFLAGS = -Wall
LDFLAGS = -lX11 -lImlib2 -lXfixes
SRCS = custray.c 
EXEC = custray

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRCS) $(LDFLAGS)

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mv -f custray ${DESTDIR}${PREFIX}/bin

clean:
	rm -f $(EXEC)