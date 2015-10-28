#include <zmq.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <limits.h>
#include <map>
#include <mapwrapper.h>

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

struct timeval getDiffTime(struct timeval tstart, struct timeval tend)
{
    struct timeval tdiff;

    //cout << "v " << tstart.tv_sec << " " << tstart.tv_usec << endl;
    if (tend.tv_usec < tstart.tv_usec) {
        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec - 1;
        tdiff.tv_usec = 1000000 + tend.tv_usec - tstart.tv_usec;
    } else {
        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec;
        tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
    }
    return tdiff;
}

class ResponseTask {
    public:
        ResponseTask(Map<string, struct timeval> &rmap) : 
            ctx_(1), receiver(ctx_, ZMQ_PULL), reqMap(rmap)
    {}
        void run() {
            receiver.connect(SERVER_RESPONSE_ADDR);
            message_t request;
            int reqNum = 0;
            char buf[16];
            struct timeval curtime;

            while (1) {
                receiver.recv(&request);
                gettimeofday(&curtime, NULL);
                snprintf(buf, sizeof(buf), "%s", (char *)request.data());
                reqMap.put(buf, getDiffTime(reqMap.get(buf), curtime));
                SYSLOG(LOG_INFO, "response %s sec:%ld usec:%ld\n", (char *)request.data(), curtime.tv_sec, curtime.tv_usec);
                ++reqNum;
            }
        }
    private:
        context_t ctx_;
        socket_t receiver;
        Map<string, struct timeval> & reqMap;
};

class RequestTask {
    public:
        RequestTask(int req, int cnum, Map<string, struct timeval> & rmap) : 
            ctx_(1), sender(ctx_, ZMQ_PUSH), numRequests(req), clientNum(cnum), 
            reqMap(rmap)
    {}
        void run() {
            char buf[16] = {0};
            srandom((unsigned) time(NULL));
            sender.connect(SERVER_REQUEST_ADDR);
            for (int num = 0; num < numRequests; ++num) {
                message_t message(16 * 1024);
                snprintf((char *)message.data(), 16 * 1024, "%d:%d", clientNum, num);
                snprintf(buf, 16, "%s", (char *)message.data());
                cout << "req : " << clientNum << " " << num << endl;
                struct timeval curtime;
                gettimeofday(&curtime, NULL);
                reqMap.put(buf, curtime);
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
        Map<string, struct timeval> & reqMap;
};


int main(int argc, char *argv[])
{
    int numThreads, numRequests;
    context_t context(1);
    Map<string, struct timeval> reqMap;

    if (argc == 3) {
        numThreads = atoi(argv[1]);
        numRequests = atoi(argv[2]);
    } else {
        numThreads = MAX_NUM_THREADS;
        numRequests = MAX_NUM_REQUESTS;
    }

    openlog(NULL, 0, LOG_USER);
    SYSLOG(LOG_INFO, "client: numThreads: %d numRequests: %d\n", numThreads, numRequests);
    ResponseTask res(reqMap);
    std::thread t1(bind(&ResponseTask::run, &res));
    t1.detach();

    vector<std::thread *> threadObjs;
    vector<RequestTask *> reqTasks;
    for (int num = 0; num < numThreads; ++num) {
        reqTasks.push_back(new RequestTask(numRequests, num, reqMap));
        threadObjs.push_back(new thread(bind(&RequestTask::run, reqTasks[num])));
        threadObjs[num]->detach();
    }
    getchar();
    reqMap.printAll();
    getchar();
    for (int num = 0; num < numThreads; ++num) {
        delete reqTasks[num];
        delete threadObjs[num];
    }
    closelog();

    return 0;
}
