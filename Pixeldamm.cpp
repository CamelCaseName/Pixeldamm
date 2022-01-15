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
#include <queue>
using namespace std;
//#include "F:/Lenny/source/repos/Pixelbach/Pixelbach/Pixelbach.h"

#define PORT "1234"    // the port users will be connecting to
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

//exit function
void my_local_handler(sig_atomic_t s) {
    for (int j = 0; j < 24576; j += 192) {
        int row = j / 192;
        for (int i = 0; i < 192; i++) {
            *(bufAddr + i + j) = 0;
        }
    }          
    usleep(10000);
    printf("bye bye\n");
    exit(0);
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

    //exit handling
    struct sigaction sigIntHandler {};

    sigIntHandler.sa_handler = my_local_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    // create thread
    thread threaded(createPanel);

    // set scheduling of created thread
    threadd::setScheduling(threaded, SCHED_RR, 99);

    pthread_t buffer_thread_id = threaded.native_handle();

    threaded.detach();

    sleep(1);

    //extract buffer address
    cout << "buffer adress (main): ";
    dirty.open("address");
    dirty >> hex >> tester;
    bufAddr = (uint32_t*)tester;
    cout << (uint32_t*)bufAddr << endl;

    std::cout << "filling buffer with test pattern" << std::endl;
    for (int j = 0; j < 24576; j += 192) {
        int row = j / 192;
        for (int i = 0; i < 192; i++) {
            //*(bufAddr + i + j) = (((row >> 1) & 31) << 11) + ((i & 31) << 5) + ((192 - i) & 31);
            *(bufAddr + i + j) = 2 << 11;
        }
    }

    /*

    int x = 0;

    while (cin){
            usleep(100000);

            for (int j = 0; j < 24576; j += 192) {
                int row = j / 192;
                for (int i = 0; i < 192; i++) {
                    *(bufAddr + i + j) = (((row >> 1) & 31) << 11) + (((i + x) & 31)<< 5) + ((192 - i + x)  & 31);
                    //*(bufAddr + i + j) = 0;
                }
            }

            if(x<192) x++;
            else x = 0;
    }

    pthread_cancel(buffer_thread_id);

    my_local_handler(0);

    */

    int opt = true;
    int master_socket, addrlen, new_socket, client_socket[MAXUSERS],
        max_clients = MAXUSERS, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;
    string orders[MAXUSERS];

    char buffer[1024];  //data buffer of 1K  

    //set of socket descriptors  
    fd_set readfds{};

    //a message  
    //char* message = "Pixeldamm running on a PI4B, hello there! \r\n";

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
    address.sin_port = htons(1234);

    //bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %s \n", PORT);

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
                    //printf("Host disconnected , ip %s , port %d \n",
                    //	inet_ntoa(address.sin_addr), ntohs(address.sin_port));

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


                //TODO:
                // den command nicht zu früh interpretieren
                //if (orders[i].find_first_of('\n')) {


                std::queue<std::string> command_queue{};

                //send pixel info at adress		
                char* pch;
                pch = strtok((char*)orders[i].c_str(), " \n");
                while (pch != NULL) {
                    command_queue.push(pch);
                    pch = strtok(NULL, " ");
                }

                //clear orders causealready split into queue
                //printf("string: %s\n\r", orders[i].c_str());
                orders[i].clear();

                int x = 0, y = 0;

                //iterate through queue and act on elements
                while (command_queue.size()) {
                    if (command_queue.front()[0] == 'P') {
                        //remove identifier element
                        command_queue.pop();

                        //try parsing coords
                        //x coord                      
                        if (isxdigit((int)command_queue.front()[0])) {
                            try {
                                x = stoi(command_queue.front());
                            }
                            catch (std::invalid_argument e) {
                                y = 0;
                            }
                            catch (std::out_of_range e) {
                                y = 0;
                            }
                            catch (exception e) {
                                printf("caught exception %s when doing stoi(%s)\n", e.what(), command_queue.front());
                            }
                            command_queue.pop();
                        }

                        //y coord
                        if (isxdigit((int)command_queue.front()[0])) {
                            try {
                                y = stoi(command_queue.front());
                            }
                            catch (std::out_of_range e) {
                                y = 0;
                            }
                            catch (exception e) {
                                printf("caught exception %s when doing stoi(%s)\n", e.what(), command_queue.front());
                            }
                            command_queue.pop();
                        }

                        //colour if there is any requested, else return the color from the coord
                        if (isxdigit((int)command_queue.front()[0])) {
                            if (command_queue.front().length() < 7) {
                                try {
                                    //set pixel to colour
                                    int c = stoi(command_queue.front(), 0, 16);
                                    //cout << "changing pixel @ " << x << " " << y << " to " << c << endl;
                                    pixie.setPixel(x, y, c >> 16, (c >> 8) & 0xFF, c & 0xFF);
                                }
                                catch (std::out_of_range e) {
                                    y = 0;
                                }
                                catch (exception e) {
                                    printf("caught exception %s when doing stoi(%s)\n", e.what(), command_queue.front());
                                }
                            }
                            else {
                                try {
                                    //set pixel alpha
                                    long c = stol(command_queue.front(), 0, 16);
                                    //cout << "cpa @ " << x << " " << y << " to " << c << endl;
                                    //double alpha = (double)(c & 255);
                                    pixie.setPixelAlpha(x, y, c >> 24, (c >> 16) & 0xFF, (c >> 8) & 0xFF, 1);
                                }
                                catch (std::out_of_range e) {
                                    y = 0;
                                }
                                catch (exception e) {
                                    printf("caught exception %s when doing stoi(%s)\n", e.what(), command_queue.front());
                                }
                            }
                            command_queue.pop();
                        }
                        //return colour at coord
                        else {
                            string temp = "PX ";
                            temp += to_string(x);
                            temp += " ";
                            temp += to_string(y);
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
                    }
                    //send help
                    else if (command_queue.front()[0] == 'H') {
                        send(sd, help.c_str(), help.length(), 0);
                        //string tempo = orders[i].substr(orders[i].find('\n'));
                        //orders[i] = tempo; 
                        command_queue.pop();
                    }

                    //send size
                    else if (command_queue.front()[0] == 'S') {
                        send(sd, "SIZE 192 128\n\r", 15, 0);
                        //string tempo = orders[i].substr(orders[i].find('\n'));
                        //orders[i] = tempo; 
                        command_queue.pop();
                    }

                    //scrap command
                    else {
                        command_queue.pop();
                    }
                    //}
                }
                //string tempo = orders[i].substr(orders[i].find_first_not_of('\n'));
                //if (tempo.length()) orders[i] = tempo;
            }
        }
    }



    return 0;
}