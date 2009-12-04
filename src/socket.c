/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*  Monkey HTTP Daemon
 *  ------------------
 *  Copyright (C) 2008-2009 Eduardo Silva P.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. 
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "socket.h"
#include "memory.h"

/* 
 * Example from:
 * http://www.baus.net/on-tcp_cork
 */
int mk_socket_set_cork_flag(int fd, int state)
{
      	return setsockopt(fd, SOL_TCP, TCP_CORK, &state, sizeof(state));
}

int mk_socket_set_nonblocking(int sockfd)
{
        if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
                perror("fcntl");
                return -1;
        }
        return 0;
}

int mk_socket_set_tcp_nodelay(int sockfd)
{
        int on=1;
        return setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &on, sizeof(on));
}

int mk_socket_get_ip(int socket, char *ipv4)
{
        int ipv4_len = 16;
        socklen_t len;
	struct sockaddr_in m_addr;

        len = sizeof(m_addr);
        getpeername(socket, (struct sockaddr*)&m_addr,  &len);
        inet_ntop(PF_INET, &m_addr.sin_addr, ipv4, ipv4_len);

        return 0;
}

int mk_socket_close(int socket)
{
	return close(socket);
}

int mk_socket_timeout(int s, char *buf, int len, 
		int timeout, int recv_send)
{
	fd_set fds;
	time_t init_time, max_time;
	int n=0, status;
	struct timeval tv;

	init_time=time(NULL);
	max_time = init_time + timeout;

	FD_ZERO(&fds);
	FD_SET(s,&fds);
	
	tv.tv_sec=timeout;
	tv.tv_usec=0;

	if(recv_send==ST_RECV)
		n=select(s+1,&fds,NULL,NULL,&tv);  // recv 
	else{
		n=select(s+1,NULL,&fds,NULL,&tv);  // send 
	}

	switch(n){
		case 0:
				return -2;
				break;
		case -1:
				//pthread_kill(pthread_self(), SIGPIPE);
				return -1;
	}
	
	if(recv_send==ST_RECV){
		status=recv(s,buf,len, 0);
	}
	else{
		status=send(s,buf,len, 0);
	}

	if( status < 0 ){
		if(time(NULL) >= max_time){
			//pthread_kill(pthread_self(), SIGPIPE);
		}
	}
	
	return status;
}

int mk_socket_create()
{
        int sockfd;

        if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                perror("client: socket");
                return -1;
        }

        return sockfd;
}

int mk_socket_connect(int sockfd, char *server, int port)
{
        int res;
        struct sockaddr_in *remote;

        remote = (struct sockaddr_in *) 
                mk_mem_malloc_z(sizeof(struct sockaddr_in));
        remote->sin_family = AF_INET;
        res = inet_pton(AF_INET, server, (void *)(&(remote->sin_addr.s_addr)));

        if(res < 0)  
        {
                perror("Can't set remote->sin_addr.s_addr");
                mk_mem_free(remote);
                return -1;
        }
        else if(res == 0){
                perror("Invalid IP address\n");
                mk_mem_free(remote);
                return -1;
        }

        remote->sin_port = htons(port);
        if (connect(sockfd, 
                    (struct sockaddr *)remote, 
                    sizeof(struct sockaddr)) == -1)
        {
                close(sockfd);
                perror("client: connect");
                return -1;
        }
        mk_mem_free(remote);
        return 0;
}

void mk_socket_reset(int socket)
{
	int status=1;
	
	if(setsockopt(socket,SOL_SOCKET,SO_REUSEADDR,&status,sizeof(int))==-1) {
		perror("setsockopt");
                exit(1);
	}	
}

/* Just IPv4 for now... */
int mk_socket_server(int port)
{
        int fd;
	struct sockaddr_in local_sockaddr_in;

        /* Create server socket */
        fd=socket(PF_INET,SOCK_STREAM,0);
        mk_socket_set_tcp_nodelay(fd);
        //mk_socket_set_nonblocking(fd);

	local_sockaddr_in.sin_family=AF_INET;
	local_sockaddr_in.sin_port=htons(port);
	local_sockaddr_in.sin_addr.s_addr=INADDR_ANY;
	memset(&(local_sockaddr_in.sin_zero),'\0',8);

        /* Avoid bind issues, reset socket */
	mk_socket_reset(fd);

	if(bind(fd,(struct sockaddr *)&local_sockaddr_in,
                sizeof(struct sockaddr)) != 0)
        {
                perror("bind");
                printf("Error: Port %i cannot be used\n", port);
		exit(1);
	}		     
	
        /* Listen queue:
         * The queue limit is given by /proc/sys/net/core/somaxconn
         * we need to add a dynamic function to get that value on fly
         */
	if((listen(fd, 1024))!=0) {
		perror("listen");
		exit(1);
	}

        return fd;
}
