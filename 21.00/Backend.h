#pragma once
#define CURL_STATICLIB

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <map>
#include <format>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>

#include "curl/curl.h"
#include <algorithm>
#include <iostream>

#pragma comment(lib, "curl/libcurl.lib")

using namespace std;

static string MMS_URL = "http://192.168.0.227:3551";
static string GameserverSession = "";

static std::string SendRequest(const std::string& url, const std::string& method) {
    CURL* curl;
    CURLcode res;
    long response_code;
    std::string response_data;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, std::string* data) -> size_t {
            data->append(static_cast<char*>(ptr), size * nmemb);
            return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            std::cout << method << " Response code: " << response_code << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    else {
        std::cerr << "Failed to initialize curl handle." << std::endl;
    }

    return response_data;
}

static std::string SendRequestBody(const std::string& url, const std::string& method, const std::string& jsonBody = "") {
    CURL* curl;
    CURLcode res;
    long response_code;
    std::string response_data;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, std::string* data) -> size_t {
            data->append(static_cast<char*>(ptr), size * nmemb);
            return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            std::cout << method << " Response code: " << response_code << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    else {
        std::cerr << "Failed to initialize curl handle." << std::endl;
    }

    return response_data;
}

std::string GenerateRandomUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 15);

    std::stringstream yes;
    for (int i = 0; i < 8; ++i) {
        yes << std::hex << dis(gen);
    }
    yes << "-";
    for (int i = 0; i < 4; ++i) {
        yes << std::hex << dis(gen);
    }
    yes << "-4";
    for (int i = 0; i < 3; ++i) {
        yes << std::hex << dis(gen);
    }
    yes << "-";
    for (int i = 0; i < 4; ++i) {
        yes << std::hex << dis(gen);
    }
    yes << "-";
    for (int i = 0; i < 12; ++i) {
        yes << std::hex << dis(gen);
    }

    return yes.str();
}

static std::string GetCurrentProcessPID() {
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif
    std::stringstream ss;
    ss << pid;
    return ss.str();
}

static void ServerIsReady(bool real) {
    if (bBackend) {
        if (real) {
            if (GameserverSession == "") {
                GameserverSession = GenerateRandomUUID();
            }

            string PID = GetCurrentProcessPID();

            std::string jsonString = "{\n";
            jsonString += "  \"serverAddress\": \"127.0.0.1\",\n";
            jsonString += "  \"serverAvailable\": \"true\",\n";
            jsonString += "  \"serverUUID\": \"" + GameserverSession + "\",\n";
            jsonString += "  \"serverPID\": \"" + PID + "\"\n";
            jsonString += "}\n";

            SendRequestBody(MMS_URL + "/api/v2/dedicated/service_update", "POST", jsonString);
        }
        else {
            string PID = GetCurrentProcessPID();

            std::string jsonString = "{\n";
            jsonString += "  \"serverAddress\": \"127.0.0.1\",\n";
            jsonString += "  \"serverAvailable\": \"false\",\n";
            jsonString += "  \"serverUUID\": \"" + GameserverSession + "\",\n";
            jsonString += "  \"serverPID\": \"" + PID + "\"\n";
            jsonString += "}\n";

            SendRequestBody(MMS_URL + "/api/v2/dedicated/service_update", "POST", jsonString);
        }
    }
    else {
        std::cout << "Backend is disabled because backend = false!" << std::endl;
    }
}
