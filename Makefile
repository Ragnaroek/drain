PROG=	drain
SRCS=	drain.c power.c
MAN=
WARNS?=	6

CFLAGS+=	-I/usr/local/include
LDADD+=		-L/usr/local/lib -lnotcurses-core -lm

.include <bsd.prog.mk>
