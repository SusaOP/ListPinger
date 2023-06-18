#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

#include "curl/curl.h"
#include "threadpool.cc"

#ifndef PINGER_H
#define PINGER_H

namespace url_pinger {
class Pinger {
    public:
        Pinger(std::string, int);
        ~Pinger();
        static size_t discardResponseCallback(void*, size_t, size_t, void*);
        std::string getArrangedURL(std::string, int);
        void pingURL(std::string&);
        void pingURLs();
        void pingFailedURLs();
        std::string timeSinceStart();
        std::string timeSinceStartVerbose();

    private:
        void loadUrls(std::string);

        void printProgress();

        std::vector<std::string> urls;
        std::chrono::_V2::system_clock::time_point startTime;
        
        std::vector<std::thread> threads;
        ThreadPool threadPool;

        std::vector<std::string> successful_urls;
        std::vector<std::string> failed_urls;
        std::unordered_map<int, int> httpCodeCounts_;
        int maxConcurrentThreads_;
        int totalThreads_;
        int finishedThreads_;
        std::mutex finishedThreadsMutex_;
        std::condition_variable finishedThreadsCV_;
        std::mutex mutex_;
        std::mutex successful_urls_mutex_;
        std::mutex failed_urls_mutex_;
        std::mutex httpCodeCountMutex_;
        std::string filename;

        std::mutex coutMutex_;
};

} // namespace url_pinger

#endif // PINGER_H
