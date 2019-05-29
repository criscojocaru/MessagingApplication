#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <iostream>
#include "helpers.h"

using namespace std;

void usage(char *file){
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

vector<string> split(char *phrase, string delimiter){
    vector<string> list;
    string s = string(phrase);
    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos) {
        token = s.substr(0, pos);
        list.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    list.push_back(s);
    return list;
}

int main(int argc, char *argv[]){
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 4) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

    // se trimite la server ID-ul clientului
	n = send(sockfd, argv[1], strlen(argv[1]), 0);
	DIE(n < 0, "send");

	while (1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
	  		// se citeste de la tastatura comanda dorita de client
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			// clientul se deconecteaza
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
			
			// se trimite comanda data de client catre server
			n = send(sockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");
		}

		// se primeste mesaj de la server
		if (FD_ISSET(sockfd, &tmp_fds)) {

			memset(buffer, 0 , BUFLEN);
			n = recv(sockfd, buffer, BUFLEN, 0);
			DIE(n < 0, "recv");
			printf("%s\n", buffer);

			// daca s-a primit exit de la server, clientul se deconecteaza
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
		}
	}

	close(sockfd);
	return 0;
}
