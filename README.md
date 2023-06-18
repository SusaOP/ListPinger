# List Pinger
- Uses a Thread Pool to concurrently ping a list of URLs loaded from file and outputs which connections succeeded and failed.
- Allows user to input the maximum number of threads they want active on the process.

## Requirements
cURL C++ library (https://github.com/curl/curl/tree/master/include/curl)

## Compilation Instructions
1. ```g++ main.cc -lcurl```
2. ```./a.out <filename> <max_concurrent_threads>```

## Example Output
Using a set of 500 URLs and 16 threads, this was one output:
Command: ``` ./a.out 500.csv 16 ```
```
[00:59.390] Finished.
[00:59.390] Duration: 0 minutes 59 seconds 390 ms.
[00:59.390] Successful URLs: 423/500 (84.6%)

[00:59.391] HTTP Status Codes:
        503: 1  (0.2%)
        301: 1  (0.2%)
        405: 1  (0.2%)
        302: 5  (1%)
        406: 1  (0.2%)
        404: 4  (0.8%)
        416: 1  (0.2%)
        520: 1  (0.2%)
        403: 41  (8.2%)
        0: 20  (4%)
        200: 424  (84.8%)
```
Upon completion, the program outputs the total duration and the percentage of successful connections (HTTP Response 200 - "OK").
Next it gives all the different encountered response codes from the dataset.
In addition to the terminal output, 2 new files are generated: "failed_urls-500.csv" and "successful_urls-500.csv", which go into more detail about the specific URLs and their individual results.

## Potential Reasons for Slow Execution
In general, the program should be able to quickly iterate through a large number of sites. Slowdowns occur primarily due to timeouts. If you decide to use high timeouts (10-30 seconds+), be aware that the overhead may grow dramatically. Since the program concurrently checks several URLs at a time, it is likely that the progress bar will get close to finished before slowing dramatically. This is simply due to a few threads attempting to contact their respective server, and eventually getting timed out. 

### Disclaimers
I am not responsible for any damages, misuse, or misconduct performed using this program. Do not use or modify it in attempts to throttle a web server, or any other nefarious acts. 
I created this simply because I wanted a way to quickly filter through large sets of URLs to see if they were still obviously online, saving the trouble of visiting every single one. 
