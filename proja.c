#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "tunnel.h"

#define MAXLINE 1024
int primary_port;

int process_argv(int argc, char const *argv[]) {
	if (argc <= 1) {
		perror("Should be more than two arguments passed!\n");
		exit(EXIT_FAILURE);
	}
	// we need to read and know it's which stage 1 or 2..
	FILE *f = fopen(argv[1], "r");
	if (f == NULL) {
		perror("Can't open configuration file!\n");
		exit(EXIT_FAILURE);
	}
	char line[256];
	int stage = 0;
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '#') continue;
		int i = (int) strlen(line) - 1;
		for (; i >= 0; --i) {
			if (line[i] == '\n' || line[i] == '\0') {
				continue;
			} else if (line[i] >= '0' && line[i] <= '9') {
				// since it only involves 1 or 2, we don't need to go more.
				stage = line[i] - '0';
				// printf("stage number: %d\n", stage);
				break;
			}
		}
		break;
	}
	fclose(f);
	return stage;
}

int main(int argc, char const *argv[])
{
	int stage = process_argv(argc, argv);
	if (stage == 1) {
		// printf("Begin stage 1\n");
		FILE *f1 = fopen("stage1.r0.out.log", "wb");
		if (f1 == NULL) {
			perror("Stage 1 error in creating r0.out.log file!\n");
			exit(EXIT_FAILURE);
		}
		// build UDP socket in primary
		int sockfd;
		char buffer[MAXLINE];
		struct sockaddr_in primaddr, secaddr;
		// creating socket file descriptor
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket creation failed!\n");
			exit(EXIT_FAILURE);
		}
		memset(&primaddr, '\0', sizeof(primaddr));
		memset(&secaddr, '\0', sizeof(secaddr));
		// fill primary router information
		primaddr.sin_family = AF_INET;
		primaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		primaddr.sin_port = htons(0);
		// bind the socket with the primary router address
		if ((bind(sockfd, (const struct sockaddr *)&primaddr, sizeof(primaddr))) < 0) {
			perror("stage 1 primary bind failed!\n");
			exit(EXIT_FAILURE);
		}
		unsigned int length = sizeof(primaddr);
		if ((getsockname(sockfd, (struct sockaddr *)&primaddr, (socklen_t *)&length)) == -1) {
			perror("stage 1 primary getsockname failed!\n");
			exit(EXIT_FAILURE);
		}
		printf("primary's port number: %d\n", primaddr.sin_port);
		
		primary_port = primaddr.sin_port;
		int pid = fork();
		if (pid == 0) {
			// second router
			FILE *f2 = fopen("stage1.r1.out.log", "wb");
			if (f2 == NULL) {
				perror("stage 1 r1.out.log file creation fails!\n");
				exit(EXIT_FAILURE);
			}
			int sockfd2;
			char buf[MAXLINE];
			struct sockaddr_in primaddr, secaddr;
			if ((sockfd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
				perror("stage 1 second router socket failed!\n");
				exit(EXIT_FAILURE);
			}
			memset(&primaddr, '\0', sizeof(primaddr));
			memset(&secaddr, '\0', sizeof(secaddr));
			primaddr.sin_family = AF_INET;
			primaddr.sin_port = primary_port;
			primaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			// htonl(INADDR_ANY)  inet_addr("127.0.0.1")
			secaddr.sin_family = AF_INET;
			secaddr.sin_port = htons(0);
			secaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			if ((bind(sockfd2, (const struct sockaddr *)&secaddr, sizeof(secaddr))) < 0) {
				perror("stage 1 second router bind failed!\n");
				exit(EXIT_FAILURE);
			}
			unsigned int leng = sizeof(secaddr);
			if ((getsockname(sockfd2, (struct sockaddr *)&secaddr, (socklen_t *)&leng)) == -1) {
				perror("stage 1 second getsockname failed!\n");
				exit(EXIT_FAILURE);
			}
			int secPid = getpid();
			// printf("pid of second router: %d\n", secPid);
			// printf("The port of second router: %d\n", secaddr.sin_port);
			fprintf(f2, "router: 1, pid: %d, port: %d\n", secPid, secaddr.sin_port);
			snprintf(buf, MAXLINE, "%d", secPid);
			sendto(sockfd2, buf, MAXLINE, 0, (struct sockaddr *)&primaddr, sizeof(primaddr));
			close(sockfd2);
			fprintf(f2, "router 1 closed\n");
			fclose(f2);
			exit(1);
		}
		socklen_t len = sizeof(secaddr);
		recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&secaddr, &len);
		char *ptr;
		long sec_pid;
		sec_pid = strtol(buffer, &ptr, 10);
		printf("pid got at primary: %ld\n", sec_pid);
		printf("second port got at primary: %d\n", secaddr.sin_port);
		fprintf(f1, "primary port: %d\n", primaddr.sin_port);
		fprintf(f1, "router: 1, pid: %ld, port: %d\n", sec_pid, secaddr.sin_port);
		close(sockfd);
		fprintf(f1, "router 0 closed\n");
		fclose(f1);
		exit(0);
	} else {
		// stage 2
		// build UDP socket in primary
		int sockfd;
		char buffer[MAXLINE];
		struct sockaddr_in primaddr, secaddr;
		// creating socket file descriptor
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket creation failed!\n");
			exit(EXIT_FAILURE);
		}
		memset(&primaddr, '\0', sizeof(primaddr));
		memset(&secaddr, '\0', sizeof(secaddr));
		// fill primary router information
		primaddr.sin_family = AF_INET;
		primaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		primaddr.sin_port = htons(0);
		// bind the socket with the primary router address
		if ((bind(sockfd, (const struct sockaddr *)&primaddr, sizeof(primaddr))) < 0) {
			perror("stage 1 primary bind failed!\n");
			exit(EXIT_FAILURE);
		}
		unsigned int length = sizeof(primaddr);
		if ((getsockname(sockfd, (struct sockaddr *)&primaddr, (socklen_t *)&length)) == -1) {
			perror("stage 1 primary getsockname failed!\n");
			exit(EXIT_FAILURE);
		}
		
		// fprintf(f1, "whihc pid write it: %d\n", getpid());
		primary_port = primaddr.sin_port;
		int pid = fork();
		if (pid == 0) {
			// second router
			/* Here we need to:
			1. prepare to receive ICMP ECHO packet from primary router
				via UDP.
			2. After received from primary router, should generate an ICMP
				ECHO reply and send it back to primary router via UDP.*/
			int sockfd2;
			char buf[MAXLINE];
			struct sockaddr_in primaddr, secaddr;
			if ((sockfd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
				perror("stage 1 second router socket failed!\n");
				exit(EXIT_FAILURE);
			}
			memset(&primaddr, '\0', sizeof(primaddr));
			memset(&secaddr, '\0', sizeof(secaddr));
			primaddr.sin_family = AF_INET;
			primaddr.sin_port = primary_port;
			printf("primary's port got at second: %d\n", primary_port);
			primaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			secaddr.sin_family = AF_INET;
			secaddr.sin_port = htons(0);
			secaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			if ((bind(sockfd2, (const struct sockaddr *)&secaddr, sizeof(secaddr))) < 0) {
				perror("stage 1 second router bind failed!\n");
				exit(EXIT_FAILURE);
			}
			unsigned int leng = sizeof(secaddr);
			if ((getsockname(sockfd2, (struct sockaddr *)&secaddr, (socklen_t *)&leng)) == -1) {
				perror("stage 1 second getsockname failed!\n");
				exit(EXIT_FAILURE);
			}
			int secPid = getpid();
			printf("second's pid: %d\n", secPid);
			printf("second's port number: %d\n", secaddr.sin_port);
			FILE *f2 = fopen("stage2.r1.out.log", "wb");
			if (f2 == NULL) {
				perror("stage 1 r1.out.log file creation fails!\n");
				exit(EXIT_FAILURE);
			}
			fprintf(f2, "router: 1, pid: %d, port: %d\n", secPid, secaddr.sin_port);
			snprintf(buf, MAXLINE, "%d", secPid);
			sendto(sockfd2, buf, MAXLINE, 0, (struct sockaddr *)&primaddr, sizeof(primaddr));
			while (1) {
				// printf("stage 2 second recv from primary\n");
				char new_buf[2048];
				socklen_t new_len = sizeof(primaddr);
				int rece_byte = recvfrom(sockfd2, new_buf, sizeof(new_buf), 0, (struct sockaddr *)
					&primaddr, &new_len);
				printf("rece bytes from primary: %d\n", rece_byte);
				// we need judge if new_buf is "disconnect"
				char *stop_msg = "disconnect";
				if (strcmp(new_buf, stop_msg) == 0) {
					break;
				}
				struct ip *ip = (struct ip *)new_buf;
				char src_ip[15], dst_ip[15];
				sprintf(src_ip, "%s", inet_ntoa(ip->ip_src));
				sprintf(dst_ip, "%s", inet_ntoa(ip->ip_dst));
				
				int ip_head_len = ip->ip_hl<<2;
				struct icmp *icmp = (struct icmp *)(new_buf + ip_head_len);
				fprintf(f2, "ICMP from port: %d, src: %s, dst: %s, type: %d\n", 
						primaddr.sin_port, src_ip, dst_ip, icmp->icmp_type);

				struct in_addr tmp = ip->ip_dst;
				ip->ip_dst = ip->ip_src;
				ip->ip_src = tmp;
				icmp->icmp_type = 0;
				icmp->icmp_cksum = 0;
				icmp->icmp_cksum = checksum(new_buf + ip_head_len, rece_byte - ip_head_len);

				int send_bytes;
				if ((send_bytes = sendto(sockfd2, new_buf, rece_byte, 0, (struct sockaddr *)&primaddr, 
					sizeof(primaddr))) < 0) 
				{
					perror("Stage 2 failed to send from second to primary!\n");
					exit(EXIT_FAILURE);
				}
				printf("stage 2 sent bytes from second: %d\n", send_bytes);
			}
			close(sockfd2);
			fprintf(f2, "router 1 closed\n");
			// printf("have reached fclose(f2)\n");
			fclose(f2);
			exit(1);
		} 
		socklen_t len = sizeof(secaddr);
		recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&secaddr, &len);
		char *ptr;
		long sec_pid;
		sec_pid = strtol(buffer, &ptr, 10);
		FILE *f1 = fopen("stage2.r0.out.log", "wb");
		if (f1 == NULL) {
			perror("Stage 1 error in creating r0.out.log file!\n");
			exit(EXIT_FAILURE);
		}
		fprintf(f1, "primary port: %d\n", primaddr.sin_port);
		fprintf(f1, "router: 1, pid: %ld, port: %d\n", sec_pid, secaddr.sin_port);
		tunnel_reader(sockfd, f1, secaddr);
		close(sockfd);
		fprintf(f1, "router 0 closed\n");
		fclose(f1);
		exit(0);
	}
	return 0;
}