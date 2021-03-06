/* 
 * File:   main.c
 * Author: piter
 *
 * Created on June 19, 2014, 11:21 PM
 * 
 * A Unix domain socket or IPC socket (inter-process communication socket)
 * is a data communications endpoint for exchanging data between processes
 * executing within the same host operating system. While similar in
 * functionality to named pipes, Unix domain sockets may be created
 * as connection‑mode (SOCK_STREAM or SOCK_SEQPACKET) or as connectionless
 * (SOCK_DGRAM), while pipes are streams only. Processes using Unix domain
 * sockets do not need to share a common ancestry.
 * 
 * Unix domain sockets use the file system as their address name space.
 * They are referenced by processes as inodes in the file system.
 * This allows two processes to open the same socket in order to communicate.
 * However, communication occurs entirely within the operating system kernel.
 * In addition to sending data, processes may send file descriptors across
 * a Unix domain socket connection using the sendmsg() and recvmsg() system
 * calls.
 * 
 * Exactly how credentials are packaged up and sent as ancillary data tends
 * to be OS-specific. We use here Linux ucred structure. It is defined in
 * <socket.h>
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "networking_functions.h"


#define SCM_CREDENTIALS 0x02

struct ucred
{
  pid_t pid;			/* PID of sending process.  */
  uid_t uid;			/* UID of sending process.  */
  gid_t gid;			/* GID of sending process.  */
};

#define	CONTROL_LEN (sizeof(struct cmsghdr) + sizeof( struct ucred))

ssize_t
read_cred( int fd, void *ptr, size_t nbytes, struct ucred * cmsgcredptr)
{
	struct msghdr	msg;
	struct iovec	iov[1];
	char    control[CONTROL_LEN];
	int	n;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);
	msg.msg_flags = 0;

	if ( ( n = recvmsg( fd, &msg, 0)) < 0)
		return(n);
        
	if ( cmsgcredptr && msg.msg_controllen > 0) {
		struct cmsghdr	*cmptr = (struct cmsghdr *) control;

		if (cmptr->cmsg_len < CONTROL_LEN)
			err_quit("control length = %d", cmptr->cmsg_len);
		if (cmptr->cmsg_level != SOL_SOCKET)
			err_quit("control level != SOL_SOCKET");
		if (cmptr->cmsg_type != SCM_CREDENTIALS)
			err_quit("control type != SCM_CREDENTIALS");
		memcpy( cmsgcredptr, CMSG_DATA(cmptr), sizeof(struct ucred));
	}

	return(n);
}


void
str_echo(int sockfd)
{
	ssize_t			n;
	int			i, len;
	char			buf[MAXLINE];
	struct ucred    cred;

again:
	while ( ( n = read_cred( sockfd, buf, MAXLINE, &cred)) > 0) {
		if (cred.uid == 0) {
			printf("uuid==0\n");
                        printf("PID of sender = %d\n", cred.pid);
			printf("real user ID = %d\n", cred.uid);
			printf("real group ID = %d\n", cred.gid);
			printf("\n");
		} else {
			printf("PID of sender = %d\n", cred.pid);
			printf("real user ID = %d\n", cred.uid);
			printf("real group ID = %d\n", cred.gid);
			printf("\n");
		}
                
                bzero( &cred, sizeof( struct ucred));
                len = sizeof(struct ucred);
                if ( getsockopt( sockfd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
                    err_quit( "getsockopt");
                
                printf( "Credentials from SO_PEERCRED: pid=%ld, euid=%ld, egid=%ld\n",
                        (long) cred.pid, (long) cred.uid, (long) cred.gid);
    
		Writen( sockfd, buf, n);
	}

	if ( n < 0 && errno == EINTR)
		goto again;
	else if ( n < 0)
		err_sys( "str_echo: read error");
}

void
sig_chld(int signo)
{
	pid_t	pid;
	int		stat;

	while ( ( pid = waitpid( -1, &stat, WNOHANG)) > 0) {
		printf( "child %d terminated\n", pid);
	}
	return;
}


/*
 * 
 */
int
main( int argc, char **argv)
{
	int			listenfd, connfd, clilen, optval;
	socklen_t		len, childpid;
	struct sockaddr_un	servaddr, addr2;
        struct sockaddr_un	cliaddr;

	if ( argc != 2)
		err_quit( "usage: unix_domain_srv2_credentials <pathname>");

        /* Create IPC socket */
	listenfd = Socket( AF_LOCAL, SOCK_STREAM, 0);
        
       /* We must set the SO_PASSCRED socket option in order to receive credentials */

        optval = 1;
        if ( setsockopt( listenfd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1)
            err_quit("setsockopt");

        /* unlink in case the link exists from an earlier run of the server */
	unlink( argv[1]);		/* OK if this fails */

	bzero( &servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_LOCAL;
	strncpy( servaddr.sun_path, argv[1], sizeof(servaddr.sun_path)-1);
	Bind( listenfd, (SA *) &servaddr, SUN_LEN(&servaddr));

	len = sizeof( addr2);
	Getsockname( listenfd, (SA *) &addr2, &len);
	printf( "bound name = %s, returned len = %d\n", addr2.sun_path, len);
	
	Listen( listenfd, LISTENQ);

	Signal( SIGCHLD, sig_chld);

	for ( ; ; ) {
		clilen = sizeof(cliaddr);
		if ( ( connfd = accept( listenfd, (SA *) &cliaddr, &clilen)) < 0) {
			if ( errno == EINTR)
				continue;		/* back to for() */
			else
				err_sys( "accept error");
		}

		if ( ( childpid = Fork()) == 0) {	/* child process */
			Close( listenfd);               /* close listening socket */
			str_echo( connfd);              /* process request */
			exit( 0);
		}
                
		Close( connfd);                         /* parent closes connected socket */
	}
}