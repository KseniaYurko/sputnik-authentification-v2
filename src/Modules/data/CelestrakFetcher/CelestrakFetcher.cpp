#include "Modules/data/CelestrakFetcher/CelestrakFetcher.h"
#include <curl/curl.h>
#include <stdexcept>

static size_t writeToString(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

std::string downloadCelestrakTLE(const std::string& group) {
    std::string url =
        "https://celestrak.org/NORAD/elements/gp.php?GROUP=" + group + "&FORMAT=TLE";

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl init failed");
    }

    std::string buffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("download failed");
    }

    return buffer;
}