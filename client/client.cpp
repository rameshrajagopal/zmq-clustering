#include <zmq.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <time.h>
#include <sys/time.h>

#define MAX_NUM_THREADS  (100)
#define MAX_NUM_REQUESTS (100)

#define MAX_NUM_RESPONSES (MAX_NUM_REQUESTS * MAX_NUM_THREADS * 6)

#define SERVER_RESPONSE_ADDR "tcp://192.168.0.241:5559"
#define SERVER_REQUEST_ADDR  "tcp://192.168.0.241:5556"

using namespace std;
using namespace zmq;

class ResponseTask {
    public:
        ResponseTask() : ctx_(1), receiver(ctx_, ZMQ_PULL)
    {}
        void run() {
            receiver.connect(SERVER_RESPONSE_ADDR);
            message_t request;
            int reqNum = 0;
            struct timeval tstart;
            struct timeval tend, tdiff;

            while (1) {
                receiver.recv(&request);
                if (reqNum == 0) {
                    gettimeofday(&tstart, NULL);
                }
                ++reqNum;
                cout <<".";
                cout.flush();
                if (reqNum == MAX_NUM_RESPONSES) {
                    gettimeofday(&tend, NULL);
                    int total_msec = 0;

                    if (tend.tv_usec < tstart.tv_usec) {
                        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec - 1;
                        tdiff.tv_usec = 1000000 + tend.tv_usec - tstart.tv_usec;
                    }
                    else {
                        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec;
                        tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
                    }
                    total_msec = tdiff.tv_sec * 1000 + tdiff.tv_usec / 1000;
                    std::cout << "\nTotal elapsed time: " << total_msec << " msec\n" << std::endl;
                }
            }

        }
    private:
        context_t ctx_;
        socket_t receiver;
};

class RequestTask {
    public:
        RequestTask(int req) : ctx_(1), sender(ctx_, ZMQ_PUSH), numRequests(req)
    {}
        void run() {
            sender.connect(SERVER_REQUEST_ADDR);
            for (int num = 0; num < numRequests; ++num) {
                message_t message(1024);
                memset((void *)message.data(), 'a', 1024);
                sender.send(message);
            }
        }
    private:
        context_t ctx_;
        socket_t sender;
        int numRequests;
};

int main(int argc, char *argv[])
{
    int numThreads, numRequests;
    context_t context(1);

    if (argc == 3) {
        numThreads = atoi(argv[1]);
        numRequests = atoi(argv[2]);
    } else {
        numThreads = MAX_NUM_THREADS;
        numRequests = MAX_NUM_REQUESTS;
    }

    cout << "Client: " << numThreads << " threads " << numRequests << " requests" << endl;
    ResponseTask res;
    std::thread t1(bind(&ResponseTask::run, &res));
    t1.detach();

    vector<std::thread *> threadObjs;
    vector<RequestTask *> reqTasks;
    for (int num = 0; num < numThreads; ++num) {
        reqTasks.push_back(new RequestTask(numRequests));
        threadObjs.push_back(new thread(bind(&RequestTask::run, reqTasks[num])));
        threadObjs[num]->detach();
    }
    getchar();
    for (int num = 0; num < numThreads; ++num) {
        delete reqTasks[num];
        delete threadObjs[num];
    }

    return 0;
}
