#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <ios>
#include <iterator>
#include <cstddef>

#include <json/value.h>

#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

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

    static Json::Value GetSubscribeMsg(const std::vector<std::string>& symbol_list,
                                       const std::string& secrets_dir)
    {
        Json::Value root;

        std::string channel_name = "level2";

        root["type"] = Json::Value("subscribe");

        Json::Value product_ids(Json::ValueType::arrayValue);
        size_t index = 0;
        for (auto symbol: symbol_list)
        {
            product_ids.insert(index++, symbol);
        }

        root["product_ids"] = product_ids;
        root["channel"] = "level2";

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
