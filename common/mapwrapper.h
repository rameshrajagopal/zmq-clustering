#ifndef _MAP_WRAPPER_H_INCLUDED_
#define _MAP_WRAPPER_H_INCLUDED_

#include <map>
#include <memory>
#include <mutex>

using namespace std;

template <typename T, typename U>
class Map 
{
    public:
    void put(T key, U value)
    {
        //cout << "put " << key << endl;
        std::unique_lock<std::mutex> mlock(mutex_);
        rmap[key] = value;
        mlock.unlock();
    }
    U get(T key) 
    {
        //cout << "get " << key << endl;
        std::unique_lock<std::mutex> mlock(mutex_);
        auto it = rmap.find(key);
        assert(it != rmap.end());
        U item = rmap[key];
        mlock.unlock();
        return item;
    }
    void printAll() 
    {
        int min_value = INT_MAX, max_value = INT_MIN;
        string min_req, max_req;
        int avg = 0, num_requests = 0;
        int total_msec = 0;
        int nerrors = 0;
        int64_t total = 0;

        auto it = rmap.begin();
        for (; it != rmap.end(); ++it) {
            total_msec = (it->second.tv_sec * 1000) + (it->second.tv_usec/1000);
            if (total_msec == 0) ++nerrors;
            else {
                total += total_msec; 
                ++num_requests;
                if (total_msec < min_value) {
                    min_value = total_msec;
                    min_req = it->first;
                }
                if (total_msec > max_value) {
                    max_value = total_msec;
                    max_req = it->first;
                }
            }
        }
        cout << "min req: " << min_req << " value " << min_value << " msec" << endl;
        cout << "max req: " << max_req << " value " << max_value << " msec" << endl;
        cout << "total time: " << total << " total requests: " << num_requests << endl;
        cout << "errors: " << nerrors << endl;
        cout << "avg time: " << (total/num_requests) << endl;
    }
    private:
    std::map<T, U> rmap;
    std::mutex mutex_;
};


#endif /*_REQUEST_MAPPER_H_INCLUDED_*/
