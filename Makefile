CFLAGS?=        -Wall -ggdb -W -O
CC?=            gcc
LIBS?=
LDFLAGS?=
PREFIX?=        /usr/local/HTTP_KILLER
VERSION=1.5
TMPDIR=/tmp/HTTP_KILLER-$(VERSION)

all:   HTTP_KILLER tags

tags:  *.c
        -ctags *.c

install: HTTP_KILLER
        install -d $(DESTDIR)$(PREFIX)/bin
        install -s HTT_KILLER $(DESTDIR)$(PREFIX)/bin
        ln -sf $(DESTDIR)$(PREFIX)/bin/webbench $(DESTDIR)/usr/local/bin/HTTP_KILLER

        install -d $(DESTDIR)$(PREFIX)/share/doc/HTTP_KILLER
        install -m 644 debian/copyright $(DESTDIR)$(PREFIX)/share/doc/HTTP_KILLER
        install -m 644 debian/changelog $(DESTDIR)$(PREFIX)/share/doc/HTTP_KILLER

HTTP_KILLER: HTTP_KILLER.o Makefile
        $(CC) $(CFLAGS) $(LDFLAGS) -o HTTP_KILLER HTTP_KILLER.o $(LIBS) 

clean:
        -rm -f *.o HTTP_KILLER *~ core *.core tags

tar:   clean
        -debian/rules clean
        rm -rf $(TMPDIR)
        install -d $(TMPDIR)
        cp -p Makefile HTTP_KILLER.c socket.c $(TMPDIR)
        install -d $(TMPDIR)/debian
        -cp -p debian/* $(TMPDIR)/debian
        ln -sf debian/copyright $(TMPDIR)/COPYRIGHT
        ln -sf debian/changelog $(TMPDIR)/ChangeLog
        -cd $(TMPDIR) && cd .. && tar cozf HTTP_KILLER-$(VERSION).tar.gz HTTP_KILLER-$(VERSION)

webbench.o:     HTTP_KILLER.c socket.c Makefile

.PHONY: clean install all tar
