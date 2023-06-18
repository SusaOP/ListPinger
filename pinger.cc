#include <iostream>
#include <cstdlib>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <iomanip>

#include "curl/curl.h"
#include "pinger.h"

//using namespace std;
namespace url_pinger {

    Pinger::Pinger(std::string fname, int maxConcurrentThreads = 10)
        : threadPool(maxConcurrentThreads), maxConcurrentThreads_(maxConcurrentThreads), totalThreads_(0), finishedThreads_(0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        filename = fname;
        loadUrls(fname);
        threads.reserve(urls.size());
    }

    Pinger::~Pinger() {
        
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        curl_global_cleanup();
        
    }

    size_t Pinger::discardResponseCallback(void* contents, size_t size, size_t nmemb, void* userptr) {
        // Discard the response data
        return size * nmemb;
    }

    std::string Pinger::getArrangedURL(std::string originalUrl, int arrangement_){
        switch (arrangement_){
            case 0:
                // strip original url of any prefixes
                if (originalUrl.length() > 12){
                    if (originalUrl.substr(0,12) == "https://www.") return originalUrl.substr(12, originalUrl.length());
                    if (originalUrl.substr(0,8) == "https://") return originalUrl.substr(8, originalUrl.length());
                    if (originalUrl.substr(0,4) == "www.") return originalUrl.substr(4, originalUrl.length());
                    return originalUrl;
                } else if (originalUrl.length() > 8){
                    if (originalUrl.substr(0,8) == "https://") return originalUrl.substr(8, originalUrl.length());
                    if (originalUrl.substr(0,4) == "www.") return originalUrl.substr(4, originalUrl.length());
                    return originalUrl;
                } else if (originalUrl.length() > 4){
                    if (originalUrl.substr(0,4) == "www.") return originalUrl.substr(4, originalUrl.length());
                    return originalUrl;
                } else {
                    return originalUrl;
                }

            case 1:
                // add "https://" to start
                return "https://" + originalUrl;     // e.g. "https://<URL>
            case 2:
                // Remove previous "https://", add "www."
                return "www."+originalUrl.substr(8, originalUrl.length());       // e.g. "www.<URL>"
            case 3:
                // Add "https://" to the existing "www.", resulting in "https://www."
                return "https://"+originalUrl;       // e.g. "https://www.<URL>
            default:
                // Unknown circumstances
                std::cout << timeSinceStart() << "Else reached for " << originalUrl << std::endl;
                return originalUrl;
        }
    }

    void Pinger::pingURL(std::string& url) {
        

        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << timeSinceStart() << "Error initializing curl" << std::endl;
            exit(1);
        }
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardResponseCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        int arrangement = 0;
        while (true){

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            CURLcode res = curl_easy_perform(curl);

            long httpCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // Store the result in local variables to avoid concurrent access to shared vectors
            std::string result;

            if (res == CURLE_OK) {
                if (httpCode == 200) {
                    result = url + " - Success (200)";
                    {
                        std::lock_guard<std::mutex> lock_success(successful_urls_mutex_);
                        std::lock_guard<std::mutex> lock_http(httpCodeCountMutex_);
                        ++httpCodeCounts_[httpCode];
                        successful_urls.push_back(result);
                    }
                    arrangement = 0;
                    break;
                } else if (httpCode == 403){
                    result = url + " - Error: Forbidden (403)";

                    {
                        std::lock_guard<std::mutex> lock_fail(failed_urls_mutex_);
                        std::lock_guard<std::mutex> lock_http(httpCodeCountMutex_);
                        failed_urls.push_back(result);
                        ++httpCodeCounts_[httpCode];
                    }
                    arrangement = 0;
                    break;
                    
                } else if (httpCode == 404) {
                    result = url + " - Error: Not Found (404)";
                } else {
                    result = url + " - Error: HTTP Status Code " + std::to_string(httpCode);
                }
            } else {
                result = url + " - Error: " + curl_easy_strerror(res);
            } // end of if-else for CURLcode


            if (arrangement > 3){
                {
                    std::lock_guard<std::mutex> lock_fail(failed_urls_mutex_);
                    std::lock_guard<std::mutex> lock_http(httpCodeCountMutex_);

                    failed_urls.push_back(result);
                    ++httpCodeCounts_[httpCode];
                }
                arrangement = 0;
                break;
            } else {
                // check if timed out
                if (res == CURLE_OPERATION_TIMEDOUT){
                    std::lock_guard<std::mutex> lockCout(coutMutex_);
                    std::cout.flush();
                    std::cout << "\r";
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L - (3 * (arrangement+1)));
                    std::cout << timeSinceStart() << url << " timed out."<< std::endl;
                    std::cout << std::flush;

                    //lower timeout by 3 times the arrangement number
                } 

                url = getArrangedURL(url, arrangement);
                ++arrangement;
            }

        }


        // Clean up cURL
        curl_easy_cleanup(curl);

        {
            std::lock_guard<std::mutex> lock(finishedThreadsMutex_);
            ++finishedThreads_;
            printProgress();
        }
        finishedThreadsCV_.notify_all();
    }

    void Pinger::pingURLs() {
        std::mutex activeThreadsMutex;
        std::atomic<int> activeThreads(0);

        startTime = std::chrono::_V2::system_clock::time_point(std::chrono::high_resolution_clock::now());

        for (auto& url : urls) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++totalThreads_;
            }

            threadPool.enqueue([this, &url, &activeThreads, &activeThreadsMutex]() {
                {
                    std::lock_guard<std::mutex> lock(activeThreadsMutex);
                    ++activeThreads;
                }
                pingURL(url);
                {
                    std::lock_guard<std::mutex> lock(activeThreadsMutex);
                    --activeThreads;
                }
            });
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            finishedThreadsCV_.wait(lock, [this] { return finishedThreads_ == totalThreads_; });
            
            std::lock_guard<std::mutex> lockCout(coutMutex_);
            std::cout << std::flush;
            std::cout << timeSinceStart() << "Finished." << std::endl;
            
        }

        std::cout.flush();
        
        std::cout << timeSinceStart() <<"Duration: " << timeSinceStartVerbose() << std::endl;

        std::cout << timeSinceStart() <<"Successful URLs: " << successful_urls.size() << "/" << urls.size() << " ("
             << (static_cast<double>(successful_urls.size()) / urls.size() * 100.0) << "%)" << std::endl;

        std::ofstream file("failed_urls-" + filename);
        for (const auto& url : failed_urls) {
            file << url << std::endl;
        }
        file.close();

        std::ofstream file2("successful_urls-" + filename);
        for (const auto& url : successful_urls) {
            file2 << url << std::endl;
        }
        file2.close();

        // Print the HTTP status codes
        std::cout << "\n" << timeSinceStart() << "HTTP Status Codes:" << std::endl;
        for (const auto& httpCodeCount : httpCodeCounts_) {
            std::cout << "\t" << httpCodeCount.first << ": " << httpCodeCount.second << "  (" << (static_cast<double>(httpCodeCount.second) / urls.size() * 100.0) << "%)" << std::endl;   
        }
    }

    std::string Pinger::timeSinceStart() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        //parse time since start into minutes and seconds
        int minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
        int seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count() % 60;
        int milliseconds = duration.count() % 1000;
        
        std::stringstream ss;
        // Format with leading zeros
        ss << "[";
        ss << std::setfill('0') << std::setw(2) << minutes << ':';
        ss << std::setfill('0') << std::setw(2) << seconds << '.';
        ss << std::setfill('0') << std::setw(3) << milliseconds;
        ss << "] ";

        return ss.str();
    }

    std::string Pinger::timeSinceStartVerbose() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        //parse time since start into minutes and seconds
        int minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
        int seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count() % 60;
        int milliseconds = duration.count() % 1000;
        
        std::stringstream ss;
        ss << minutes << " minutes " << seconds << " seconds " << milliseconds << " ms.";

        return ss.str();
    }

    void Pinger::loadUrls(std::string filename) {
        std::ifstream file(filename);
        std::string line;
        while (getline(file, line)) {
            if (!line.empty() && line.size() > 3)
                urls.push_back(line);
        }
    }

    void Pinger::printProgress(){
        static const int progressBarWidth = 40;

        float progress = static_cast<float>(finishedThreads_) / urls.size();
        int barWidth = static_cast<int>(progress * progressBarWidth);
        {
            std::string progressBar = ""+timeSinceStart() + "[";
            for (int i = 0; i < progressBarWidth; ++i) {
                if (i < barWidth){
                    progressBar += "=";
                } else if (i == barWidth) {
                    progressBar += ">";
                } else {
                    progressBar += " ";
                }
            }
            progressBar += "] " + std::to_string(finishedThreads_) + "/" + std::to_string(urls.size());
            progressBar += "  " + std::to_string(int(progress * 100.0)) + "%";
            
            {
                std::lock_guard<std::mutex> lockCout(coutMutex_);
                
                std::cout << "\r";
                std::cout << progressBar << "\r";
                std::cout.flush();
                
            }
        }
    }

} // namespace url_pinger

