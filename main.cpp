#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <iostream>
#include "third_party/httplib.h"
#include "third_party/json.hpp"

httplib::Server svr;

struct record {
    std::string name;
    int age;
};

nlohmann::json get_params() {
    httplib::Client cli("https://4f065ccf2f163f1acbbd4167a664ad57.m.pipedream.net");
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(10, 0);

    nlohmann::json requestData;
    requestData["key"] = "value";
    requestData["name"] = "test";

    std::string jsonStr = requestData.dump();

    auto res = cli.Post("/", jsonStr, "application/json");

    if (res) {
        std::cout << "Response Status: " << res->status << "\n";
        std::cout << "Response Body: " << res->body << "\n";

        if (res->status == 200 || res->status == 201) {
            if (!res->body.empty()) {
                try {
                    return nlohmann::json::parse(res->body);
                } catch (...) {
                    std::cout << "Failed to parse JSON response\n";
                }
            }
        }
    } else {
        std::cout << "Request failed\n";
    }

    return nlohmann::json::object();
}

int main() {
    std::cout << "starting http server" << std::endl;

    svr.Get("/hi", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("Hello World!", "application/json");
    });

    svr.Post("/create-record", [](const httplib::Request &req, httplib::Response &res) -> void {
        nlohmann::json body = nlohmann::json::parse(req.body);
        auto response_data = get_params();
        std::string  name = response_data["name"];
        res.set_header("Content-Type", "application/json");
        nlohmann::json j;
        j["name"] = name;
        j["age"] = 5;
        res.set_content(j.dump(), "application/json");

    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}

