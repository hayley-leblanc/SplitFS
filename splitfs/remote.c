#include <string.h>

#include "remote.h"

int parse_config(
    struct config_options *conf_opts, 
    const char* config_path, 
    int (*fclose_fn)(FILE*),
    FILE* (*fopen_fn)(const char*, const char*)) 
{
    FILE* conf_file;
    char buf[BUFFER_SIZE];
    char ip_buffer[BUFFER_SIZE];
    int len, i;

    conf_file = fopen_fn(config_path, "r");
    if (conf_file == NULL) {
        perror("fopen");
        return -1;
    }

    memset(ip_buffer, 0, BUFFER_SIZE);

    while (fgets(buf, BUFFER_SIZE, conf_file)) {
        memset(ip_buffer, 0, BUFFER_SIZE);
        if (strncmp(buf, "metadata_server_ips", strlen("metadata_server_ips")) == 0) {
            len = strlen("metadata_server_ips");
            strncpy(ip_buffer, buf+len+1, BUFFER_SIZE-(len+1));
            parse_comma_separated(ip_buffer, conf_opts->metadata_server_ips);
        } else if (strncmp(buf, "metadata_server_port", strlen("metadata_server_port")) == 0) {
            len = strlen("metadata_server_port");
            strncpy(conf_opts->metadata_server_port, buf+len+1, 8);
            for (i = 0; i < 8; i++) {
                if (conf_opts->metadata_server_port[i] == ';') {
                    conf_opts->metadata_server_port[i] = '\0';
                }
            }
        } else if (strncmp(buf, "metadata_client_port", strlen("metadata_client_port")) == 0){
            len = strlen("metadata_client_port");
            strncpy(conf_opts->metadata_client_port, buf+len+1, 8);
            for (i = 0; i < 8; i++) {
                if (conf_opts->metadata_client_port[i] == ';') {
                    conf_opts->metadata_client_port[i] = '\0';
                }
            }
        } else if (strncmp(buf, "splitfs_server_ips", strlen("splitfs_server_ips")) == 0) {
            len = strlen("splitfs_server_ips");
            strncpy(ip_buffer, buf+len+1, BUFFER_SIZE-(len+1));
            parse_comma_separated(ip_buffer, conf_opts->splitfs_server_ips);
        } else if (strncmp(buf, "splitfs_server_port", strlen("splitfs_server_port")) == 0) {
            len = strlen("splitfs_server_port");
            strncpy(conf_opts->splitfs_server_port, buf+len+1, 8);
            for (i = 0; i < 8; i++) {
                if (conf_opts->splitfs_server_port[i] == ';') {
                    conf_opts->splitfs_server_port[i] = '\0';
                }
            }
        } else if (strncmp(buf, "zookeeper_ips", strlen("zookeeper_ips")) == 0) {
            len = strlen("zookeeper_ips");
            strncpy(ip_buffer, buf+len+1, BUFFER_SIZE-(len+1));
            parse_comma_separated(ip_buffer, conf_opts->zookeeper_ips);
        } else if (strncmp(buf, "zookeeper_port", strlen("zookeeper_port")) == 0) {
            len = strlen("zookeeper_port");
            strncpy(conf_opts->zookeeper_port, buf+len+1, 8);
            for (i = 0; i < 8; i++) {
                if (conf_opts->zookeeper_port[i] == ';') {
                    conf_opts->zookeeper_port[i] = '\0';
                }
            }
        }
    }

    // print config options
    printf("CONFIGURATION OPTIONS:\n");
    printf("metadata server ips: ");
    for (int i = 0; i < 8; i++) {
        if (conf_opts->metadata_server_ips[i][0] != '\0') {
            printf("%s ", conf_opts->metadata_server_ips[i]);
        }
    }
    printf("\n");
    printf("metadata server port: %s\n", conf_opts->metadata_server_port);
    printf("metadata client port: %s\n", conf_opts->metadata_client_port);

    printf("splitfs server ips: ");
    for (int i = 0; i < 8; i++) {
        if (conf_opts->splitfs_server_ips[i][0] != '\0') {
            printf("%s ", conf_opts->splitfs_server_ips[i]);
        }
    }
    printf("\n");
    printf("splitfs server port: %s\n", conf_opts->splitfs_server_port);

    printf("zookeeper server ips: ");
    for (int i = 0; i < 8; i++) {
        if (conf_opts->zookeeper_ips[i][0] != '\0') {
            printf("%s ", conf_opts->zookeeper_ips[i]);
        }
    }
    printf("\n");
    printf("zookeeper port: %s\n", conf_opts->zookeeper_port);

    fclose_fn(conf_file);
    return 0;
}

void parse_comma_separated(char *ip_buffer, char ips[8][16]) {
    int i, j = 0, current_ip_index = 0;
    for (i = 0; i < 8; i++) {
        memset(ips[i], 0, 16);
    }

    for (i = 0; i < strlen(ip_buffer); i++) {
        if (ip_buffer[i] == ',' || ip_buffer[i] == ';') {
            strncpy(ips[current_ip_index], ip_buffer+j, i-j);
            current_ip_index++;
            j += i+1;
        }
    }
}

int read_from_socket(int sock, void *buf, size_t len) {
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	int bytes_read = recv(sock, buf, len, 0);
	if (bytes_read < 0) {
        perror("recv");
		return bytes_read;
	}
	int cur_index = bytes_read;
	while (cur_index < len) {
		bytes_read = recv(sock, buf+cur_index, len-cur_index, 0);
		if (bytes_read < 0) {
            perror("recv");
			return bytes_read;
		}
		cur_index += bytes_read;
	}
	return cur_index;
}
