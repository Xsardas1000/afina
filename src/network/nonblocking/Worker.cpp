#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Utils.h"


#include <cstring>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <../src/protocol/Parser.h>



namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) {
    // TODO: implementation here
    pStorage = ps;
}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here

    this->server_socket = server_socket;
    running.store(true);
    struct worker_pthread_args *args = (struct worker_pthread_args*)malloc(sizeof(struct worker_pthread_args));
    args->server_socket = server_socket;
    args->ptr = this;

    if (pthread_create(&thread, NULL, OnRunProxy, args) < 0) {
        throw std::runtime_error("Could not create worker thread");
    }

}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    running.store(false);
    shutdown(server_socket, SHUT_RDWR);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    pthread_join(thread, NULL);
}



void *Worker::OnRunProxy(void *p) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    auto *args = (worker_pthread_args*) p;
    auto *worker = reinterpret_cast<Worker *>(args->ptr);
    auto server_socket = args->server_socket;

    try {
        worker->OnRun(args);
    } catch (std::runtime_error &ex) {
        std::cerr << "Worker fails: " << ex.what() << std::endl;
    }
    return NULL;
}


// See Worker.h
void Worker::OnRun(void *args) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: implementation here
    // 1. Create epoll_context here
    int max_epoll = 5;
    int epoll_fd = epoll_create(max_epoll);
    if (epoll_fd == -1) {
       throw std::runtime_error("Failed to create epoll context.");
    }

    // 2. Add server_socket to context
    struct epoll_event server_listen_event;
    server_listen_event.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
    server_listen_event.data.ptr = (void*)&server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &server_listen_event) == -1) {
        throw std::runtime_error("Failed to add an event for server socket.");
    }

    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    unsigned int MAXEVENTS = 20;
    struct epoll_event *events = (epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));


    while(running.load())
    {
        // waiting for a event on the epoll object, on which epoll_fd (file descriptor) points to
        // can return from 1 to MAXEVENTS
        int n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        if (n == -1) {
            throw std::runtime_error("Failed to epoll_wait");
        }

        // for each ready socket
        for (int i = 0; i < n; ++i) {
            if (&server_socket == events[i].data.ptr) {
                // 3. Accept new connections, don't forget to call make_socket_nonblocking on
                //    the client socket descriptor
                int client_socket = accept(server_socket, (struct sockaddr *)&clientaddr, &clientlen);
                if (client_socket == -1) {
                    if( errno == EINVAL || errno == EAGAIN || errno == EWOULDBLOCK ) {
                        std::cout << "Accept returned EINVAL\n";
                        break;
                    } else {
                        throw std::runtime_error("Accept failed");
                    }
                }
                make_socket_non_blocking(client_socket);

                // 4. Add connections to the local context
                //
                // Do not forget to use EPOLLEXCLUSIVE flag when register socket
                // for events to avoid thundering herd type behavior.
                struct epoll_event client_conn_event;
                client_conn_event.events = EPOLLIN | EPOLLOUT;
                client_conn_event.data.ptr = (void*)&client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_conn_event) == -1) {
                    throw std::runtime_error("Failed to add an event for client socket.");
                }
            }
            else {
                int client_socket = *(int *)events[i].data.ptr;
                // 5. Process connection events
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                } else if (events[i].events & EPOLLIN) {
                    if (ConnectionWork(client_socket) == true) {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                        close(client_socket);
                    }
                } else {
                    throw std::runtime_error("Unknown event");
                }
            }
        }
    }


    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.
}

bool Worker::ConnectionWork(int client_socket) {
  // TODO: All connection work is here
  size_t buf_size = 256;
  char buf[buf_size];
  memset(buf, 0, buf_size);
  ssize_t received;
  size_t parsed = 0;
  Afina::Protocol::Parser parser;
  std::string command = "";

  bool parse_finished = false;
  while(running.load() && ((received = (int)read(client_socket, buf, buf_size)) != 0 || !command.empty() > 0) )
  {
      if(received < 0)
      {
          std::cout << "Reading stream error" << std::endl;
          break;
      }

      //std::cout << "buf after reading:" << buf << std::endl;
      command += buf;
      try
      {
          parse_finished = parser.Parse(command.c_str(), received, parsed);
          //std::cout << "buf after parsing:" << buf << std::endl;

      } catch(...) {
          std::string result = "PARSE ERROR\r\n";
          if (send(client_socket, result.data(), result.size(), 0) <= 0)
          {
              close(client_socket);
              throw std::runtime_error("Socket send() failed");
          }
          break;
      }
      command.erase(0, parsed); //remove parsed characters from the beginning
      parsed = 0;

      // if parser returns true, it means that it has parsed the hole command
      if(parse_finished) {
          uint32_t body_size;

          //create new command and get number of bytes to read (arguments for the command)
          std::unique_ptr<Afina::Execute::Command> com_ptr = parser.Build(body_size);

          parser.Reset();
          parsed = 0;

          std::cout << "Bytes to read:" << body_size << std::endl;
          std::string args;
          if(body_size > 0) {
              while(body_size + 2 > command.size())
              {
                  received = read(client_socket, buf, buf_size);
                  command += buf;
              }

              args = command.substr(0, body_size);
          }
          command.clear();
          std::string result;
          try {
              com_ptr->Execute(*pStorage, args, result);
          } catch(...) {
              result = "SERVER_ERROR";
          }
          result += "\r\n";
          if (!result.empty() && send(client_socket, result.data(), result.size(), 0) <= 0) {
              close(client_socket);
              throw std::runtime_error("Socket send() failed");
          }

      }
      memset(buf, 0, buf_size);
  }
  return parse_finished;

}


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
