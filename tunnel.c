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

// tunnel set up
int tun_alloc(char *dev, int flags) {

	struct ifreq ifr;
	int fd, err;
	char *clonedev = "/dev/net/tun";

	/*
	dev: the name of an interface (or '\0')
	flags: such as IFF_TUN
	*/

	/*open the clone device*/
	if ((fd = open(clonedev, O_RDWR)) < 0) {
		perror("Opending /dev/net/tun fails\n");
		return fd;
	}

	/*preparation of the struct ifr, of type "struct ifreq */
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags;

	if (*dev) {
		/* if device name was specified, put it in the structure; ow, the kernel
		will try to allocate the "next" device of the specified type */
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	/* try to create the device */
	if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
		close(fd);
		return err;
	}

	/*
	if the operation is successful, write back the name of the interface to the variable
	to the variable "dev", so the caller can know it.
	*/
	strcpy(dev, ifr.ifr_name);

	/* this is the special file descripor that the caller will use to talk with
	the virtual interface */
	return fd;
}

int tunnel_reader(int udp_fd, FILE *f_r0, struct sockaddr_in secaddr) {
	char tun_name[IFNAMSIZ];
	char buffer[2048];

	/* coonect to the tunnel interface */
	strcpy(tun_name, "tun1");
	int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);

	if (tun_fd < 0) {
		perror("Open tunnel interface fails\n");
		exit(1);
	}

	/* This loop reads packets from the tunnel interface, use select() here to
	replace the while loop */
	
	fd_set readfds;
	int max_fd = (tun_fd > udp_fd) ? tun_fd : udp_fd;
	struct timeval timeout;
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(udp_fd, &readfds);
		FD_SET(tun_fd, &readfds);
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;
		/* Now read data coming from the tunnel */
		int activity;
		activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
		if (activity < 0) {
			perror("select error!\n");
			exit(EXIT_FAILURE);
			break;
		} else if (activity == 0) {
			printf("timeout!\n");
			char stop_msg[12] = "disconnect";
			int send_stop_msg;
			if ((send_stop_msg = sendto(udp_fd, stop_msg, 12, 0, (struct sockaddr *)&secaddr,
				sizeof(secaddr))) < 0) 
			{
				perror("Disconnect msg sent by primary fails!\n");
				exit(EXIT_FAILURE);
			}
			printf("len of stop msg sent: %d\n", send_stop_msg);
			break;
		} else {
			if (FD_ISSET(udp_fd, &readfds)) {
				printf("Has go into stage 2 from second part!\n");
				int read_bytes;
				socklen_t len = sizeof(secaddr);
			// get ICMP echo reply packet from secondary router via UDP
				if ((read_bytes = recvfrom(udp_fd, buffer, sizeof(buffer), 0, 
					(struct sockaddr *)&secaddr, &len)) < 0) 
				{
					perror("Stage 2 failed to receive in primary from second!\n");
					exit(EXIT_FAILURE);
				} else {
				// analyze ICMP echo reply packet
					printf("Jump into stage 2 second to primary part!\n");
					printf("Read bytes from second: %d\n", read_bytes);
					struct ip *ip = (struct ip *)buffer;
					char src_ip[15], dst_ip[15];
					sprintf(src_ip, "%s", inet_ntoa(ip->ip_src));
					sprintf(dst_ip, "%s", inet_ntoa(ip->ip_dst));

					int ip_head_len = ip->ip_hl<<2;
					struct icmp *icmp = (struct icmp *)(buffer + ip_head_len);
					fprintf(f_r0, "ICMP from port: %d, src: %s, dst: %s, type: %d\n", 
						secaddr.sin_port, src_ip, dst_ip, icmp->icmp_type);
					printf("ICMP from port: %d, src: %s, dst: %s, type: %d\n", 
						secaddr.sin_port, src_ip, dst_ip, icmp->icmp_type);
					int write_to_tunnel;
					if ((write_to_tunnel = write(tun_fd, buffer, read_bytes)) < 0) {
						perror("fails to write back to tunnel!\n");
						exit(EXIT_FAILURE);
					}
					printf("Read from udp, send to tunnel, packet length: %d\n", write_to_tunnel);
				}
			}
			if (FD_ISSET(tun_fd, &readfds)) {
			// get ICMP echo packet (ping) from tunnel
			/* receive and analyze the packet */
				int nread;
				if ((nread = read(tun_fd, buffer, sizeof(buffer))) < 0) {
					perror("Failure reading from tunnel interface!\n");
					// close(tun_fd);
					exit(EXIT_FAILURE);
				} else {
				// analyze ICMP echo packet
				// printf("have read something from tunnel!\n");
					printf("nread: %d\n", nread);
					struct ip *ip = (struct ip *)buffer;
					char src_ip[15], dst_ip[15];
					sprintf(src_ip, "%s", inet_ntoa(ip->ip_src));
					sprintf(dst_ip, "%s", inet_ntoa(ip->ip_dst));
					int ip_head_len = ip->ip_hl<<2;
					struct icmp *icmp = (struct icmp *)(buffer + ip_head_len);
					if (icmp->icmp_type == 8) {
						fprintf(f_r0, "ICMP from tunnel, src: %s, dst: %s, type: %d\n", 
							src_ip, dst_ip, icmp->icmp_type);
						int send_bytes;
						if ((send_bytes = sendto(udp_fd, buffer, nread, 0, (struct sockaddr *)&secaddr,
							sizeof(secaddr))) < 0) 
						{
							perror("stage 2 primary sendto second fails!\n");
							exit(EXIT_FAILURE);
						}
						printf("stage 2 primary sendto second: %d\n", send_bytes);
					}
				}
			}
		}
	}
	return 0;
}

unsigned short checksum(char *addr, short count) {
	/* Compute Internet checksum for "count" bytes
	beginning at location "addr" */
	register long sum = 0;
	while (count > 1) {
		sum += *(unsigned short *)addr;
		addr += 2;
		count -= 2;
	}

	/* Add left-over byte, if any */
	if (count > 0) {
		sum += *(unsigned char *)addr;
	}

	/* Fold 32 bit sum to 16 bit */
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (unsigned short)~sum;
}