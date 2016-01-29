bin_PROGRAMS	=	server client sem rwlock cond

server_SOURCES	=	server.c
server_LDADD	=	../lib/librivus.a -lpthread

client_SOURCES	=	client.c
client_LDADD 	=	-lpthread

sem_SOURCES		=	sem_eg.c
sem_LDADD		=	../lib/librivus.a -lpthread

rwlock_SOURCES	=	rwlock_eg.c
rwlock_LDADD	=	../lib/librivus.a -lpthread

cond_SOURCES	=	cond_eg.c
cond_LDADD		=	../lib/librivus.a -lpthread
