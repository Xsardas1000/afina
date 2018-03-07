////
//// Created by parallels on 07.03.18.
////
//
//#include "ServerImpl.h"
//
//#include <cassert>
//#include <cstring>
//#include <iostream>
//#include <memory>
//#include <stdexcept>
//
//#include <pthread.h>
//#include <signal.h>
//
//#include <netdb.h>
//#include <sys/ioctl.h>
//#include <sys/socket.h>
//#include <sys/types.h>
//
//#include <arpa/inet.h>
//#include <netinet/in.h>
//#include <unistd.h>
//
//#include <afina/Storage.h>
//#include <afina/execute/Command.h>
//#include <../src/protocol/Parser.h>
//
//#include <algorithm>
//#include <errno.h>
//
//int main() {
//    fd_set fd_in, fd_out;
//    FD_ZERO(&fd_in);
//    FD_ZERO(&fd_out);
//
//    struct sockaddr_in addr;
//    int master = socket(AF_INET, SOCK_STREAM, 0);
//
//    int on = 1;
//    ioctl(master, FIONBIO, (char *)&on);
//
//    memset(&addr, 0, sizeof(addr));
//    addr.sin_family = AF_INET;
//    addr.sin_addr.s_addr = htonl(INADDR_ANY);
//    addr.sin_port = htonl(6666);
//
//    bind(master, (struct sockaddr))
//
//    listen(master, 32);
//    FD_SET(master, &fd_in);
//    int max_sd = master;
//    do {
//        int rc;
//        rc = select(max_sd + 1, &fd_in, NULL, NULL, NULL);
//
//        if (rc > 0) {
//            if (FD_ISSET(master, &fd_in)) {
//                int new_sock = accept(master,)
//            }
//        }
//    }
//
//
//}
//
//
