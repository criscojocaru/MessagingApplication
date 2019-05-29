#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <set>

#include <math.h>
#include "helpers.h"

using namespace std;

#define ever ;;

#define BUFSIZE 2000

void usage(char *file){
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int maxim(int a, int b) {
	return a > b ? a : b;
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

int main(int argc, char *argv[]) {

	int sockfd, udp_sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	int sockets[100];
	int noSockets = 0;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	fd_set tmp2_fds;
	int fdmax;			// valoare maxima fd din multimea read_fds

	int optval;  		// flag value for setsockopt
	int clientlen; /* byte size of client's address */
	struct sockaddr_in clientaddr; /* client addr */
	char buf[BUFSIZE]; /* message buf */
	char *hostaddrp; /* dotted decimal host addr string */
	struct hostent *hostp; /* client host info */

	unordered_map <int, string> client_ids;
	unordered_map <string, set<string> > topics;
	unordered_map <string, string> SF;
	unordered_map <string, vector<string> > stored_messages;
	unordered_map <string, bool> clients;
	unordered_map <string, int> id_to_socket;

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// TCP socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "tcp socket");

	// UDP socket
	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_sockfd < 0, "udp socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	/* setsockopt: Handy debugging trick that lets 
	 * us rerun the server immediately after we kill it; 
	 * otherwise we have to wait about 20 secs. 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

	int flag = 1;
	int result = setsockopt(sockfd,      /* socket affected */
	                    IPPROTO_TCP,     /* set option at TCP level */
	                    TCP_NODELAY,     /* name of option */
	                    (char *) &flag,  /* the cast is historical cruft */
	                    sizeof(int));    /* length of option value */
	DIE(result < 0, "TCP_NODELAY");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// TCP bind
	ret = ::bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "tcp bind");

	// UDP bind
	ret = ::bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "udp bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	clientlen = sizeof(clientaddr);
	fdmax = maxim(sockfd, udp_sockfd);

	for (ever) {
		// se adauga socket-ul pentru UDP in multimea read_fds
		FD_SET(udp_sockfd, &read_fds);

		tmp_fds = read_fds;
		tmp2_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == STDIN_FILENO) {
					scanf("%s", buffer);
					if (!strcmp(buffer, "exit")) {
						for (int j = 0; j < noSockets; j++) {
							if (sockets[j] != -1) {
								n = send(sockets[j], buffer, strlen(buffer), 0);
								DIE(n < 0, "send");
							}
						}
						goto end;
					}
				} else if (i == udp_sockfd){
					// s-a primit o datagrama de la clientul UDP
					bzero(buf, BUFSIZE);
					n = recvfrom(udp_sockfd, buf, BUFSIZE, 0,
						(struct sockaddr *) &clientaddr, (unsigned int *)&clientlen);
					DIE(n < 0, "ERROR in recvfrom");

					// prelucrarea datagramei primite in functie de tip
					/* se primeste un buffer de maxim 1551 bytes:
					 * primii 50 de bytes - topic
					 * byte-ul 51 - tip de date
					 * urmatorii bytes (maxim 1500) - valoare
					 */
					string topic = "";
					string type = "";
					string value = "";
					for (i = 0; i < 50; i++)
						if (buf[i] != '\0')
							topic += buf[i];

						// tipul 0 - INT
						if (buf[50] == 0) {
							type = "INT";
							long long int_value = 0;

							i = 52;
							int_value = (uint8_t)buf[i + 3] | (uint32_t)((uint8_t)buf[i + 2]) << 8
							| (uint32_t)((uint8_t)buf[i + 1]) << 16 | (uint32_t)((uint8_t)buf[i]) << 24;

							// bitul de semn
							if (buf[51] == 1) {
								int_value = -int_value;
							}

							value = to_string(int_value);

						} else if (buf[50] == 1) {
							// tipul 1 - SHORT REAL
							type = "SHORT REAL";
							uint16_t int_value = 0;
							i = 51;
							int_value = (uint8_t)buf[i + 1] | (uint16_t)((uint8_t)buf[i]) << 8;
							value = to_string(int_value / 100.0);

						} else if (buf[50] == 2) {
							// tipul 2 - FLOAT
							type = "FLOAT";
							long long modul;
							i = 52;
							modul = (uint8_t)buf[i + 3] | (uint32_t)((uint8_t)buf[i + 2]) << 8
							| (uint32_t)((uint8_t)buf[i + 1]) << 16 | (uint32_t)((uint8_t)buf[i]) << 24;
							if (buf[51] == 1) {
								modul = -modul;
							}

							uint8_t power;
							i = 56;
							power = (uint8_t)buf[i];
							double d_value = modul * 1.0 / pow(10, power);
							value = to_string(d_value);

						} else if (buf[50] == 3) {
							// tipul 3 - STRING 
							type = "STRING";
							char s_value[1500];
							memset(s_value, 0, 1500);
							for (i = 51; i < n; i++)
								s_value[i - 51] = buf[i];
							s_value[n] = '\0';
							value = string(s_value);
						}

						//printf("\n");
				  		
						hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
							sizeof(clientaddr.sin_addr.s_addr), AF_INET);
						DIE(hostp == NULL, "ERROR on gethostbyaddr");
						hostaddrp = inet_ntoa(clientaddr.sin_addr);
						DIE(hostaddrp == NULL, "ERROR on inet_ntoa\n");

						stringstream msgToSend;
						msgToSend << hostaddrp << ":" << htons(clientaddr.sin_port) << " - "
						<< topic << " - " << type << " - " << value;
					
						memset(buf, 0, BUFSIZE);
						string snd = msgToSend.str(); 
						int len = snd.length() + 1;
						strcpy(buf, snd.c_str());

						for (auto client_id : topics[topic]) {
							// daca clientul este deconectat si are SF 1 pe topicul respectiv
							// dam store la datagrama
							if (clients[client_id] == false) {
								if (SF[topic + client_id] == "1") {
									stored_messages[topic + client_id].push_back(buf);
								}

							} else {
								// se trimite datagrama catre clientii activi si abonati la topic
								n = send(id_to_socket[client_id], buf, len, 0);
								DIE(n < 0, "send");

							}
						}
						i = udp_sockfd;

				} else if (i == sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					flag = 1;
					result = setsockopt(i,	/* socket affected */
						IPPROTO_TCP,     	/* set option at TCP level */
						TCP_NODELAY,     	/* name of option */
						(char *) &flag,  	/* the cast is historical cruft */
						sizeof(int));    	/* length of option value */
					DIE(result < 0, "TCP_NODELAY");

					sockets[noSockets] = newsockfd;
					noSockets++;

					// se primeste ID-ul noului client
					memset(buffer, 0, BUFLEN);
					n = recv(newsockfd, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					string client_id = string(buffer);

					// noul client a folosit un ID deja existent
					if(clients.count(client_id) != 0 && clients[client_id] == true){
						memset(buf, 0, BUFSIZE);
						strcpy(buf, "Error: Client ID already exists. Please exit and use a different ID.");
						// clientul este notificat sa foloseasca un ID diferit
						n = send(newsockfd, buf, strlen(buf), 0);
						DIE(n < 0, "send");
						
					} else {
						// noul client a folosit un ID disponibil
						client_ids[newsockfd] = client_id;
						id_to_socket[client_id] = newsockfd;
						clients[client_id] = true;

						// in cazul in care clientul s-a reconectat si avea SF = 1 setat pentru
						// un anumit topic, serverul ii va trimite mesajele pierdute in timpul
						// in care clientul a fost inactiv
						for (auto topic = topics.begin(); topic != topics.end(); topic++) {
							if ((topic->second).count(client_id) != 0) {

								if (SF[topic->first + client_id] == "1") {
									for (auto message : stored_messages[topic->first + client_id]) {
										memset(buf, 0, BUFSIZE);
										int len = message.length() + 1;
										strcpy(buf, message.c_str());
						
										n = send(newsockfd, buf, len, 0);
										DIE(n < 0, "send");
										usleep(3 * stored_messages[topic->first + client_id].size());
									}
									stored_messages[topic->first + client_id].clear();
								}
							}
						}

						cout << "New client " << client_ids[newsockfd] << " connected from " <<
						inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << ".\n";
					}

				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea clientului s-a inchis
						if(client_ids[i] != ""){
							cout << "Client " << client_ids[i] << " disconnected.\n";
						}

						clients[client_ids[i]] = false;
						client_ids.erase(i);

						id_to_socket.erase(client_ids[i]);

						close(i);
						for (int j = 0; j < noSockets; j++) {
							if (sockets[j] == i) {
								sockets[j] = -1;
							}
						}
						// se scoate din multimea de citire socketul inchis
						FD_CLR(i, &read_fds);

					} else {
						// clientul a trimis o comanda valida: subscribe / unsubscribe
						// sau o comanda invalida
						vector<string> listcommand = split(buffer, " ");
						string topic;
						int len;
						char *toSend;

						if(listcommand.size() < 2){
							topic = "Error: BAD request";
							len = topic.length() + 1;
						} else {
							topic = listcommand[1];
							
							if (listcommand[0] == "subscribe") {
								listcommand[2] = listcommand[2].substr(0, listcommand[2].size()-1);
								if (topics[topic].count(client_ids[i]) == 0) {								
									topics[topic].insert(client_ids[i]);
									SF[topic + client_ids[i]] = listcommand[2];

									topic = "subscribed " + topic;
									len = topic.length() + 1;
								} else {
									topic = "Error: client already subscribed " + topic;
									len = topic.length() + 1;
								}
							} else if (listcommand[0] == "unsubscribe") {
								listcommand[1] = listcommand[1].substr(0, listcommand[1].size()-1);
								if (topics[listcommand[1]].count(client_ids[i]) != 0) {
									topics[listcommand[1]].erase(client_ids[i]);
									topic = "unsubscribed " + topic;
									len = topic.length() + 1;
								} else {
									topic = "Error: client already not subscribed " + topic;
									len = topic.length() + 1;
								}
							} else {
								topic = "Error: BAD request";
								len = topic.length() + 1;
							}
						}

						toSend = new char[len];
						strcpy(toSend, topic.c_str());

						// serverul trimite catre client confirmarea primirii request-ului
						n = send(i, toSend, len, 0);
						DIE(n < 0, "send");
						
					}
				}
			}
		}
	}

end:
	close(sockfd);
	return 0;
}
