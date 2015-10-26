#include <zmq.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>

#define MAX_NUM_THREADS  (100)
#define MAX_NUM_REQUESTS (100)

#define ENABLE_SYSLOG
#ifdef ENABLE_SYSLOG
#define SYSLOG(fmt...) syslog(fmt)
#else
#define SYSLOG(fmt...) 
#endif

#define MAX_NUM_RESPONSES (MAX_NUM_REQUESTS * MAX_NUM_THREADS * 6)

#define SERVER_RESPONSE_ADDR "tcp://192.168.0.241:5559"
#define SERVER_REQUEST_ADDR  "tcp://192.168.0.241:5556"

#define within(num) (int) ((float) num * random() / (RAND_MAX + 1.0))

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
            struct timeval curtime;

            while (1) {
                receiver.recv(&request);
                gettimeofday(&curtime, NULL);
                SYSLOG(LOG_INFO, "response %s sec:%ld usec:%ld\n", (char *)request.data(), curtime.tv_sec, curtime.tv_usec);
                ++reqNum;
            }
        }
    private:
        context_t ctx_;
        socket_t receiver;
};

class RequestTask {
    public:
        RequestTask(int req, int cnum) : ctx_(1), sender(ctx_, ZMQ_PUSH),
                                              numRequests(req), clientNum(cnum)
    {}
        void run() {
            srandom((unsigned) time(NULL));
            sender.connect(SERVER_REQUEST_ADDR);
            for (int num = 0; num < numRequests; ++num) {
                message_t message(16 * 1024);
                snprintf((char *)message.data(), 16 * 1024, "%d:%d", clientNum, num);
                struct timeval curtime;
                gettimeofday(&curtime, NULL);
                SYSLOG(LOG_INFO, "request %s sec:%ld usec:%ld\n", (char *)message.data(), curtime.tv_sec, curtime.tv_usec);
                sender.send(message);
                usleep(within(400 * 1000)); //400 msec
            }
        }
    private:
        context_t ctx_;
        socket_t sender;
        int numRequests;
        int clientNum;
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

    openlog(NULL, 0, LOG_USER);
    SYSLOG(LOG_INFO, "client: numThreads: %d numRequests: %d\n", numThreads, numRequests);
    ResponseTask res(reqMaps);
    std::thread t1(bind(&ResponseTask::run, &res));
    t1.detach();

    vector<std::thread *> threadObjs;
    vector<RequestTask *> reqTasks;
    for (int num = 0; num < numThreads; ++num) {
        reqTasks.push_back(new RequestTask(numRequests, num));
        threadObjs.push_back(new thread(bind(&RequestTask::run, reqTasks[num])));
        threadObjs[num]->detach();
    }
    getchar();
    for (int num = 0; num < numThreads; ++num) {
        delete reqTasks[num];
        delete threadObjs[num];
    }
    closelog();

    return 0;
}
