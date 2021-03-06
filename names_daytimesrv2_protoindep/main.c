/* 
 * File:   main.c
 * Author: piter
 *
 * Created on June 8, 2014, 3:37 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "networking_functions.h"

int
tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
	int				listenfd, n;
	const int		on = 1;
	struct addrinfo	hints, *res, *ressave;

        /* We set the address family to AF_UNSPEC, but the caller can use
         * specific address to force a particular protocol (IPv4 or IPv6)
         * i.e. 0.0.0.0 for IPv4 or 0::0 for IPv6
         */
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( ( n = getaddrinfo( host, serv, &hints, &res)) != 0)
		err_quit( "tcp_listen error for %s, %s: %s",
				 host, serv, gai_strerror(n));
	ressave = res;

	do {
		listenfd = socket( res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)
			continue;		/* error, try next one */

		Setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break;			/* success */

		Close( listenfd);	/* bind error, close and try next one */
	} while ( ( res = res->ai_next) != NULL);

	if ( res == NULL)	/* errno from final socket() or bind() */
		err_sys("tcp_listen error for %s, %s", host, serv);

	Listen( listenfd, LISTENQ);

	if ( addrlenp)
		*addrlenp = res->ai_addrlen;	/* return size of protocol address */

	freeaddrinfo( ressave);

	return( listenfd);
}
/* end tcp_listen */

/*
 * We place the wrapper function here, not in wraplib.c, because some
 * XTI programs need to include wraplib.c, and it also defines
 * a Tcp_listen() function.
 */

int
Tcp_listen( const char *host, const char *serv, socklen_t *addrlenp)
{
	return( tcp_listen( host, serv, addrlenp));
}

/*
 * 
 */
int
main(int argc, char **argv)
{
	int			listenfd, connfd;
	socklen_t		len, addrlen;
	char			buff[MAXLINE];
	time_t			ticks;
	struct sockaddr_storage	cliaddr;

	if (argc == 2)
		listenfd = Tcp_listen(NULL, argv[1], &addrlen);
	else if (argc == 3)
		listenfd = Tcp_listen( argv[1], argv[2], &addrlen);
	else
		err_quit("usage: daytimetcpsrv2 [ <host> ] <service or port>");

	for ( ; ; ) {
		len = sizeof(cliaddr);
		connfd = Accept( listenfd, (SA *)&cliaddr, &len);
		printf( "connection from %s\n", Sock_ntop((SA *)&cliaddr));

		ticks = time( (time_t*)0);
                char* cp = ctime(&ticks);
                printf( "%s", ctime(&ticks));
		snprintf( buff, sizeof(buff), "%s\r\n", ctime(&ticks));
		Write(connfd, buff, strlen(buff));

		Close(connfd);
	}
}

