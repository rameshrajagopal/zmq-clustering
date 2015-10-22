#include <zmq.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;
using namespace zmq;
#define within(num) (int) ((float) num * random() / (RAND_MAX + 1.0))

#define  SERVER_ADDR "tcp://*:5557"
#define  SERVER_RESPONSE_ADDR "tcp://192.168.0.241:5558"
#define  INTERNAL_IPC_ADDR   "inproc://backend"

class WorkerTask {
public:
    WorkerTask(zmq::context_t &ctx, int sock_type)
        : ctx_(ctx),
          worker_(ctx_, sock_type),
          sender(ctx_, ZMQ_PUSH)
    {
    }

    void work() {
        worker_.connect(INTERNAL_IPC_ADDR);
        sender.connect(SERVER_RESPONSE_ADDR);
        int respNum  = 0;

        try {
            int workload;
            while (true) {
                zmq::message_t identity;
                zmq::message_t msg;
                zmq::message_t copied_id;
                zmq::message_t copied_msg;
                // get server id and message
                worker_.recv(&identity);
                worker_.recv(&msg);
                ++respNum;
                /* actual work */
                usleep(100 * 1000);
                /* reply */
#if 0
                copied_id.copy(&identity);
                copied_msg.copy(&msg);
                worker_.send(copied_id, ZMQ_SNDMORE);
                worker_.send(copied_msg);
#else
                cout << "Reponse Num " << respNum << endl;
                sender.send(msg);
#endif
            }
        }
        catch (std::exception &e) {}
    }

private:
    zmq::context_t &ctx_;
    zmq::socket_t worker_;
    socket_t sender;
};

class WorkerMainTask {
public:
    WorkerMainTask(int numThreads) 
        : ctx_(1),
          frontend_(ctx_, ZMQ_ROUTER),
          backend_(ctx_, ZMQ_DEALER),
          maxThreads(numThreads)
    {}

    enum { kMaxThread = 10 };

    void run() {
//        frontend_.bind("tcp://*:5557");
        frontend_.bind(SERVER_ADDR);
        backend_.bind(INTERNAL_IPC_ADDR);

        std::vector<WorkerTask *> worker;
        std::vector<std::thread *> worker_thread;
        for (int i = 0; i < maxThreads; ++i) {
            worker.push_back(new WorkerTask(ctx_, ZMQ_DEALER));

            worker_thread.push_back(new std::thread(std::bind(&WorkerTask::work, worker[i])));
            worker_thread[i]->detach();
        }

        try {
            zmq::proxy((void *)frontend_, (void *)backend_, nullptr);
        }
        catch (std::exception &e) {

        }

        for (int i = 0; i < maxThreads; ++i) {
            delete worker[i];
            delete worker_thread[i];
        }
    }

private:
    zmq::context_t ctx_;
    zmq::socket_t frontend_;
    zmq::socket_t backend_;
    int maxThreads;
};


int main(int argc, char *argv[])
{
    context_t context(1);
    int numThreads = 0;

    if (argc == 2) {
        numThreads = atoi(argv[1]);
    } else {
        numThreads = 10;
    }
    cout << "Worker: " << numThreads << endl;
    WorkerMainTask maintask(numThreads);
    std::thread t(bind(&WorkerMainTask::run, &maintask));
    t.detach();
    getchar();

    return 0;
}
