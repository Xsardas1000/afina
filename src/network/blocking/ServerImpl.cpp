#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <../src/protocol/Parser.h>

#include <algorithm>

#include <afina/Executor.h>

namespace Afina {
namespace Network {
namespace Blocking {

    void *ServerImpl::RunAcceptorProxy(void *p) {
        auto *srv = reinterpret_cast<ServerImpl *>(p);
        try {
            srv->RunAcceptor();
        } catch (std::runtime_error &ex) {
            std::cerr << "Server fails: " << ex.what() << std::endl;
        }
        return nullptr;
    }


    void* ServerImpl::RunConnectionProxy(void *p) {
        auto *args = (Args*)p;
        auto *srv = reinterpret_cast<ServerImpl *>(args->this_ptr);
        try {
            srv->RunConnection(args->client_socket);
        } catch (std::runtime_error &ex) {
            std::cerr << "Server fails: " << ex.what() << std::endl;
        }
        return nullptr;
    }

// See Server.h
    ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
    ServerImpl::~ServerImpl() {}

// See Server.h
    void ServerImpl::Start(uint32_t port, uint16_t n_workers=4) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

        // If a client closes a connection, this will generally produce a SIGPIPE
        // signal that will kill the process. We want to ignore this signal, so send()
        // just returns -1 when this happens.
        sigset_t sig_mask;
        sigemptyset(&sig_mask);
        sigaddset(&sig_mask, SIGPIPE);
        if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
            throw std::runtime_error("Unable to mask SIGPIPE");
        }

        // Setup server parameters BEFORE thread created, that will guarantee
        // variable value visibility
        std::cout << n_workers << std::endl;
        max_workers = n_workers;
        listen_port = port;

        // The pthread_create function creates a new thread.
        //
        // The first parameter is a pointer to a pthread_t variable, which we can use
        // in the remainder of the program to manage this thread.
        //
        // The second parameter is used to specify the attributes of this new thread
        // (e.g., its stack size). We can leave it NULL here.
        //
        // The third parameter is the function this thread will run. This function *must*
        // have the following prototype:
        //    void *f(void *args);
        //
        // Note how the function expects a single parameter of type void*. We are using it to
        // pass this pointer in order to proxy call to the class member function. The fourth
        // parameter to pthread_create is used to specify this parameter value.
        //
        // The thread we are creating here is the "server thread", which will be
        // responsible for listening on port 23300 for incoming connections. This thread,
        // in turn, will spawn threads to service each incoming connection, allowing
        // multiple clients to connect simultaneously.
        // Note that, in this particular example, creating a "server thread" is redundant,
        // since there will only be one server thread, and the program's main thread (the
        // one running main()) could fulfill this purpose.
        running.store(true);
        if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
            throw std::runtime_error("Could not create server thread");
        }
    }

// See Server.h
    void ServerImpl::Stop() {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        running.store(false);
        shutdown(server_socket, SHUT_RDWR);
    }

// See Server.h
    void ServerImpl::Join() {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        pthread_join(accept_thread, 0); // wait
    }

// See Server.h
    void ServerImpl::RunAcceptor() {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

        // For IPv4 we use struct sockaddr_in:
        // struct sockaddr_in {
        //     short int          sin_family;  // Address family, AF_INET
        //     unsigned short int sin_port;    // Port number
        //     struct in_addr     sin_addr;    // Internet address
        //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
        // };
        //
        // Note we need to convert the port to network order

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;          // IPv4
        server_addr.sin_port = htons(listen_port); // TCP port number
        server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

        // Arguments are:
        // - Family: IPv4
        // - Type: Full-duplex stream (reliable)
        // - Protocol: TCP
        server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == -1) {
            throw std::runtime_error("Failed to open socket");
        }

        // when the server closes the socket,the connection must stay in the TIME_WAIT state to
        // make sure the client received the acknowledgement that the connection has been terminated.
        // During this time, this port is unavailable to other processes, unless we specify this option
        //
        // This option let kernel knows that we are OK that multiple threads/processes are listen on the
        // same port. In a such case kernel will balance input traffic between all listeners (except those who
        // are closed already)
        int opts = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket setsockopt() failed");
        }

        // Bind the socket to the address. In other words let kernel know data for what address we'd
        // like to see in the socket
        if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket bind() failed");
        }

        // Start listening. The second parameter is the "backlog", or the maximum number of
        // connections that we'll allow to queue up. Note that listen() doesn't block until
        // incoming connections arrive. It just makesthe OS aware that this process is willing
        // to accept connections on this socket (which is bound to a specific IP and port)
        if (listen(server_socket, 5) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket listen() failed");
        }


        Afina::Executor ex("Executor 1", 10, 20, 60, std::chrono::milliseconds(100));

        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t sinSize = sizeof(struct sockaddr_in);
        while (running.load())
        {
            std::cout << "network debug: waiting for connection..." << std::endl;

            // When an incoming connection arrives, accept it. The call to accept() blocks until
            // the incoming connection arrives
            if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
                close(client_socket); //?
                throw std::runtime_error("Socket accept() failed");
            }

            // TODO: Start new thread and process data from/to connection
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                if(connections.size() < max_workers) {

                    if (!ex.Execute(&Afina::Network::Blocking::ServerImpl::RunConnection, this, client_socket))
                    {
                        close(client_socket);
                        throw std::runtime_error("Could not create server thread");
                    }

//                    pthread_t thread_id;
//                    auto args = new Args(this, client_socket);
//
//                    if (pthread_create(&thread_id, nullptr, ServerImpl::RunConnectionProxy, args) < 0) {
//                        throw std::runtime_error("Could not create server thread");
//                    }
//                    connections.insert(thread_id);
//                    std::cout << "Current number of connections:" << connections.size() << std::endl;

                } else {
                    close(client_socket);
                }
            } // TODO

            std::cout << "test" << std::endl;

        }

        // Cleanup on exit...
        close(server_socket);
        std::cout << "close server socket" << std::endl;

        // Wait until for all connections to be complete
        std::unique_lock<std::mutex> _lock(connections_mutex);
        while (!connections.empty()) {
            connections_cv.wait(_lock);
        }
    }



// See Server.h
    void ServerImpl::RunConnection(int client_socket) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

        pthread_t self_id = pthread_self();
        {

            std::lock_guard<std::mutex> lock(connections_mutex);
            connections.insert(self_id);
        }



        // TODO: All connection work is here
        size_t buf_size = 256;
        char buf[buf_size];
        memset(buf, 0, buf_size);
        ssize_t received;
        size_t parsed = 0;
        Afina::Protocol::Parser parser;
        std::string command = "";

        bool parse_finished;
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

        close(client_socket);
        {
            std::unique_lock<std::mutex> _lock(connections_mutex);
            auto index = connections.find(pthread_self());
            connections.erase(index);
            connections_cv.notify_all();
        }
    }

} // namespace Blocking
} // namespace Network
} // namespace Afina