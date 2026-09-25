PROG=	drain
SRCS=	drain.c
MAN=
WARNS?=	6

CFLAGS+=	-I/usr/local/include
LDADD+=		-L/usr/local/lib -lnotcurses-core

.include <bsd.prog.mk>
