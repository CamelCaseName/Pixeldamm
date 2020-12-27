#include <Pixelbach.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <sstream>
#include <regex>
using namespace std;
//#include "F:/Lenny/source/repos/Pixelbach/Pixelbach/Pixelbach.h"

#define PORT "1337"    // the port users will be connecting to
#define MAXBUFLEN 100

#define MAXUSERS 30000

volatile uint32_t* bufAddr;
string help;

//display object
Pixelbach pixie(1);

class threadd : public thread {
public:
	threadd() {}
	static void setScheduling(thread& th, int policy, int priority) {
		sched_param sch_params;
		sch_params.sched_priority = priority;
		if (pthread_setschedparam(th.native_handle(), policy, &sch_params)) {
			cerr << "Failed to set Thread scheduling : " << strerror(errno) << endl;
		}
		cout << "Thread is executing at priority " << sch_params.sched_priority << endl;
	}
private:
	sched_param sch_params;
};

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* createPanel() {
	//dirty way of getting the buffer address
	ofstream outfile("address");
	outfile << pixie.retBA();
	outfile.close();
	usleep(100);
	cout << "buffer adress (thread): " << pixie.retBA() << endl;
	usleep(100);
	pixie.start();
	pthread_exit(NULL);
}

int main(void) {
	help += "Pixelflut defines four main commands that are always supported to get you started:\n\r";
	help += "HELP: Returns a short introductional help text.\n\rSIZE : Returns the size of the visible canvas in pixel as SIZE <w> <h>.\n\r";
	help += "PX <x> <y> Return the current color of a pixel as PX <x> <y> <rrggbb>.\n\r";
	help += "PX <x> <y> <rrggbb(aa)> : Draw a single pixel at position(x, y) with the specified hex color code.If the color code contains an alpha channel value, it is blended with the current color of the pixel.\n\r";
	help += "You can send multiple commands over the same connection by terminating each command with a single newline character(\\n)\n\r";
	fstream dirty;
	unsigned int tester;

	// create thread
	thread threaded(createPanel);

	// set scheduling of created thread
	threadd::setScheduling(threaded, SCHED_RR, 3);

	sleep(2);

	//extract buffer address
	cout << "buffer adress (main): ";
	dirty.open("address");
	dirty >> hex >> tester;
	bufAddr = (uint32_t*)tester;
	cout << (uint32_t*)bufAddr << endl;

	/*
	int sockfd;
	struct addrinfo hints, * servinfo, * p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("listener: waiting to recvfrom...\n");

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
		(struct sockaddr*)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}

	printf("listener: got packet from %s\n",
		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr*)&their_addr),
			s, sizeof s));

	//send help
	if (regex_search(buf, regex("((h|H)(e|E)(l|L)(p|P))"))) {
		if ((numbytes = sendto(sockfd, help.c_str(), help.length(), 0,
			(struct sockaddr*)&their_addr, p->ai_addrlen)) == -1) {
			perror("talker: sendto");
			exit(1);
		}
		else cout << "sent: " << numbytes  << " bytes of data back" << endl;
	}

	//send size
	else if (regex_search(buf, regex("((s|S)(i|I)(z|Z)(e|E)"))) {

	}

	//send pixel info at adress
	else if (regex_search(buf, regex("((p|P)(x|X)(.\d\d){2}"))) {

	}

	//set pixel info
	else if (regex_search(buf, regex("((p|P)(x|X)(.\d\d){2}.(\d|[A-F]){6})"))) {

	}

	//set pixel info with alpha channel
	else if (regex_search(buf, regex("((p|P)(x|X)(.\d\d){2}.(\d|[A-F]){8})"))) {

	}

	close(sockfd);
	*/
	int opt = true;
	int master_socket, addrlen, new_socket, client_socket[MAXUSERS],
		max_clients = MAXUSERS, activity, i, valread, sd;
	int max_sd;
	struct sockaddr_in address;
	string orders[MAXUSERS];

	char buffer[1025];  //data buffer of 1K  

	//set of socket descriptors  
	fd_set readfds;

	//a message  
	char* message = "Pixeldamm running on a PI4B, hello there! \r\n";

	//initialise all client_socket[] to 0 so not checked  
	for (i = 0; i < max_clients; i++) {
		client_socket[i] = 0;
	}

	//create a master socket  
	if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	//set master socket to allow multiple connections ,  
	//this is just a good habit, it will work without this  
	if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,
		sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	//type of socket created  
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(1337);

	//bind the socket to localhost port 8888  
	if (bind(master_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	printf("Listener on port %d \n", PORT);

	//try to specify maximum of 3 pending connections for the master socket  
	if (listen(master_socket, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	//accept the incoming connection  
	addrlen = sizeof(address);
	puts("Waiting for connections ...");

	while (true) {
		//clear the socket set  
		FD_ZERO(&readfds);

		//add master socket to set  
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		//add child sockets to set  
		for (i = 0; i < max_clients; i++) {
			//socket descriptor  
			sd = client_socket[i];

			//if valid socket descriptor then add to read list  
			if (sd > 0)
				FD_SET(sd, &readfds);

			//highest file descriptor number, need it for the select function  
			if (sd > max_sd)
				max_sd = sd;
		}

		//wait for an activity on one of the sockets , timeout is NULL ,  
		//so wait indefinitely  
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR)) {
			printf("select error");
		}

		//If something happened on the master socket ,  
		//then its an incoming connection  
		if (FD_ISSET(master_socket, &readfds)) {
			if ((new_socket = accept(master_socket,
				(struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
				perror("accept");
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands  
			printf("New connection , socket fd is %d , ip is : %s , port : %d\n",
				new_socket, inet_ntoa(address.sin_addr), ntohs
				(address.sin_port));
			/*
			//send new connection greeting message  
			if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
				perror("send");
			}
			*/
			

			//puts("Welcome message sent successfully");

			//add new socket to array of sockets  
			for (i = 0; i < max_clients; i++) {
				//if position is empty  
				if (client_socket[i] == 0) {
					client_socket[i] = new_socket;

					break;
				}
			}
		}

		//else its some IO operation on some other socket 
		for (i = 0; i < max_clients; i++) {

			sd = client_socket[i];

			if (FD_ISSET(sd, &readfds)) {
				//Check if it was for closing , and also read the  
				//incoming message  
				if ((valread = read(sd, buffer, 1024)) == 0) {
					//Somebody disconnected , get his details and print  
					getpeername(sd, (struct sockaddr*)&address, \
						(socklen_t*)&addrlen);
					printf("Host disconnected , ip %s , port %d \n",
						inet_ntoa(address.sin_addr), ntohs(address.sin_port));

					//clear buffer for this client
					orders[i].clear();

					//Close the socket and mark as 0 in list for reuse  
					close(sd);
					client_socket[i] = 0;
				}

				orders[i] += buffer;
				for (int j = 0; j < 1024; j++) {
					buffer[j] = '\0';
				}
				orders[i].erase(remove(orders[i].begin(), orders[i].end(), '\r'), orders[i].end());
				//cout << "buf:" << orders[i] << endl;
				//Echo back the message that came in  -- here i can get all stings that came in

				//send help
				if (regex_search(orders[i], regex("((h|H)(e|E)(l|L)(p|P)(\n)+\r*)"))) {
					send(sd, help.c_str(), help.length(), 0);
					//string tempo = orders[i].substr(orders[i].find('\n'));
					//orders[i] = tempo;
					orders[i].clear();
				}

				//send size
				if (regex_search(orders[i], regex("((s|S)(i|I)(z|Z)(e|E)(\n)+\r*)"))) {
					send(sd, "SIZE 192 128\n\r", 15, 0);
					//string tempo = orders[i].substr(orders[i].find('\n'));
					//orders[i] = tempo;
					orders[i].clear();
				}

				//send pixel info at adress
				if (regex_match(orders[i], regex("((p|P)(x|X).[0-9]+.[0-9]+\n+\r*)", regex_constants::ECMAScript))) {
					//cout << "getpixel detected" << endl;

					std::stringstream strem(orders[i]);
					std::string segment;
					std::vector<std::string> seglist;

					while (std::getline(strem, segment, ' ')) {
						seglist.push_back(segment);
					}
					int x = stoi(seglist[1]);
					//cout << stoi(seglist[1]) << endl;
					int y = stoi(seglist[2]);
					//cout << stoi(seglist[2]) << endl;
					string temp = "PX ";
					temp += seglist[1];
					temp += " ";
					temp += seglist[2];
					temp.pop_back();
					temp += " ";
					int rgb = pixie.getPixel(x, y);
					//cout << rgb << endl;
					temp += to_string(((rgb & 0xF800) >> 11));
					temp += " ";
					temp += to_string((rgb & 0x7E0) >> 5);
					temp += " ";
					temp += to_string(rgb & 0x1F);
					temp += "\n\r";
					send(sd, temp.c_str(), temp.length(), 0);



					orders[i].clear();
				}

				//set pixel color
				//cout << regex_search(orders[i], regex("((p|P)(x|X)(.\d{1,3}){2}.(\d|[A-F]|[a-f]){6}\n+\r*)")) << endl;
				if (regex_match(orders[i], regex("((p|P)(x|X).[0-9]+.[0-9]+.([0-9]|[A-F]|[a-f]){6}\n+\r*)", regex_constants::ECMAScript))) {
					//cout << "setpixel detected" << endl;
					std::stringstream strem(orders[i]);
					std::string segment;
					std::vector<std::string> seglist;

					while (std::getline(strem, segment, ' ')) {
						seglist.push_back(segment);
					}
					int x = stoi(seglist[1]);
					//cout << stoi(seglist[1]) << endl;
					int y = stoi(seglist[2]);
					//cout << stoi(seglist[2]) << endl;
					int c = stoi(seglist[3], NULL, 16);
					cout << "changing pixel @ " << x << " " << y << " to " << c << endl;
					pixie.setPixel(x, y, c >> 16, (c >> 8) & 0xFF, c & 0xFF);

					string tempo = orders[i].substr(orders[i].find_first_not_of('\n'));
					orders[i].clear();
					if (tempo.length()) orders[i] = tempo;
				}

				//set pixel color with alpha channel
				if (regex_search(orders[i], regex("((p|P)(x|X).[0-9]+.[0-9]+.([0-9]|[A-F]|[a-f]){8}\n+\r*)", regex_constants::ECMAScript))) {
					//cout << "setpixel detected" << endl;
					std::stringstream strem(orders[i]);
					std::string segment;
					std::vector<std::string> seglist;

					while (std::getline(strem, segment, ' ')) {
						seglist.push_back(segment);
					}
					int x = stoi(seglist[1]);
					//cout << stoi(seglist[1]) << endl;
					int y = stoi(seglist[2]);
					//cout << stoi(seglist[2]) << endl;
					long c = stol(seglist[3], NULL, 16);
					cout << "cpa @ " << x << " " << y << " to " << c << endl;
					double alpha = (double)(c & 255);
					pixie.setPixelAlpha(x, y, c >> 24, (c >> 16) & 0xFF, (c >> 8) & 0xFF, 1);

					string tempo = orders[i].substr(orders[i].find_first_not_of('\n'));
					orders[i].clear();
					if (tempo.length()) orders[i] = tempo;
				}
			}
		}
	}

	return 0;
}