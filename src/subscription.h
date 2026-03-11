#pragma once

#include <bits/stdc++.h>
#include <json/json.h>
#include <pybind11/embed.h>

namespace py = pybind11;

//------------------------------------------------------------------------------
// Coinbase Advanced Trade API Subscription
//------------------------------------------------------------------------------

/* Subscribe
   {
     "type"        : "subscribe",
     "product_ids" : ["ETH-USD", "ETH-EUR", ...],
     "channel"     : "level2",
     "jwt"         : "abcd..",
   }
*/

constexpr const char* api_key_txt = "/api_key.txt";
constexpr const char* api_secret_pem = "/api_secret.pem";

namespace coinbase {

    static Json::Value GetSubscribeMsg(const std::string& symbol_name,
                                       const std::string& secrets_dir)
    {
        Json::Value root;

        std::string channel_name = "level2";
        std::vector<std::string> crypto_ids{symbol_name};

        root["type"] = Json::Value("subscribe");

        Json::Value product_ids(Json::ValueType::arrayValue);
        size_t index = 0;
        for (auto i: crypto_ids)
        {
            product_ids.insert(index++, i);
        }

        root["product_ids"] = product_ids;
        root["channel"] = "level2";

        py::scoped_interpreter guard{};

        py::module_ jwt_generator = py::module_::import("coinbase.jwt_generator");

        std::string api_key;
        std::ifstream api_key_ifstrm(secrets_dir + api_key_txt);
        api_key_ifstrm >> api_key;

        std::ifstream api_secret_ifstrm(secrets_dir + api_secret_pem, std::ios::in | std::ios::binary);
        std::string api_secret((std::istreambuf_iterator<char>(api_secret_ifstrm)),
                               std::istreambuf_iterator<char>());

        py::object jwt_token = jwt_generator.attr("build_ws_jwt")(api_key, api_secret);
        std::string token = py::str(jwt_token);

        root["jwt"] = token;

        return root;
    }
}

