#ifndef METADATA_SERVER_H
#define METADATA_SERVER_H

#define BUFFER_SIZE 256
#define CONFIG_PATH "../config"

// options read from a config file
struct config_options {
    char metadata_server_port[8];
    char splitfs_server_port[8];
    char zookeeper_port[8];
    // allow up to 8 machines that may act as metadata servers,
    // splitfs servers, or zookeeper servers
    // these lists can overlap
    char metadata_server_ips[8][16];
    char splitfs_server_ips[8][16];
    char zookeeper_ips[8][16];
    
};

int parse_config(struct config_options *conf_opts);
void parse_comma_separated(char *ip_buffer, char ips[8][16]);

#endif // METADATA_SERVER_H