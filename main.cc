#include <iostream>

#include "pinger.cc"

using namespace url_pinger;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <filename> <max_concurrent_threads>" << std::endl;
        exit(1);
    }

    std::string filename = argv[1];
    int maxConcurrentThreads = atoi(argv[2]);

    //Pinger pinger(filename, maxConcurrentThreads);

    Pinger* ping = new Pinger(filename, maxConcurrentThreads);

    try {
        ping->pingURLs();
        //std::string tempURL = "myspace.com";
        //pinger.pingURL(tempURL);
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}