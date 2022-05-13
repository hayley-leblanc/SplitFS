#include "nv_common.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include<time.h>

#include "remote.h"
#include "fileops_nvp.h"
#include "splitfs_server.h"

void* server_listen(void* args);
void* server_connect(void* args);
int handle_metadata_notif(struct ll_node *node);
int handle_client_request(struct ll_node *node);
int handle_pwrite(struct ll_node* node, struct remote_request request);
int handle_pread(struct ll_node* node, struct remote_request request);
int XOR_Decode(int x, int y);
int XOR_Encode(int x, int y);

// void server_listen(int sock_fd, struct addrinfo *addr_info);
// int remote_create(struct remote_request *request);
// int remote_write(struct remote_request *request);
// int remote_read(struct remote_request *request);
// int remote_open(struct remote_request *request);
// int remote_close(struct remote_request *request);
// int set_up_listen(int sock_fd, struct addrinfo *addr_info);

pthread_t server_cxn_thread, server_select_thread;
pthread_mutex_t fdset_lock;
fd_set fdset;
int connected_peers = 0;
int metadata_server_fd;
char fd_to_name[2000][256];

void* server_thread_start(void *arg) {
    int accept_socket, sock_fd, ret;
    struct addrinfo hints;
	struct addrinfo *result, *rp;
    int opt = 1;
	struct config_options conf_opts;
    int ip_protocol = AF_INET; // IPv4
	int transport_protocol = SOCK_STREAM; // TCP
	pthread_attr_t server_cxn_thread_attr, server_select_thread_attr;

	FD_ZERO(&fdset);

	DEBUG("reading configuration\n");
	ret = parse_config(&conf_opts, "config", _hub_find_fileop("posix")->FCLOSE, _hub_find_fileop("posix")->FOPEN);
	if (ret < 0) {
		DEBUG("unable to read configuration options\n");
		assert(0);
	}

	DEBUG("connecting to metadata server\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ip_protocol;
	hints.ai_socktype = transport_protocol;
	hints.ai_protocol = 0;
	// TODO: multiple possible metadata server IPs?
	ret = getaddrinfo(conf_opts.metadata_server_ips[0], conf_opts.metadata_server_port, &hints, &result);
	if (ret < 0) {
		DEBUG("getaddrinfo on %s failed: %s\n", conf_opts.metadata_server_ips[0], strerror(errno));
		assert(0);
	}

	metadata_server_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (metadata_server_fd < 0) {
		DEBUG("socket failed: %s\n", strerror(errno));
		assert(0);
	}

	// allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(metadata_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		// return res;
		assert(0);
	}

	ret = connect(metadata_server_fd, result->ai_addr, result->ai_addrlen);
	if (ret != -1) {
		DEBUG("now connected to metadata server %s port %s\n", conf_opts.metadata_server_ips[0], conf_opts.metadata_server_port);
	} else {
		perror("connect");
		DEBUG("unable to connect to metadata server\n");
		_hub_find_fileop("posix")->CLOSE(metadata_server_fd);
		assert(0);
	}

	pthread_mutex_lock(&fdset_lock);
	FD_SET(metadata_server_fd, &fdset);
	new_peer_fd_node(metadata_server_fd, METADATA_FD);
	connected_peers++;
	pthread_mutex_unlock(&fdset_lock);

	DEBUG("metadata server fd is %d\n", metadata_server_fd);

	// set up threads to listen for client connections and to select on incoming cxns
	// TODO: clean up nicely if something goes wrong

	ret = pthread_mutex_init(&fdset_lock, NULL);
	if (ret < 0) {
		perror("pthread_mutex_init");
		return NULL;
	}

	ret = pthread_attr_init(&server_cxn_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_init");
		return NULL;
	}
	ret = pthread_create(&server_cxn_thread, &server_cxn_thread_attr, server_connect, &conf_opts);
	if (ret != 0) {
        perror("pthread_create");
        return NULL;
    }
	ret = pthread_attr_destroy(&server_cxn_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
		return NULL;
	}

	ret = pthread_attr_init(&server_select_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_init");
		return NULL;
	}
	ret = pthread_create(&server_select_thread, &server_select_thread_attr, server_listen, &conf_opts);
	if (ret != 0) {
        perror("pthread_create");
        return NULL;
    }
	ret = pthread_attr_destroy(&server_select_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
		return NULL;
	}

	pthread_join(server_select_thread, NULL);
	pthread_join(server_cxn_thread, NULL);
	close(metadata_server_fd);

	return NULL;
}

void* server_connect(void* args) {
	struct addrinfo hints;
    struct addrinfo *result;
	struct config_options *conf_opts = (struct config_options*)args;
    int ip_protocol = AF_INET;
    int transport_protocol = SOCK_STREAM;
	int sock_fd, ret, cxn_socket;
	int opt = 1;

	// set up a socket to listen for incoming client connections
	memset(&hints, 0, sizeof(hints));
	memset(&result, 0, sizeof(result));
	hints.ai_family = ip_protocol;
	hints.ai_socktype = transport_protocol;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(NULL, conf_opts->splitfs_server_port, &hints, &result);
	if (ret < 0) {
		perror("getaddrinfo");
        return NULL;
	}

	sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock_fd < 0) {
		DEBUG("socket\n");
		return NULL;
	}

	// allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		return NULL;
	}

	// bind the socket to the local address and port so we can accept connections on it
	// ret = bind(sock_fd, (struct sockaddr*)&my_addr, addrlen);
	ret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
	if (ret < 0) {
		DEBUG("bind failed: %s\n", strerror(errno));
		// return ret;
		assert(0);
	}

	ret = listen(sock_fd, 8);
    if (ret < 0) {
        perror("listen");
        return NULL;
    }

	// listen for incoming client connections
	
	while(1) {
		cxn_socket = accept(sock_fd, result->ai_addr, &result->ai_addrlen);
		if (cxn_socket < 0) {
			perror("accept");
			assert(0);
		}
		DEBUG("connected to client with fd %d\n", cxn_socket);
		pthread_mutex_lock(&fdset_lock);
		FD_SET(cxn_socket, &fdset);
		new_peer_fd_node(cxn_socket, CLIENT_FD);
		connected_peers++;
		pthread_mutex_unlock(&fdset_lock);

		// struct ll_node* cur = peer_fd_list_head;
		// printf("current fds: ");
		// while (cur != NULL) {
		// 	printf("%d ", cur->fd);
		// 	cur = cur->next;
		// }
		// printf("\n");
	}

	// TODO: clean stuff up if something fails
	return NULL;
}

void* server_listen(void* args) {
    struct timeval tv;
    int ret, nfds = 0;
    fd_set fdset_copy;
    struct remote_request request;
	struct ll_node* cur;
	bool done = false;

    while(1) {
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        pthread_mutex_lock(&fdset_lock);
        FD_ZERO(&fdset_copy);
		cur = peer_fd_list_head;
		// printf("current fds: ");
		while (cur != NULL) {
			// printf("%d ", cur->fd);
			if (cur->fd > nfds) {
				nfds = cur->fd;
			}
			cur = cur->next;
		}
		// printf("\n");
        for (int i = 0; i <= nfds; i++) {
            if (FD_ISSET(i, &fdset)) {
                FD_SET(i, &fdset_copy);
            }
        }
        pthread_mutex_unlock(&fdset_lock);
        ret = select(nfds+1, &fdset_copy, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
        } else {
            // // determine which fd is ready
            // // pthread_mutex_lock(&fdset_lock);
			// cur = peer_fd_list_head;
			// while (cur != NULL) {
			// 	if (FD_ISSET(cur->fd, &fdset_copy) && cur->fd == metadata_server_fd) {
			// 		printf("METADATA SERVER SET\n");
			// 		// fd can either be for a connection with a client or with 
			// 		// the metadata server
			// 		// these functions handle requests and also disconnection
			// 		switch (cur->type) {
			// 			case METADATA_FD:
			// 				DEBUG("metadata fd\n");
			// 				ret = handle_metadata_notif(cur);
			// 				if (ret < 0) {
			// 					DEBUG("failed reading metadata server notification");
			// 				}
			// 				done = true;
			// 				break;
			// 			case CLIENT_FD:
			// 				DEBUG("client fd\n");
			// 				ret = handle_client_request(cur);
			// 				if (ret < 0) {
			// 					DEBUG("failed handling client request\n");
			// 				}
			// 				break;
			// 		}
			// 		break;
			// 	}
			// 	cur = cur->next;
			// }

			// if (!done) {
				// printf("checking client fds\n");
				cur = peer_fd_list_head;
				while (cur != NULL) {
					if (FD_ISSET(cur->fd, &fdset_copy)) {
						// fd can either be for a connection with a client or with 
						// the metadata server
						// these functions handle requests and also disconnection
						switch (cur->type) {
							case METADATA_FD:
								DEBUG("metadata fd\n");
								ret = handle_metadata_notif(cur);
								if (ret < 0) {
									DEBUG("failed reading metadata server notification");
								}
								break;
							case CLIENT_FD:
								DEBUG("client fd %d\n", cur->fd);
								ret = handle_client_request(cur);
								if (ret < 0) {
									DEBUG("failed handling client request\n");
								}
								break;
						}
						break;
					}
					cur = cur->next;
				// }
			}

			// pthread_mutex_unlock(&fdset_lock);
        }
    }
	return NULL;
}

int handle_metadata_notif(struct ll_node *node) {
	int ret, bytes_read, local_file_fd;
	struct remote_request notif;
	struct remote_response response;
	
	bytes_read = read_from_socket(node->fd, &notif, sizeof(struct remote_request));
	if (bytes_read < sizeof(struct remote_request)) {
		return -1;
	}
	switch(notif.type) {
		case METADATA_WRITE_NOTIF:
			DEBUG("metadata write notif\n");
			// 1. open or create the file
			local_file_fd = open(notif.file_path, O_CREAT | O_WRONLY, 777);
			if (local_file_fd < 0) {
				perror("open");
				return local_file_fd;
			}

			strcpy(fd_to_name[local_file_fd],notif.file_path);
			printf("fd_to_name local file_path SAAMAJA %s\n\n\n\n", fd_to_name[local_file_fd]);
			

			// 2. save the file descriptor
			new_file_fd_node(local_file_fd, notif.fd);
			DEBUG("added local fd %d to record for remote fd %d\n", local_file_fd, notif.fd);

			response.fd = local_file_fd;
			response.return_value = local_file_fd;
			response.type = METADATA_WRITE_NOTIF;
			DEBUG("sending write ack\n");
			ret = write(node->fd, &response, sizeof(struct remote_response));
			if (write < 0) {
				perror("write");
				return ret;
			}
			DEBUG("sent write ack\n");
			break;
		case METADATA_READ_NOTIF:
			DEBUG("metadata read notif\n");
			local_file_fd = open(notif.file_path, O_RDONLY);
			if (local_file_fd < 0) {
				perror("open");
				return local_file_fd;
			}
			// 2. save the file descriptor
			new_file_fd_node(local_file_fd, notif.fd);
			DEBUG("added local fd %d to record for remote fd %d\n", local_file_fd, notif.fd);
			response.fd = local_file_fd;
			response.return_value = local_file_fd;
			response.type = METADATA_READ_NOTIF;
			DEBUG("sending read ack to %d\n", node->fd);
			ret = write(node->fd, &response, sizeof(struct remote_response));
			if (ret < 0) {
				perror("write");
				return ret;
			}
			DEBUG("sent read ack\n");
			break;
		default:
			DEBUG("unrecognized type %d\n", notif.type);

	}

	return 0;
}

int handle_client_request(struct ll_node *node) {
	struct remote_request request;
	int bytes_read, ret;

	DEBUG("getting client request\n");
	// receive request with metadata about write - offset, length, etc.
	bytes_read = read_from_socket(node->fd, &request, sizeof(struct remote_request));
	if (bytes_read < sizeof(struct remote_request)) {
		return -1;
	}
	DEBUG("got client request\n");

	switch(request.type) {
		case PWRITE:
			ret = handle_pwrite(node, request);
			if (ret < 0) {
				return ret;
			}
			DEBUG("finishing up pwrite\n");
			pthread_mutex_lock(&fdset_lock);
			FD_CLR(node->fd, &fdset);
			close(node->fd);
			delete_peer_fd_node(node->fd);
			connected_peers--;
			pthread_mutex_unlock(&fdset_lock);
			DEBUG("done handling pwrite, disconnected from client\n");
			break;
		case PREAD:
			ret = handle_pread(node, request);
			if (ret < 0) {
				return ret;
			}
			DEBUG("finishing up pread\n");
			pthread_mutex_lock(&fdset_lock);
			FD_CLR(node->fd, &fdset);
			close(node->fd);
			delete_peer_fd_node(node->fd);
			connected_peers--;
			pthread_mutex_unlock(&fdset_lock);
			DEBUG("done handling pread, disconnected from client\n");
			break;
	}

	return 0;
}

int handle_pwrite(struct ll_node* node, struct remote_request request) {
	int ret, remote_fd, local_fd = 0;
	int retA, retB, retP, retQ, ret1, ret2, ret3;
	struct remote_response response, responseA, responseB, responseP, responseQ, response1, response2, response3;
	struct ll_node *cur;
	char *data_buf;

	// find the local fd to write to
	remote_fd = request.fd;
	cur = file_fd_list_head;
	while (cur != NULL) {
		if (cur->remote_fd == remote_fd) {
			local_fd = cur->fd;
			break;
		}
		cur = cur->next;
	}
	if (cur == NULL || local_fd == 0) {
		DEBUG("requested file is not open\n");
		response.type = PWRITE;
		response.fd = request.fd;
		response.return_value = -1;

		DEBUG("sending response to metadata server\n");
		ret = write(metadata_server_fd, &response, sizeof(struct remote_response));
		if (ret < 0) {
			DEBUG("failed writing response to metadata server\n");
		}
		DEBUG("sent response to metadata server\n");
		return -1;
	}

	// allocate space to receive the incoming data
	data_buf = malloc(request.count);
	if (data_buf == NULL) {
		perror("malloc");
		return -1;
	}

	// receive data
	ret = read_from_socket(node->fd, data_buf, request.count);
	if (ret < 0) {
		return ret;
	}

	printf("trying out replication - saamaja %s \n\n\n\n", request.file_path);
	srand(time(0));
	int replication_erasure_flag = rand();
	/*
	if(replication_erasure_flag % 2 == 0)
	{
		//replication starts here
		//file 1
		char onefile[256];
		int one_local_file_fd=0;
		snprintf(onefile, sizeof(onefile), "%s", fd_to_name[local_fd]);
    	onefile[(strlen(onefile))-4] = '\0'; 	
    	strcat(onefile, "-1.txt");
		
		one_local_file_fd = _nvp_OPEN(onefile, O_CREAT | O_RDWR, 777);
    	if (one_local_file_fd < 0)
			printf("saamaja fd one error\n");
		
		ret1 = pwrite(one_local_file_fd, data_buf, strlen(data_buf), 0);
		
		printf("MORNING trying to write file contents one back SAAMAJA  = %d\n\n\n\n", ret1);
		
		response1.type = PWRITE;
		response1.fd = one_local_file_fd;
		response1.return_value = ret1;
		
		ret1 = write(metadata_server_fd, &response1, sizeof(struct remote_response));
		
		printf("TRIED TO WRITE TO METADATA SERVER \n\n\n\n", ret1);
		if (ret1 < 0)
			DEBUG("failed writing response to metadata server 1 file\n");
		DEBUG("sent response to metadata server 1 file\n");
		
		delete_file_fd_node(one_local_file_fd);
		
		printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
		
		//file 2
		char twofile[256];
		int two_local_file_fd=0;
		snprintf(twofile, sizeof(twofile), "%s", fd_to_name[local_fd]);
    	twofile[(strlen(twofile))-4] = '\0'; 	
    	strcat(twofile, "-2.txt");
		printf("SAAMAJA what is twofile - saamaja %s \n\n\n\n", twofile);
    	two_local_file_fd = _nvp_OPEN(twofile, O_CREAT | O_RDWR, 777);
    	if (two_local_file_fd < 0)
			printf("saamaja fd two error\n");

		ret2 = pwrite(two_local_file_fd, data_buf, strlen(data_buf), 0);
		
		printf("MORNING trying to write file contents one back SAAMAJA  = %d\n\n\n\n", ret2);
		
		response2.type = PWRITE;
		response2.fd = two_local_file_fd;
		response2.return_value = ret2;
		ret2 = write(metadata_server_fd, &response2, sizeof(struct remote_response));
		
		printf("TRIED TO WRITE TO METADATA SERVER \n\n\n\n", ret2);
		
		if (ret2 < 0)
			DEBUG("failed writing response to metadata server 2 file\n");
		
		DEBUG("sent response to metadata server 2 file\n");
		
		delete_file_fd_node(two_local_file_fd);
		
		printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
		
		//file 3
		char threefile[256];
		int three_local_file_fd=0;
		snprintf(threefile, sizeof(threefile), "%s", fd_to_name[local_fd]);
    	threefile[(strlen(threefile))-4] = '\0'; 	
    	strcat(threefile, "-3.txt");
		printf("SAAMAJA what is threefile - saamaja %s \n\n\n\n", threefile);
    	three_local_file_fd = _nvp_OPEN(threefile, O_CREAT | O_RDWR, 777);
    	if (three_local_file_fd < 0)
			printf("saamaja fd three error\n");

		ret3 = pwrite(three_local_file_fd, data_buf, strlen(data_buf), 0);
		printf("MORNING trying to write file contents three back SAAMAJA  = %d\n\n\n\n", ret3);
		response3.type = PWRITE;
		response3.fd = three_local_file_fd;
		response3.return_value = ret3;
		ret3 = write(metadata_server_fd, &response3, sizeof(struct remote_response));
		printf("TRIED TO WRITE TO METADATA SERVER \n\n\n\n", ret3);
		if (ret3 < 0)
			DEBUG("failed writing response to metadata server 3 file\n");

		DEBUG("sent response to metadata server 3 file\n");
		delete_file_fd_node(three_local_file_fd);
		printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
		replication_erasure_flag = 1;
	}*/
	
	if(1)//else
	{
		//Erasure coding starts here
		char *first_file, *second_file, *P_file,*Q_file;
    	char data_buffer[256],firstfile[256], secondfile[256];
    	char finalParityP[256]="";
    	char finalParityQ[256]="";
    	char ch = ' ';
    	int mid_num;
    	int k=0;
    	strcpy(data_buffer,data_buf);
    	//finding the middle of the file
    	if(strlen(data_buffer)%2!=0)
        	strncat(data_buffer, &ch, 1);
    	mid_num=(strlen(data_buffer)/2)-1;
		printf("\nmid_num = %d\n",mid_num);

    	//splitting into two 
    	int counter = mid_num;
    	for(int i = 0;i<strlen(data_buffer);i++)
    	{
        	if(counter>-1)
        	{
        		firstfile[i] = data_buffer[i];
        		counter--;
        	}
	        else
	        {
	        	secondfile[k]=data_buffer[i];
	        	k++;
	        }
    	}

		firstfile[mid_num+1]='\0';
    	secondfile[mid_num+1]='\0';

	    //Calculating parity P
    
    	for(int i = 0;i<strlen(firstfile);i++)
    	{
        	int temp = XOR_Encode(firstfile[i],secondfile[i]);
        	char str[10];
         	sprintf(str, "%d", temp); 
         	char final[10]=""; 
         	if(strlen(str)<2)
         	{  
            	strcpy(final, "00");
            	strcat(final, str);
			
            	strcat(finalParityP, final);
            }
         	else if (strlen(str)<3)
         	{
            	strcpy(final, "0");
            	strcat(final, str);
			
            	strcat(finalParityP, final);
            }
         	else
             	strcat(finalParityP, str);

    	}

    	//Calculating parity Q
 
    	for(int i = 0;i<strlen(firstfile);i++)
    	{
        	int temp = XOR_Encode(firstfile[i],2*secondfile[i]);
        	char str[10];
         	sprintf(str, "%d", temp); 
         	char final[10]=""; 
         	if(strlen(str)<2)
         	{  
            	strcpy(final, "00");
            	strcat(final, str);
            	strcat(finalParityQ, final);
            }
         	else if (strlen(str)<3)
         	{
            	strcpy(final, "0");
            	strcat(final, str);
            	strcat(finalParityQ, final);
            }
         	else
             	strcat(finalParityQ, str);
    	}

    	first_file=&firstfile[0];
    	second_file= &secondfile[0];
    	P_file=&finalParityP[0];
    	Q_file=&finalParityQ[0];
	
		//Generatiing file A
		char Afile[256];
		int A_local_file_fd=0;
		snprintf(Afile, sizeof(Afile), "%s", fd_to_name[local_fd]);
    	Afile[(strlen(Afile))-4] = '\0'; 	
    	strcat(Afile, "-A.txt");
    	A_local_file_fd = _nvp_OPEN(Afile, O_CREAT | O_RDWR, 777);
    	if (A_local_file_fd < 0)
		printf("saamaja fd error\n");
    	printf("saamaja trying to create A file_fd=%d\n\n\n\n\n",A_local_file_fd);
	//printf("fd_to_name in other function fd SAAMAJA %d\n\n\n\n", local_fd);
	//printf("fd_to_name in other function filepath SAAMAJA %s\n\n\n\n", fd_to_name[local_fd]);
	retA = pwrite(A_local_file_fd, first_file, strlen(first_file), 0);
	printf("MORNING trying to write file contents A back SAAMAJA  = %d\n\n\n\n", retA);
	responseA.type = PWRITE;
	responseA.fd = A_local_file_fd;
	responseA.return_value = retA;
	retA = write(metadata_server_fd, &responseA, sizeof(struct remote_response));
	printf("TRIED TO WRITE TO METADATA SERVER \n\n\n\n", retA);
	if (ret < 0) {
		DEBUG("failed writing response to metadata server A file\n");
	}
	DEBUG("sent response to metadata server A file\n");
	delete_file_fd_node(A_local_file_fd);
	printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
	
	//Generatiing file B
	char Bfile[256];
	int B_local_file_fd=0;
	snprintf(Bfile, sizeof(Bfile), "%s", fd_to_name[local_fd]);
    	Bfile[(strlen(Bfile))-4] = '\0'; 	
    	strcat(Bfile, "-B.txt");
    	B_local_file_fd = _nvp_OPEN(Bfile, O_CREAT | O_RDWR, 777);
    	if (B_local_file_fd < 0) {
	printf("saamaja fd B error\n");
	}
    	printf("saamaja trying to create B file_fd=%d\n\n\n\n\n",B_local_file_fd);
	//printf("fd_to_name in other function fd SAAMAJA %d\n\n\n\n", local_fd);
	//printf("fd_to_name in other function filepath SAAMAJA %s\n\n\n\n", fd_to_name[local_fd]);
	retB = pwrite(B_local_file_fd, second_file, strlen(second_file), 0);
	printf("MORNING trying to write file contents A back SAAMAJA  = %d\n\n\n\n", retB);
	responseB.type = PWRITE;
	responseB.fd = B_local_file_fd;
	responseB.return_value = retB;
	retB = write(metadata_server_fd, &responseB, sizeof(struct remote_response));
	printf("TRIED TO WRITE TO METADATA SERVER \n\n\n\n", retB);
	if (ret < 0) {
		DEBUG("failed writing response to metadata server B file\n");
	}
	DEBUG("sent response to metadata server B file\n");
	delete_file_fd_node(B_local_file_fd);
	printf("TRIED TO DELETE FILE_FD_NODE B \n\n\n\n");
	
	//Generatiing file P
	char Pfile[256];
	int P_local_file_fd=0;
	snprintf(Pfile, sizeof(Pfile), "%s", fd_to_name[local_fd]);
    	Pfile[(strlen(Pfile))-4] = '\0'; 	
    	strcat(Pfile, "-P.txt");
    	P_local_file_fd = _nvp_OPEN(Pfile, O_CREAT | O_RDWR, 777);
    	if (P_local_file_fd < 0) {
	printf("saamaja fd P error\n");
	}
    	printf("saamaja trying to create P file_fd=%d\n\n\n\n\n",P_local_file_fd);
	//printf("fd_to_name in other function fd SAAMAJA %d\n\n\n\n", local_fd);
	//printf("fd_to_name in other function filepath SAAMAJA %s\n\n\n\n", fd_to_name[local_fd]);
	retP = pwrite(P_local_file_fd, P_file, strlen(P_file), 0);
	printf("MORNING trying to write file contents A back SAAMAJA  = %d\n\n\n\n", retP);
	responseP.type = PWRITE;
	responseP.fd = P_local_file_fd;
	responseP.return_value = retP;
	retP = write(metadata_server_fd, &responseP, sizeof(struct remote_response));
	printf("TRIED TO WRITE TO METADATA SERVER  P\n\n\n\n", retP);
	if (ret < 0) {
		DEBUG("failed writing response to metadata server P file\n");
	}
	DEBUG("sent response to metadata server P file\n");
	delete_file_fd_node(P_local_file_fd);
	printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
	
	
	//Generatiing file Q
	char Qfile[256];
	int Q_local_file_fd=0;
	snprintf(Qfile, sizeof(Qfile), "%s", fd_to_name[local_fd]);
    	Qfile[(strlen(Qfile))-4] = '\0'; 	
    	strcat(Qfile, "-Q.txt");
    	Q_local_file_fd = _nvp_OPEN(Qfile, O_CREAT | O_RDWR, 777);
    	if (Q_local_file_fd < 0) {
	printf("saamaja fd Q error\n");
	}
    	printf("saamaja trying to create Q file_fd=%d\n\n\n\n\n",Q_local_file_fd);
	//printf("fd_to_name in other function fd SAAMAJA %d\n\n\n\n", local_fd);
	//printf("fd_to_name in other function filepath SAAMAJA %s\n\n\n\n", fd_to_name[local_fd]);
	retQ = pwrite(Q_local_file_fd, Q_file, strlen(Q_file), 0);
	printf("MORNING trying to write file contents A back SAAMAJA  = %d\n\n\n\n", retQ);
	responseQ.type = PWRITE;
	responseQ.fd = Q_local_file_fd;
	responseQ.return_value = retQ;
	retQ = write(metadata_server_fd, &responseQ, sizeof(struct remote_response));
	printf("TRIED TO WRITE TO METADATA SERVER Q \n\n\n\n", retQ);
	if (ret < 0) {
		DEBUG("failed writing response to metadata server Q file\n");
	}
	DEBUG("sent response to metadata server Q file\n");
	delete_file_fd_node(Q_local_file_fd);
	printf("TRIED TO DELETE FILE_FD_NODE \n\n\n\n");
	replication_erasure_flag = 0;
	}

	// TODO: should this just be pwrite?
	ret = pwrite(local_fd, data_buf, request.count, request.offset);

	// send the response with the number of bytes written to the METADATA SERVER
	// so it can update metadata/coalesce multiple responses from multiple file servers
	// before sending a single final response to the client
	response.type = PWRITE;
	response.fd = request.fd;
	response.return_value = ret;

	DEBUG("sending response to metadata server\n");
	ret = write(metadata_server_fd, &response, sizeof(struct remote_response));
	if (ret < 0) {
		DEBUG("failed writing response to metadata server\n");
	}
	DEBUG("sent response to metadata server\n");


	// TODO: keep the file open for longer in case we want to read/write to it again soon
	delete_file_fd_node(local_fd);
	close(local_fd);
	DEBUG("done handling pwrite\n");
	return 0;
}

int handle_pread(struct ll_node* node, struct remote_request request) {
	char *data_buf;
	int ret, local_fd, remote_fd;
	struct ll_node *cur;
	struct remote_response response;
	DEBUG("handle pread\n");

	// find the local fd to write to
	remote_fd = request.fd;
	cur = file_fd_list_head;
	while (cur != NULL) {
		if (cur->remote_fd == remote_fd) {
			local_fd = cur->fd;
			break;
		}
		cur = cur->next;
	}
	if (cur == NULL || local_fd == 0) {
		DEBUG("requested file is not open\n");
		return -1;
	}

	// read the contents of the file and send them back to the client

	data_buf = malloc(request.count);
	if (data_buf == NULL) {
		perror("malloc");
		return -1;
	}
	DEBUG("reading %d bytes from fd %d\n", request.count, local_fd);
	ret = pread(local_fd, data_buf, request.count, request.offset);
	DEBUG("read %d bytes\n", ret);

	response.type = PREAD;
	response.fd = request.fd;
	response.return_value = ret;

	DEBUG("sending response to client\n");
	ret = write(node->fd, &response, sizeof(response));
	if (ret < 0) {
		DEBUG("failed writing response to client\n");
	}

	DEBUG("sending data to client\n");
	ret = write(node->fd, data_buf, ret);
	if (ret < 0) {
		DEBUG("failed writing data to client\n");
	}
	DEBUG("sent response to client\n");

	// TODO: keep the file open for longer in case we want to read/write to it again soon
	delete_file_fd_node(local_fd);
	close(local_fd);
	DEBUG("done handling pread\n");

	return 0;
}

int XOR_Decode(int x, int y)
{
    while (y != 0)
    {
        int borrow = (~x) & y;
        x = x ^ y;
        y = borrow << 1;
    }
	printf("\nXor decode = %d\n",x);
    return x;
}

int XOR_Encode(int x, int y)
{
    while (y != 0)
    {
        unsigned carry = x & y;
        x = x ^ y;
        y = carry << 1;
    }
	printf("\nXor encode = %d\n",x);
    return x;
}
