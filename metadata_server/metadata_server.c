#include <stdio.h>
#include <string.h>

#include "metadata_server.h"
#include "../splitfs/remote.h"


int main() {
    int ret;
    struct config_options conf_opts;

    ret = parse_config(&conf_opts);
    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

