#include <zmq.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <thread>
#include <map>

using namespace std;
using namespace zmq;
#define within(num) (int) ((float) num * random() / (RAND_MAX + 1.0))

#define  SINK_TASK_RECEIVER_ADDR "tcp://*:5558"
#define  SINK_TASK_RESPONSE_ADDR "tcp://*:5559"
#define  WORKER_1_ADDR "tcp://localhost:5557"
#define  WORKER_2_ADDR "tcp://192.168.0.203:5557"
#define  CLIENT_REQUEST_RECV_ADDR "tcp://*:5556"

class SinkTask {
    public:
        SinkTask(std::map<string, int> & cmap) : 
            ctx_(1), receiver(ctx_, ZMQ_PULL), response(ctx_, ZMQ_PUSH), clientMap(cmap)
    {}
        void run() {
            receiver.bind(SINK_TASK_RECEIVER_ADDR);
            response.bind(SINK_TASK_RESPONSE_ADDR);
            int reqNum = 0;

            while (1) {
                message_t message;
                receiver.recv(&message);
                ++reqNum;
                cout << "Received message " <<  reqNum << endl;
                --clientMap[(char *)message.data()];
                if (clientMap[(char *)message.data()] == 0) {
                    message_t reply(1024);
                    memcpy((char *)reply.data(), (char *)message.data(), 1024);
                    response.send(message);
                }
            }
        }
    private:
        context_t ctx_;
        socket_t receiver;
        socket_t response;
        std::map<string, int> & clientMap;
};

class ServerTask {
    public:
        ServerTask(std::map<string, int> & cmap) : 
            ctx_(1), receiver(ctx_, ZMQ_PULL), sender(ctx_, ZMQ_DEALER), 
            sender_1(ctx_, ZMQ_DEALER), clientMap(cmap)
    {}
        void run() {
            char identity[16] = "server";
            sender.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));
            sender.connect(WORKER_1_ADDR);
            sender_1.connect(WORKER_2_ADDR);
            receiver.bind(CLIENT_REQUEST_RECV_ADDR);
            message_t request, message;
            int total_msec = 0, reqNum = 0;
            
            cout << "server started..." << endl;
            srandom((unsigned) time(NULL));
            while (1) {
                receiver.recv(&request);
                clientMap[(char *)request.data()] = 2;
                ++reqNum;
                message.rebuild(1024);
                memcpy((char *)message.data(), (char *)request.data(), 1024);
                sender.send(message);
                sender_1.send(message);
            }
        }
    private:
        context_t ctx_;
        socket_t receiver;
        socket_t sender;
        socket_t sender_1;
        std::map<string, int> & clientMap;
};

int main(int argc, char *argv[])
{
    std::map <string, int> clientMap;
    ServerTask serverTaskObj(clientMap);
    SinkTask st(clientMap);
    std::thread serverThread(bind(&ServerTask::run, &serverTaskObj));
    serverThread.detach();

    std::thread sinkthread(bind(&SinkTask::run, &st));
    sinkthread.detach();

    getchar();
    cout << "sending tasks to worker " << endl;
    return 0;
}
