#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <mutex> // isto é para as threads não se vomitarem todas no output
#include <thread>
#include <sys/time.h>
#include <sys/select.h>

#define BUFF 512

std::mutex mtx; // critical output

const char * channel_tag =      "\033[1;33m";
const char * connection_tag =   "\033[1;32m";
const char * disconnect_tag =   "\033[1;31m";
const char * ip_name_tag =      "\033[1;35m";
const char * reset_tag =        "\033[0m";

void server_setup(int * sok, const int * port, sockaddr_in * info)
{
    *sok = socket(AF_INET, SOCK_STREAM, 0);
    memset((char *)info, '\0', sizeof(*info));

    info->sin_family = AF_INET;
    info->sin_addr.s_addr = INADDR_ANY;
    info->sin_port = htons(*port);
}


void listening(const int thread_id, sockaddr_in * n_connection, const int * server)
{
    mtx.lock();
    std::cout << "thread " << thread_id << " open!" << std::endl;
    mtx.unlock();

    char buffer[BUFF];
    int client_socket, n_chan, sresult;

    socklen_t client_length = sizeof(sockaddr_in);


    client_socket = accept(*server, (struct sockaddr *) n_connection, &client_length);

    if (client_socket < 0)
        return;


    std::chrono::system_clock::time_point now;
    std::time_t now_c;

    char str[INET_ADDRSTRLEN];
    inet_ntop( AF_INET, &n_connection->sin_addr, str, INET_ADDRSTRLEN );

    mtx.lock();
    std::cout << "[" << connection_tag << str << reset_tag << "] has joined on channel " << thread_id << std::endl;
    mtx.unlock();

    memset(buffer, '\0', BUFF);

    // agora usamos aqui o select

    fd_set readset, tempset;
    FD_ZERO(&readset);
    FD_ZERO(&tempset);
    FD_SET(0,&tempset);
    FD_SET(client_socket, &readset);
    struct timeval time_v;

    time_v.tv_sec = 60;//300; // 60 sec = 1 min, 60 * 5 = 300 = 5 min
    time_v.tv_usec = 60000;//300000;

    mtx.lock();
    std::cout << "Setting 1 minute timeout..." << std::endl;
    mtx.unlock();

    sresult = select(client_socket+1, &readset, NULL, NULL, &time_v);

    if(sresult < 0)
    {
        mtx.lock();
        std::cerr << "Error in channel " << thread_id << std::endl;
        mtx.unlock();
        close(client_socket);
        return;
    }
    else if(sresult == 0)
    {
        mtx.lock();
        std::cout << "(not_dowhile)[" << disconnect_tag << str << reset_tag << "] has timedout from channel ("
        << channel_tag << thread_id <<reset_tag << ") !" << std::endl;
        mtx.unlock();

        n_chan = write(client_socket, "Disconnected from innactivity", 29);
    }
    //n_chan = read(client_socket, buffer, 511);
    else {
        n_chan = recv(client_socket, buffer, 511, 0);
        do {
            std::cout << "Estou no do while!" << std::endl;

            if (n_chan < 0) {
                mtx.lock();
                std::cout << "[" << disconnect_tag << str << reset_tag << "] has left channel ("
                          << channel_tag << thread_id << reset_tag << ") !" << std::endl;
                mtx.unlock();
                break;
            }

            now = std::chrono::system_clock::now();
            now_c = std::chrono::system_clock::to_time_t(now);

            mtx.lock();
            std::cout << "[" << ip_name_tag << str << reset_tag << "]:["
                      << channel_tag << thread_id << "_chan" << reset_tag << "]@("
                      << std::put_time(std::localtime(&now_c), "%T")
                      << ")> " << buffer << std::endl;
            mtx.unlock();

            n_chan = write(client_socket, "I got your message", 18);

            if (n_chan < 0) {
                mtx.lock();
                std::cout << "[" << disconnect_tag << str << reset_tag << "] has left channel " << thread_id
                          << std::endl;
                mtx.unlock();
                break;
            }

            memset(buffer, '\0', BUFF);

            // FAZER ESTA MERDA DE NOVO, PQ NS O QUE SE ESTÁ A PASSAR, tirando o programa
            FD_ZERO(&readset);
            FD_SET(0, &readset);
            FD_ZERO(&tempset);
            FD_SET(0,&tempset);
            FD_SET(client_socket, &readset);
            // ^ FALTA TESTAR: remover isto para ver se ainda funciona, já que o problema n era disto em principio

            sresult = select(client_socket + 1, &readset, NULL, NULL, &time_v);

            if (sresult < 0) {
                mtx.lock();
                std::cerr << "Error in channel " << thread_id << std::endl;
                mtx.unlock();
                break;
            } else if (sresult == 0) {
                mtx.lock();
                std::cout << "(dowhile)[" << disconnect_tag << str << reset_tag << "] has timedout from channel ("
                          << channel_tag << thread_id << reset_tag << ") !" << std::endl;
                mtx.unlock();

                n_chan = write(client_socket, "Disconnected from innactivity", 29);
                //ignoramos se deu ou não, isto só se faz porque somos simpáticos
                break;
            }

            n_chan = recv(client_socket, buffer, 511, 0);

        } while ((sresult > 0 && FD_ISSET(client_socket, &readset)) && (buffer[0] != 'd' && buffer[1] != 'c' && buffer[2] != '\0'));
    }
    //limpamos a memoria, não vá alguem cheira-la >.>
    memset(buffer,'\0',BUFF);

    mtx.lock();
    std::cout << "(" << channel_tag << thread_id << reset_tag << ") has fallen, and it can't get up!" << std::endl;
    mtx.unlock();
    close(client_socket);
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}


int main(int argc, char ** argv) {


    if(argc != 2)
        return 1;

    int server_socket = -1 , server_port;
    sockaddr_in server_addr;
    sockaddr_in client_addr[5];

    std::vector<std::thread> connections;


    server_port = atoi(argv[1]);

    server_setup(&server_socket,&server_port, &server_addr);

    std::cout << "size of vector: " << connections.size() << std::endl;

    if (server_socket < 0)
        error("ERROR opening socket");
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("ERROR on binding");

    std::cout << "Listening for up to 5 incoming connection..." << std::endl;
    listen(server_socket,1);

    for(int i = 0; i < 5; i++)
        connections.emplace_back(std::thread(listening, i, &client_addr[i], &server_socket));

    std::cout << "size of vector: " << connections.size() << std::endl;

    for(int i = 0; i != 5; i++)
        connections[i].join();

    std::cout << "only appears when all threads are done" << std::endl;

    return 0;
}