while(running.load())
{
  //  std::cout << "While loop on worker\n";
    //Системный вызов epoll_wait() ожидает события на экземпляре epoll, на который указывает
    //файловый дескриптор epoll_fd. Область памяти, на которую указывает events, будет содержать события,
    //доступные для обработки. Вызов epoll_wait() может вернуть до MAXEVENTS событий.
    int n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

    std::cout << "events happend\n";
    //возвращает количество файловых дескрипторов, готовых для запросов ввода-вывода
    if (n == -1) {
        throw std::runtime_error("Failed to epoll_wait");
    }

    int current_socket;
    //std::cout << "test\n";
    // for each ready socket
    for (int i = 0; i < n; ++i) {
        current_socket = events[i].data.fd;
        if (server_socket == current_socket) {

            // 3. Accept new connections, don't forget to call make_socket_nonblocking on
            //    the client socket descriptor

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                std::cout << "EPOLLERR or EPOLLHUB, server_socket\n";
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_socket, NULL);
                close(server_socket);

            } else if (events[i].events & EPOLLIN) {
                int client_socket;
                if ((client_socket == accept(server_socket,  (struct sockaddr *)&clientaddr, &clientlen)) == -1) {
                    throw std::runtime_error("Failed to connect new client.");

                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_socket, NULL);
                    close(server_socket);
                }

                std::cout << "accepted client socket" << std::endl;
                make_socket_non_blocking(client_socket);
                connection_sockets.insert(client_socket);

                // 4. Add connections to the local context
                //
                // Do not forget to use EPOLLEXCLUSIVE flag when register socket
                // for events to avoid thundering herd type behavior.
                struct epoll_event client_conn_event;
                client_conn_event.events = EPOLLIN  | EPOLLERR | EPOLLHUP;
                client_conn_event.data.fd = client_socket;  // buf, state, clent_socket
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_conn_event) == -1) {
                    throw std::runtime_error("Failed to add an event for client socket.");
                }
            } else {
                throw std::runtime_error("Unknown event");
            }

        } else {
            std::cout << "test\n";
            if (connection_sockets.count(current_socket) != 0 && events[i].events & (EPOLLERR | EPOLLHUP)) {
                std::cout << "EPOLLERR or EPOLLHUB, client socket\n";

                connection_sockets.erase(current_socket);
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_socket, nullptr) == -1) {
                    throw std::runtime_error("Failed delete current_socket from epoll");
                }
                close(current_socket);

            } else if (connection_sockets.count(current_socket) != 0 && events[i].events & EPOLLIN) {
                  std::cout << "EPOLLIN Event client\n";
                  if (ConnectionWork(current_socket)) {
                      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_socket, NULL);
                      close(current_socket);
                  }
            } else {
                throw std::runtime_error("Unknown event");
            }
        }
    }
}



// enum class State {
//     kReading,
//     kBuilding,
//     kWriting
// };
//
// class Connection {
// public:
//     Connection(int socket) : socket(socket) {
//         buf.clear();
//     }
//
//     ~Connection(void) {}
//     int socket;
//     std::string buf;
// };
