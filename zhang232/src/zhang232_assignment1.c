/**
 * @zhang232_assignment1
 * @author  Yuchen Zhang <zhang232@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "../include/global.h"
#include "../include/logger.h"

#define BACKLOG 5
#define STDIN 0
#define TRUE 1
#define CMD_SIZE 512
#define BUFFER_SIZE 1024
#define MSG_SIZE 256

int login(char *cmd, int client_socket);
int send_login_list(int client_socket);
int connect_to_host(char *server_ip, int server_port, int client_socket);
int establish_server(int port_num);
int establish_client(int port_num);
char* get_ip(void);
int get_localIP(char *res);
int author_print(char *cmd);
int ip_print(char* ip_addr, char *cmd);
int port_print(int socket,char *cmd, int the_port);
int add_to_clients(int fdaccept, struct sockaddr_in client_addr, int i, int client_port);
int process_cmd_from_client(char *msg, int sock_index);
int process_res_from_server(char *cmd);
int sort_list();
int check_if_login(char ip[INET_ADDRSTRLEN]);
int check_if_block(char ip[INET_ADDRSTRLEN],char sender_ip[INET_ADDRSTRLEN]);
int search_client_by_ip(char ip[INET_ADDRSTRLEN]);
int send_login_list_refresh(int client_socket);
int isValidAddr(char addr[INET_ADDRSTRLEN], char port[CMD_SIZE]);

int c_port;
int client_num = 0;

fd_set master_list, watch_list;

struct client
{
	int socket_id;
	int no;
	char hostname[128];
	char ip_addr[INET_ADDRSTRLEN];
	int port_no;
	char blockedList[10][INET_ADDRSTRLEN];
	int block_num;
	//struct client blocked_clients[4];
    int login_status;
    int send_msg_num;
    int recv_msg_num;
    char msg_buffer[100][BUFFER_SIZE];
	int buffered_msg;

}clients[BACKLOG];

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv){
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/

	if( argc != 3 ){

		printf("Usage: %s [s/c] [port]\n", argv[0]);
		exit(-1);

	}
	else{

		for(int i = 0; i < BACKLOG; i++){
			clients[i].buffered_msg = 0;
			clients[i].socket_id = 0;
			clients[i].block_num = 0;
			for(int j = 0; j<4;j++){

				
				bzero(clients[i].blockedList[j],INET_ADDRSTRLEN);

			}

		}
	}

	if( strcmp(argv[1],"s") == 0){
		
		int port = atoi(argv[2]);
		establish_server(port);

	}
	else if( strcmp(argv[1],"c") == 0){
		
		int port = atoi(argv[2]);
		establish_client(port);

	}

	return 0;
}

int establish_server(int port_num){

	printf("\nServer Running\n");
	int port, server_socket, head_socket, selret, sock_index, fdaccept = 0;
	int yes = 1;
	socklen_t caddr_len;
	struct sockaddr_in server_addr;
	struct client client_list[BACKLOG];
	

	// Socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(server_socket < 0)
		perror("Cannot create socket");

	port = port_num;
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);// the IP address of the machine on which the server is running
	server_addr.sin_port = htons(port);

	// Bind
	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
    	perror("Bind failed");

 	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));   

	// Listen
	if(listen(server_socket, BACKLOG) < 0)
    	perror("Unable to listen on port");
	
	/* Zero select FD sets */
    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);
    
    /* Register the listening socket */
    FD_SET(server_socket, &master_list);
    /* Register STDIN */
    FD_SET(STDIN, &master_list);
	
	head_socket = server_socket;
	char* ip_addr;
	ip_addr = get_ip();
    while(TRUE){
        memcpy(&watch_list, &master_list, sizeof(master_list));
        /* select() system call. This will BLOCK */
        selret = select(head_socket + 1, &watch_list, NULL, NULL, NULL);
        if(selret < 0)
            perror("select failed.");

        /* Check if we have sockets/STDIN to process */
        if(selret > 0){
            /* Loop through socket descriptors to check which ones are ready */
            for(sock_index=0; sock_index<=head_socket; sock_index+=1){

                if(FD_ISSET(sock_index, &watch_list)){

                    /* Check if new command on STDIN */
                    if (sock_index == STDIN){
                    	char *cmd = (char*) malloc(sizeof(char)*CMD_SIZE);

                    	memset(cmd, '\0', CMD_SIZE);
						if(fgets(cmd, CMD_SIZE-1, stdin) == NULL) //Mind the newline character that will be written to cmd
							exit(-1);

						printf("\nServer got: %s\n", cmd);
						cmd[strlen(cmd)-1]='\0';
						
						//Process PA1 commands here ...
						if(strcmp(cmd, "AUTHOR") == 0){

							author_print(cmd);

						}
						else if(strcmp(cmd, "IP") == 0){

							ip_print(ip_addr, cmd);

						}
						else if(strcmp(cmd, "PORT") == 0){

							port_print(server_socket, cmd, port_num);

						}
						else if(strcmp(cmd, "LIST") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
							for (int i = 0; i < client_num; i++){

								if(clients[i].login_status == 1){
						
									cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",clients[i].no,clients[i].hostname,clients[i].ip_addr,clients[i].port_no);

								}
							}
							cse4589_print_and_log("[%s:END]\n", cmd);  
						}
						else if(strcmp(cmd, "STATISTICS") == 0){

							cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
							for (int i = 0; i < client_num; i++){
								char status[25];
								bzero(status,25);
								if(clients[i].socket_id > 0){
									if(clients[i].login_status == 1){
										strcpy(status,"logged-in");
									}
									else if(clients[i].login_status == 0){
										strcpy(status,"logged-out");
									}
									else{
										continue;
									}
									cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", clients[i].no, clients[i].hostname, clients[i].send_msg_num, clients[i].recv_msg_num, status);
								}
							}
							cse4589_print_and_log("[%s:END]\n", cmd);  
						}
						else if(strncmp(cmd, "BLOCKED", 7)==0){

							char *p;
							p = strtok(cmd," ");
							if(p!=NULL){
								p = strtok(NULL," ");
							}else{
								cse4589_print_and_log("[%s:ERROR]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);
								return 0;

							}
							char check_b_ip[INET_ADDRSTRLEN];
							strcpy(check_b_ip,p);
							//printf("%s\n", check_b_ip);
							struct client blocklist[15];
							int n = 0;
							
							for (int i = 0; i < client_num; i++){
								// printf("%s\n", clients[i].ip_addr);
								// printf("%s\n", check_b_ip);
								// printf("%d\n",strcmp(clients[i].ip_addr,check_b_ip) );
								if(strcmp(clients[i].ip_addr,check_b_ip)==0){
									
									//printf("%d\n",2 );
									if(clients[i].block_num>0){
										//printf("%d\n",3 );
										for(int j = 0; j < clients[i].block_num;j++){
											printf("%s\n", clients[i].blockedList[j]);
											for (int m = 0; m < client_num; m++){
												// printf("%s\n", clients[m].ip_addr);
												// printf("%s\n", clients[i].blockedList[j]);
												// printf("%d\n",strcmp(clients[m].ip_addr,clients[i].blockedList[j]));
												if(strcmp(clients[m].ip_addr,clients[i].blockedList[j])==0){
													//cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",n+1,clients[m].hostname,clients[m].ip_addr,clients[m].port_no);													
													
													strcpy(blocklist[n].hostname, clients[m].hostname);
													strcpy(blocklist[n].ip_addr, clients[m].ip_addr);
													blocklist[n].port_no = clients[m].port_no;
													n++;
													
												}
											}
										}
									}
								}
								
							}
							struct client temp;
    						for (int i = 0; i < n; i++){
    							for (int j = i + 1; j < n; j++){
      								if(blocklist[i].port_no>blocklist[j].port_no){
       									temp = blocklist[i];
       									blocklist[i] = blocklist[j];
       									blocklist[j] = temp;
      
      								}
     							}
    						}
    						cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
    						for(int i = 0; i < n; i++){

    							cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",i+1,blocklist[i].hostname,blocklist[i].ip_addr,blocklist[i].port_no);

    						}

							cse4589_print_and_log("[%s:END]\n", cmd);  

						}

						free(cmd);
                    }
                    /* Check if new client is requesting connection */
                    else if(sock_index == server_socket){
                    	char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
						struct sockaddr_in client_addr;
                        caddr_len = sizeof(client_addr);
                        fdaccept = accept(server_socket, (struct sockaddr *)&client_addr, &caddr_len);
                        if(fdaccept < 0){
                            perror("Accept failed.");
						}
						printf("\nRemote Host connected!\n the fd is: %d\n",fdaccept);    
						 /* Add to watched socket list */
						FD_SET(fdaccept, &master_list);

						client_num = add_to_clients(fdaccept, client_addr, client_num, 0);
						
						free(buffer);
						if(fdaccept > head_socket) head_socket = fdaccept;
                    }
                    /* Read from existing clients */
                    else{
                        /* Initialize buffer to receieve response */
                        char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
                        memset(buffer, '\0', BUFFER_SIZE);

                        if(recv(sock_index, buffer, BUFFER_SIZE, 0) <= 0){
                            close(sock_index);
                            printf("Remote Host terminated connection!\n");
                            client_num--;

                            /* Remove from watched list */
                            FD_CLR(sock_index, &master_list);
                        }
                        else {
                        	//Process incoming data from existing clients here ...
                        	char *cmd = buffer;
	                       	printf("\nClient sent me: %s\n", cmd);
							process_cmd_from_client(cmd, sock_index);	                        	
							fflush(stdout);


                        }

                        free(buffer);
                    }
                }
            }
        }
    }

    return 0;
}

int establish_client(int port_num){

	printf("\nClient Running\n");
	c_port = port_num;
	struct client this_client;
	char host[128];
	char msg_port[MSG_SIZE];
	int head_socket, client_socket, selret, sock_index, server_fd, server_socket = 0;
	int yes = 1;
	socklen_t caddr_len;
	struct sockaddr_in client_addr, server_addr;
	struct hostent *hostinfo;

    

	

    /* Zero select FD sets */
    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);
    /* Register STDIN */
    FD_SET(STDIN, &master_list);
	
	head_socket = STDIN;

	char* ip_addr;
	ip_addr = get_ip();
	while(TRUE){

		memcpy(&watch_list, &master_list, sizeof(master_list));
        /* select() system call. This will BLOCK */
        selret = select(head_socket + 1, &watch_list, NULL, NULL, NULL);
        if(selret < 0)
            perror("select failed.");

        /* Check if we have sockets/STDIN to process */
        if(selret > 0){
            /* Loop through socket descriptors to check which ones are ready */
            for(sock_index = 0; sock_index <= head_socket; sock_index += 1){

                if(FD_ISSET(sock_index, &watch_list)){

                    /* Check if new command on STDIN */
                    if (sock_index == STDIN){
                    	char *cmd = (char*) malloc(sizeof(char)*CMD_SIZE);
                    	memset(cmd, '\0', CMD_SIZE);
						if(fgets(cmd, CMD_SIZE-1, stdin) == NULL) //Mind the newline character that will be written to cmd
							exit(-1);

						printf("\nClient got: %s\n", cmd);
						cmd[strlen(cmd)-1]='\0';

						if(strncmp(cmd, "LOGIN", 5) == 0){
							client_socket = socket(AF_INET, SOCK_STREAM, 0);

    						if(client_socket < 0){

        					perror("Failed to create socket");

    						}
							char msg_temp[MSG_SIZE];
							
							memset(msg_port, '\0', MSG_SIZE);
							memset(msg_temp, '\0', MSG_SIZE);
							strcat(msg_port,"PORT_SEND ");
							sprintf(msg_temp,"%d", port_num);
							strcat(msg_port,msg_temp);
							login(cmd,client_socket);
							FD_SET(client_socket, &master_list);
							if(client_socket > head_socket) head_socket = client_socket;
							if(send(client_socket, msg_port, strlen(msg_port),0) == strlen(msg_port)) 
								printf("Done in port send!\n");
							fflush(stdout);												
						}
						else if(strncmp(cmd, "SEND", 4) == 0)
						{
							int count = 0;
							char *p;
							char *words[1024];

							p = strtok(cmd," ");
	
							while( p != NULL){

								words[count] = p;
								count++;
    							p = strtok (NULL, " ");

							}
							if(count<2){
								cse4589_print_and_log("[%s:ERROR]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);	
								break;							
							}

							char target_client[128];
							bzero(target_client,128);
							char send_msg[MSG_SIZE];
							bzero(send_msg,MSG_SIZE);

							strcat(target_client,words[1]);
							if(count>=2){
								for(int i = 2;i<count;i++){
									strcat(send_msg,words[i]);
									strcat(send_msg," ");
								}
							}
							if(send_msg[strlen(send_msg)-1]==' '){
								send_msg[strlen(send_msg)-1]='\0';
							}
							

							char msg[BUFFER_SIZE];

							bzero(msg,BUFFER_SIZE);
							strcat(msg,"SEND ");
							strcat(msg,target_client);
							strcat(msg," ");
							strcat(msg,send_msg);
							
							//int target_socket = search_client_by_ip(target_client);
							int exists = 0;
							for(int i = 0;i<client_num;i++){
								if(strcmp(clients[i].ip_addr,target_client)==0){
									exists = 1;
									break;
								}
							}
							if(exists>0){
								if (send (client_socket, msg, strlen(msg), 0) > 0){
									printf("Done in SEND\n");
									cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
									cse4589_print_and_log("[%s:END]\n", cmd);
								}
							}else{
								cse4589_print_and_log("[%s:ERROR]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);

							}


							

						}
						else if(strcmp(cmd, "AUTHOR") == 0){

							author_print(cmd);

						}
						else if(strcmp(cmd, "IP") == 0){

							
							ip_print(ip_addr, cmd);

						}
						else if(strcmp(cmd, "PORT") == 0){

							port_print(client_socket, cmd, port_num);


						}
						else if(strcmp(cmd, "REFRESH") == 0){

							char *str = "REFRESH";
							if(send(client_socket,str, strlen(str), 0) == strlen(str)){

								cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);

							}

						}
						else if(strcmp(cmd, "LOGOUT") == 0){

							char *str = "LOGOUT";
							if(send(client_socket,str, strlen(str), 0) == strlen(str)){

								close(client_socket);
								FD_CLR(client_socket,&master_list);
								cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);

							}

						}
						else if(strcmp("EXIT",cmd) == 0){

							close(client_socket);
							exit(0);

						}
						else if(strcmp(cmd, "LIST") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
							
							for (int i = 0; i < client_num; i++){
							
									cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",clients[i].no,clients[i].hostname,clients[i].ip_addr,clients[i].port_no);
																
							}
							cse4589_print_and_log("[%s:END]\n", cmd);  
						}
						else if(strncmp(cmd, "BROADCAST", 9) == 0){
							int count = 0;
							char *p;
							char *word[1024];
							char send_msg[MSG_SIZE];
							bzero(send_msg,MSG_SIZE);

							p = strtok(cmd," ");
			
							while( p != NULL){
								
								word[count] = p;
								count++;
    							p = strtok (NULL, " ");

							}
							if(count>=1){
								for(int i = 1; i < count; i++){
									strcat(send_msg,word[i]);
									strcat(send_msg," ");
								}
							}
							char msg[MSG_SIZE];
							bzero(msg,MSG_SIZE);
							strcat(msg,"BROADCAST");
							strcat(msg," ");
							strcat(msg,send_msg);
							//int target_socket = search_client_by_ip(target_client);

							if (send (client_socket, msg, strlen(msg), 0) > 0){
								printf("Done in BROADCAST\n");
								cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);
							}
						}
						else if(strncmp(cmd, "BLOCK",5) == 0){
							char *p;
							char c_ip[INET_ADDRSTRLEN];
							int count = 0;
							bzero(c_ip,INET_ADDRSTRLEN);
							p = strtok(cmd," ");
							while(p != NULL){
								
								if(count==1)
									strcat(c_ip,p);
								count++;
    							p = strtok (NULL, " ");

							}
							char msg[MSG_SIZE];
							bzero(msg,MSG_SIZE);
							strcat(msg,"BLOCK_IP");
							strcat(msg," ");
							strcat(msg,c_ip);
							if (send (client_socket, msg, strlen(msg), 0) > 0){
								printf("Done in BLOCK\n");
								cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);
							}

						}
						else if(strncmp(cmd, "UNBLOCK",5) == 0){
							char *p;
							char c_ip[INET_ADDRSTRLEN];
							int count = 0;
							bzero(c_ip,INET_ADDRSTRLEN);
							p = strtok(cmd," ");
							while(p != NULL){
								
								if(count==1)
									strcat(c_ip,p);
								count++;
    							p = strtok (NULL, " ");

							}
							char msg[MSG_SIZE];
							bzero(msg,MSG_SIZE);
							strcat(msg,"UNBLOCK_IP");
							strcat(msg," ");
							strcat(msg,c_ip);
							if (send (client_socket, msg, strlen(msg), 0) > 0){
								printf("Done in UNBLOCK\n");
								cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
								cse4589_print_and_log("[%s:END]\n", cmd);
							}

						} 
        				
					}
					else{
                        /* Initialize buffer to receieve response */
                        char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
                        memset(buffer, '\0', BUFFER_SIZE);

                        int recv_bytes = recv(sock_index, buffer, BUFFER_SIZE, 0);
						char *cmd = buffer;
						if (recv_bytes > 0){
							//printf("\nServer sent me: %s\n", buffer);
                        	process_res_from_server(cmd);
						}

						fflush(stdout);
                        free(buffer);
                    }
                }
        	}
		}
	}
	
	return 0;
}

char* get_ip(void)
{
       int s;
       struct ifconf conf;
       struct ifreq *ifr;
       char buff[BUFFER_SIZE];
       int num;
       int i;

       s = socket(PF_INET, SOCK_DGRAM, 0);
       conf.ifc_len = BUFFER_SIZE;
       conf.ifc_buf = buff;

       ioctl(s, SIOCGIFCONF, &conf);
       num = conf.ifc_len / sizeof(struct ifreq);
       ifr = conf.ifc_req;

       for(i=0;i < num;i++)
       {
            struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);
            ioctl(s, SIOCGIFFLAGS, ifr);
           if(((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP))
            {
                char* ipaddr = inet_ntoa(sin->sin_addr);
                return ipaddr;
            }
            ifr++;
       }
}

int connect_to_host(char *server_ip, int server_port,int fdsocket){


    struct sockaddr_in remote_server_addr;
    bzero(&remote_server_addr, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &remote_server_addr.sin_addr);
    remote_server_addr.sin_port = htons(server_port);
	connect(fdsocket, (struct sockaddr*)&remote_server_addr, sizeof(remote_server_addr));
    // if(connect(fdsocket, (struct sockaddr*)&remote_server_addr, sizeof(remote_server_addr)) == -1 ){
    	
    //     cse4589_print_and_log("[%s:ERROR]\n", "LOGIN");
    //     cse4589_print_and_log("[%s:END]\n", "LOGIN");

    // }
    return fdsocket;
}

int isValidAddr(char addr[INET_ADDRSTRLEN], char port[CMD_SIZE]){
    int ret = 1;
    for(int i=0; i<CMD_SIZE;i++){
        if(addr[i] == '\0') break;
        if(addr[i] == '.') continue;
        int t = addr[i] - '0';
        if(t<0 || t>9) {
            ret = 0;
            break;
        }
    }

    for(int i=0; i<CMD_SIZE;i++){
        if(port[i] == '\0') break;
        int t = port[i] - '0';
        if(t<0 || t>9) {
            ret = 0;
            break;
        }
    }
    return ret;
}

int author_print(char *cmd){

	char your_ubit_name[128] = "zhang232";

	cse4589_print_and_log("[%s:SUCCESS]\n", cmd); 
	cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", your_ubit_name);
	cse4589_print_and_log("[%s:END]\n", cmd);
	
	return 0;
}

int ip_print(char* ip_addr, char *cmd){

	cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
	cse4589_print_and_log("IP:%s\n", ip_addr);
	cse4589_print_and_log("[%s:END]\n", cmd);

	return 0;
}

int port_print(int socket,char *cmd, int the_port){

	cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
	cse4589_print_and_log("PORT:%d\n", the_port);
	cse4589_print_and_log("[%s:END]\n", cmd);
    return 0;
}

int add_to_clients(int fdaccept, struct sockaddr_in client_addr, int i, int client_port){
	for(int j = 0; j < client_num; j++){
		if(strcmp(clients[j].ip_addr,inet_ntoa(client_addr.sin_addr))==0){
			return i;

			break;
		}
	}

	char host[1024];
	char service[20];
	clients[i].socket_id = fdaccept;
	clients[i].no = i + 1;
	getnameinfo(&client_addr, sizeof client_addr, host, sizeof host, service, sizeof service, 0);
	strcpy(clients[i].hostname, host);
	strcpy(clients[i].ip_addr, inet_ntoa(client_addr.sin_addr));
	clients[i].login_status = 1;
	clients[i].send_msg_num = 0;
	clients[i].recv_msg_num = 0;
	clients[i].block_num = 0;

	return i+1;
}

int login(char *cmd,int client_socket){

	
	int server = 0;
	int count = 0;
	char *p;
	char *words[10];
	

	p = strtok(cmd," ");
	
	while( p != NULL){

		words[count] = p;
		count++;
    	p = strtok (NULL, " ");

	}
	if(count<2){

        cse4589_print_and_log("[%s:ERROR]\n", "LOGIN");
        cse4589_print_and_log("[%s:END]\n", "LOGIN");
        return 0;
    }
    if(isValidAddr(words[1],words[2])==0){
    	cse4589_print_and_log("[%s:ERROR]\n", "LOGIN");
        cse4589_print_and_log("[%s:END]\n", "LOGIN");
        return 0;
    }
	char *login_address = words[1];
	int login_port = atoi(words[2]);
	
	server = connect_to_host(login_address, login_port, client_socket);

	return server;
}


int send_login_list(int client_socket){

	char login_msg[1024];
	char temp[1024];
	memset(login_msg,'\0',1024);
	memset(temp,'\0',1024);

	strcat(login_msg,"CLIST ");
	for(int i = 0;i < client_num;i++){
		if(clients[i].login_status>0){

			sprintf(temp, "%d", clients[i].no);
			strcat(login_msg, temp);
			memset(temp,'\0',1024);
			strcat(login_msg, ",");
			strcat(login_msg, clients[i].hostname);
			strcat(login_msg, ",");
			strcat(login_msg, clients[i].ip_addr);
			strcat(login_msg, ",");
			sprintf(temp, "%d", clients[i].port_no);
			strcat(login_msg, temp);
			strcat(login_msg, " ");

		}
	}
	//printf("print login_list: %s\n", login_msg);

	int len = send(client_socket, login_msg, sizeof(login_msg), 0);
	if (len > 0)
		printf("Done in list send\n");
	fflush(stdout);

	return 0;
}

int send_login_list_refresh(int client_socket){

	char login_msg[1024];
	char temp[1024];
	memset(login_msg,'\0',1024);
	memset(temp,'\0',1024);

	strcat(login_msg,"refresh ");
	for(int i = 0;i < client_num;i++){
		if(clients[i].login_status>0){

			sprintf(temp, "%d", clients[i].no);
			strcat(login_msg, temp);
			memset(temp,'\0',1024);
			strcat(login_msg, ",");
			strcat(login_msg, clients[i].hostname);
			strcat(login_msg, ",");
			strcat(login_msg, clients[i].ip_addr);
			strcat(login_msg, ",");
			sprintf(temp, "%d", clients[i].port_no);
			strcat(login_msg, temp);
			strcat(login_msg, " ");

		}
	}
	//printf("print login_list: %s\n", login_msg);

	int len = send(client_socket, login_msg, sizeof(login_msg), 0);
	if (len > 0)
		printf("Done in refresh list send\n");
	fflush(stdout);

	return 0;
}
 

int process_cmd_from_client(char *msg, int sock_index){

	char *sender_ip;
	int sender_no;
    for(int i =0; i<client_num; i++)
    {
        if(sock_index ==clients[i].socket_id)
        { 
           sender_ip = clients[i].ip_addr;
           sender_no = i;
           break;
        }
    }	
    
    char *cmd, c_ip[INET_ADDRSTRLEN], *token, c_msg[MSG_SIZE], send_buf[BUFFER_SIZE];

    bzero(send_buf,BUFFER_SIZE);
    bzero(c_msg,MSG_SIZE);
    bzero(c_ip,INET_ADDRSTRLEN);

    token = strtok(msg, " ");

    if(token != NULL)
    {      
        cmd = token;
        token = strtok(NULL, " ");
    }
    else{
    	cmd = msg;
    }

    printf("We get this CMD:%s\n", cmd);

    if (strcmp("SEND",cmd)==0){
    	int target_socket = 0;
    	int count = 0;
		char *words[10];

		while( token != NULL){

			words[count] = token;
			//printf ("%s\n", words[count]);
			count++;
    		token = strtok (NULL, " ");

		}
		if(count<1){
			cse4589_print_and_log("[RELAYED:ERROR]\n");
        	cse4589_print_and_log("[RELAYED:END]\n");
        	return 0;
		}

		if(count>=1){
			for(int i = 1; i < count; i++){
				strcat(c_msg,words[i]);
				strcat(c_msg," ");
			}
		}
		if(c_msg[strlen(c_msg)-1]==' '){
			c_msg[strlen(c_msg)-1]='\0';
		}
		// c_msg[strlen(c_msg)-1]='\0';
		strcpy(c_ip,words[0]);
		// strcpy(c_msg,words[1]);
		strcat(send_buf,"msg_send ");
		strcat(send_buf,sender_ip);		
        strcat(send_buf," ");
        strcat(send_buf,c_msg);

        //printf("now we got this msg:%s\n", send_buf);
        fflush(stdout);
        int n = 0;
        int if_login = check_if_login(c_ip);
        int if_block = check_if_block(c_ip,sender_ip);
        //printf("if_login:%d  if_block:%d\n",if_login,if_block);
		if(if_login == 1 && if_block == 0){
			target_socket = search_client_by_ip(c_ip);
			//printf("target_socket:%d\n",target_socket);
			if(target_socket > 0){
				if(send(target_socket, send_buf, strlen(send_buf),0)>0){
					//printf("Done!\n");
					n++;
					for (int i =0; i< client_num;i++){
						if(strcmp(clients[i].ip_addr,c_ip)==0){
							clients[i].recv_msg_num++;
							clients[sender_no].send_msg_num++;
							break;
						}
					}
					fflush(stdout);
				}
			}
		}else if(if_login == 0 && if_block == 0){
			if( target_socket = search_client_by_ip(c_ip)>0){
				for (int i =0; i< client_num;i++){
					if(strcmp(clients[i].ip_addr,c_ip)==0&&clients[i].buffered_msg<100){
						//bzero(clients[i].msg_buffer[clients[i].buffered_msg],BUFFER_SIZE);
						strcpy(clients[i].msg_buffer[clients[i].buffered_msg],send_buf);
						n++;
						clients[i].buffered_msg++;
						clients[sender_no].send_msg_num++;
						break;
					}
				}
			}
		}
		else{
        	cse4589_print_and_log("[RELAYED:ERROR]\n");
        	cse4589_print_and_log("[RELAYED:END]\n");
		}

		if(n > 0){
			cse4589_print_and_log("[RELAYED:SUCCESS]\n");
			cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender_ip, c_ip, c_msg);
			cse4589_print_and_log("[RELAYED:END]\n");
		}else{
			cse4589_print_and_log("[RELAYED:ERROR]\n");
			cse4589_print_and_log("[RELAYED:END]\n");
		}

		bzero(send_buf,BUFFER_SIZE);


    }
    else if (strcmp("BROADCAST",cmd) == 0){
    	int target_socket = 0;
    	char broadcast_msg[MSG_SIZE];
    	bzero(broadcast_msg,MSG_SIZE);
    	if( token != NULL){

			strcpy(broadcast_msg,token);
    		token = strtok (NULL, " ");

		}
		
		strcat(send_buf,"msg_broad ");
		strcat(send_buf,sender_ip);		
        strcat(send_buf," ");
        strcat(send_buf,broadcast_msg);
		//printf("%s\n",send_buf);
        int count = 0;
		for(int i = 0; i < client_num; i++){
			//if(strcmp(clients[i].ip_addr,sender_ip) != 0){
			if(i != sender_no){
				target_socket = clients[i].socket_id;
				//printf("if_login:%d\n",clients[i].login_status);
				//printf("if_block:%d\n",check_if_block(clients[i].ip_addr,sender_ip));
				if(clients[i].login_status == 1 && check_if_block(clients[i].ip_addr,sender_ip) == 0){
					if(send(target_socket, send_buf, strlen(send_buf),0)>0){
						//printf("Done!\n");
						count++;
						//for (int j =0; j< client_num;j++){
							//if(strcmp(clients[j].ip_addr,clients[i].ip_addr)==0){
						clients[i].recv_msg_num++;
						clients[sender_no].send_msg_num++;
								//break;
							//}
						//}
						fflush(stdout);
					}
				}
				else if(clients[i].login_status == 0 && check_if_block(clients[i].ip_addr,sender_ip) == 0){
					for (int m =0; m< client_num;m++){
						if(strcmp(clients[m].ip_addr,clients[i].ip_addr)==0&&clients[m].buffered_msg<100){
							//bzero(clients[m].msg_buffer[clients[m].buffered_msg],BUFFER_SIZE);
							strcpy(clients[m].msg_buffer[clients[m].buffered_msg],send_buf);
							count++;
							clients[m].buffered_msg++;
							clients[sender_no].send_msg_num++;
						}
					}
				}
			}
		}
		if(count>0){
			cse4589_print_and_log("[RELAYED:SUCCESS]\n");
			cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender_ip, "255.255.255.255", broadcast_msg);
			cse4589_print_and_log("[RELAYED:END]\n");
		}else{
			cse4589_print_and_log("[RELAYED:ERROR]\n");
			cse4589_print_and_log("[RELAYED:END]\n");
		}
	}
	else if (strcmp("LOGOUT",cmd) == 0){

    	for(int i = 0;i<client_num;i++)
        {
            if(sock_index == clients[i].socket_id)
            {
            	
                clients[i].login_status = 0;
                close(sock_index);
            	FD_CLR(sock_index,&master_list);
                break;
            }
        }

    }
    else if (strcmp("REFRESH",cmd) == 0){
    	for(int i = 0;i<client_num;i++)
        {
            if(sock_index == clients[i].socket_id)
            {
                sort_list();
                send_login_list_refresh(sock_index);
				break;
            }
        }
    }
    else if(strcmp("PORT_SEND",cmd) == 0){
		char temp[MSG_SIZE];	

		strcpy(temp,token);		
		int client_port = atoi(temp);
		
		for(int i = 0; i< client_num; i++){
		
			if(clients[i].socket_id == sock_index){
				clients[i].port_no = client_port;
				clients[i].login_status = 1;
				break;
			}
		
		}
		//add_to_clients(sock_index, client_addr, client_num, client_port);                   
        /* Send Login List to this Client*/
        sort_list();
		send_login_list(sock_index);
		// for(int i = 0; i < client_num; i++){
		// 	if (clients[i].socket_id == sock_index){
		if(clients[sender_no].buffered_msg > 0){
			//printf("buf_num:%d\n",clients[sender_no].buffered_msg);
			int buf_num = clients[sender_no].buffered_msg;
			for (int k = 0; k < buf_num; k++){
				// char message[MSG_SIZE];	
				// bzero(message,MSG_SIZE);
				// char *p;
				// char *sip;
				// char temp[1024];
				// strcpy(temp,clients[sender_no].msg_buffer[k]);
				// p = strtok(temp," ");
				// int count = 0;
				// char *words[10];

				// while( p != NULL){

				// 	words[count] = p;
				// 	if(count>1){
				// 		strcat(message,words[count]);
				// 		strcat(message," ");
				// 	}
				// 	count++;					
    // 				p = strtok (NULL, " ");

				// }
				// if(count<2){
				// 	cse4589_print_and_log("[RELAYED:ERROR]\n");
    //     			cse4589_print_and_log("[RELAYED:END]\n");
    //     			return 0;
				// }
				if(send(sock_index,clients[sender_no].msg_buffer[k],strlen(clients[sender_no].msg_buffer[k]),0)>0){
					//printf("send no.%d msg from %s to %s\r\n", k, sender_ip, c_ip);
					bzero(clients[sender_no].msg_buffer[k],BUFFER_SIZE);
					clients[sender_no].buffered_msg--;
					clients[sender_no].recv_msg_num++;
					usleep(5000);
					//cse4589_print_and_log("[RELAYED:SUCCESS]\n");
					// if(strcmp(words[0],"msg_send")==0){
					// 	cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", words[1], clients[sender_no].ip_addr, message);
					// }else{
					// 	cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", words[1], "255.255.255.255", message);
					// }
					// cse4589_print_and_log("[RELAYED:END]\n");	
				}
			}
			usleep(5000);
			send(sock_index,"LOGINSUCCESS",12,0);
		}else{
			usleep(5000);
			send(sock_index,"LOGINSUCCESS",12,0);
		}

	}
	else if(strcmp("BLOCK_IP",cmd) == 0){
		char c_ip[INET_ADDRSTRLEN];
		bzero(c_ip,INET_ADDRSTRLEN);
		if( token != NULL){

			strcat(c_ip,token);
    		token = strtok (NULL, " ");

		}
		for(int i = 0; i < client_num; i++){
			if (clients[i].socket_id == sock_index){
				strcat(clients[i].blockedList[clients[i].block_num],c_ip);
				clients[i].block_num++;
				break;
			}
		}

	}else if(strcmp("UNBLOCK_IP",cmd) == 0){
		char c_ip[INET_ADDRSTRLEN];
		bzero(c_ip,INET_ADDRSTRLEN),INET_ADDRSTRLEN;
		if( token != NULL){

			strcat(c_ip,token);
    		token = strtok (NULL, " ");

		}
		for(int i = 0; i < client_num; i++){
			if (clients[i].socket_id == sock_index){
				for(int j = 0; j<clients[i].block_num;j++){
					if (strcmp(clients[i].blockedList[j],c_ip)==0 ){
						bzero(clients[i].blockedList[j],INET_ADDRSTRLEN);
						
						break;
					}
				}
			}
			break;
		}
	}
}


int process_res_from_server(char *msg){
	char *res,*token;
	token = strtok(msg, " ");
	char from_ip[INET_ADDRSTRLEN], c_msg[MSG_SIZE], send_buf[BUFFER_SIZE];
	bzero(from_ip,INET_ADDRSTRLEN);
	bzero(c_msg,MSG_SIZE);
	bzero(send_buf,BUFFER_SIZE);

    if(token != NULL)
    {      
        res = token;
        token = strtok(NULL, " ");
    }

    //printf("res:%s\n",res);

	if (strcmp(res,"CLIST") == 0){
		client_num = 0;
    	char *temp[10];
		char *line, *p2;
		
    	while( token != NULL){

			temp[client_num] = token;
			//printf ("%s\n", temp[client_num]);
			client_num++;
    		token = strtok (NULL, " ");

		}
		//printf("cln:%d\n",client_num);
		for (int j = 0 ; j < client_num; j++){
			line = strtok(temp[j], ",");
			int i = 0;
			while(line != NULL){

				switch (i){

					 case 0: clients[j].no = atoi(line);
					 	clients[j].login_status = 1;
					 	i++;
					 	break;
								
					case 1: strcpy(clients[j].hostname,line);
						i++;
						break;
						
					case 2: strcpy(clients[j].ip_addr,line);
						i++;
						break;
								
					case 3: clients[j].port_no = atoi(line);
						i++;
						break;

                }	
                //printf("<token main before while :%s\r\n",line);
			 	line=strtok(NULL,",");		
			 	fflush(stdout);				
			}
		}
		
	}else if(strcmp(res,"msg_send") == 0){
		char *temp[10];
		int i = 0;
    	while( token != NULL){

			temp[i] = token;
			//printf ("%s\n", temp[i]);
			i++;
    		token = strtok (NULL, " ");

		}
		if(i<1){
			cse4589_print_and_log("[RECEIVED:ERROR]\n");
        	cse4589_print_and_log("[RECEIVED:END]\n");
        	return 0;
		}
		if(i>=1){
			for(int j = 1; j < i; j++){
				strcat(c_msg,temp[j]);
				strcat(c_msg," ");
			}
		}
		if(c_msg[strlen(c_msg)-1]==' '){
			c_msg[strlen(c_msg)-1]='\0';
		}
		// c_msg[strlen(c_msg)-1]='\0';

		strcpy(from_ip,temp[0]);
		//strcpy(c_msg,temp[1]);

		cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
		cse4589_print_and_log("msg from:%s\n[msg]:%s\n", from_ip, c_msg);
		cse4589_print_and_log("[RECEIVED:END]\n");

	}
	else if(strcmp(res,"refresh") == 0){
    	char *temp[10];
		char *line, *p2;
		client_num = 0;
    	while( token != NULL){

			temp[client_num] = token;
			printf ("%s\n", temp[client_num]);
			client_num++;
    		token = strtok (NULL, " ");

		}
		//printf("cln:%d\n",client_num);
		for (int j = 0 ; j < client_num; j++){
			line = strtok(temp[j], ",");
			int i = 0;
			while(line != NULL){

				switch (i){

					 case 0: clients[j].no = atoi(line);
					 	clients[j].login_status = 1;
					 	i++;
					 	break;
								
					case 1: strcpy(clients[j].hostname,line);
						i++;
						break;
						
					case 2: strcpy(clients[j].ip_addr,line);
						i++;
						break;
								
					case 3: clients[j].port_no = atoi(line);
						i++;
						break;

                }	
                //printf("<token main before while :%s\r\n",line);
			 	line=strtok(NULL,",");		
			 	fflush(stdout);				
			}
		}
	}
	else if(strcmp(res,"msg_broad") == 0){
		char *temp[10];
		int i = 0;
    	while( token != NULL){

			temp[i] = token;
			//printf ("%s\n", temp[i]);
			i++;
    		token = strtok (NULL, " ");

		}
		if(i<1){
			cse4589_print_and_log("[RECEIVED:ERROR]\n");
        	cse4589_print_and_log("[RECEIVED:END]\n");
        	return 0;
		}
		if(i>=1){
			for(int j = 1; j < i; j++){
				strcat(c_msg,temp[j]);
				strcat(c_msg," ");
			}
		}



		strcpy(from_ip,temp[0]);
		strcpy(c_msg,temp[1]);
		cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
		cse4589_print_and_log("msg from:%s\n[msg]:%s\n", from_ip, c_msg);
		cse4589_print_and_log("[RECEIVED:END]\n");
	}
	else if(strcmp(res,"msg_buffered") == 0){
		char *temp[10];
		int i = 0;
    	while( token != NULL){

			temp[client_num] = token;
			printf ("%s\n", temp[client_num]);
			i++;
    		token = strtok (NULL, " ");

		}

		strcat(from_ip,temp[0]);
		strcat(c_msg,temp[1]);
		cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
		cse4589_print_and_log("msg from:%s\n[msg]:%s\n", from_ip, c_msg);
		cse4589_print_and_log("[RECEIVED:END]\n");
 		cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
        cse4589_print_and_log("[%s:END]\n", "LOGIN");

	}else if(strcmp(res,"LOGINSUCCESS") == 0){

		cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
        cse4589_print_and_log("[%s:END]\n", "LOGIN");
	
	}
		
	

}

int check_if_block(char ip[INET_ADDRSTRLEN],char sender_ip[INET_ADDRSTRLEN]){

	for(int i = 0; i< client_num; i++){
		
		if(strcmp(clients[i].ip_addr,ip) == 0){

			for (int j = 0; j < clients[i].block_num; i++){

				if(strcmp(clients[i].blockedList[j],sender_ip) == 0){
					return 1;
					break;
				}

			}
			break;
		}


	}
	return 0;
}

int check_if_login(char ip[INET_ADDRSTRLEN]){

	for(int i = 0; i< client_num; i++){
		
		if(strcmp(clients[i].ip_addr,ip) == 0 && clients[i].login_status == 1){
			return 1;
			break;
		}
		
	}
	return 0;
}

int sort_list(){
	struct client temp;
	int temp_no;
    for (int i = 0; i < client_num; i++){
    	for (int j = i + 1; j < client_num; j++){
      		if(clients[i].port_no>clients[j].port_no){
       			temp = clients[i];
       			clients[i] = clients[j];
       			clients[j] = temp;
				clients[i].no = i+1;
				clients[j].no = j+1;
      		}
     	}
    }	
}

int search_client_by_ip(char ip[INET_ADDRSTRLEN]){
	int socket = 0;
	for(int i = 0; i< client_num; i++){
		
		if(strcmp(clients[i].ip_addr,ip) == 0){
			socket = clients[i].socket_id;
			return socket;
			break;
		}	
	}	
}