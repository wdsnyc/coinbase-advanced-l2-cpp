#pragma once

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <json/reader.h>
#include <json/value.h>

#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include "tls-ca-bundle-pem.h"
#include "subscription.h"  // reuses api_key_txt / api_secret_pem constants

namespace py = pybind11;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace coinbase {
    
    struct ProductInfo
    {
        std::string product_id;
        // minimum price increment (e.g., $0.01 for BTC/USD), 
        // order prices are be multiples of this value.
        double quote_increment;
        // minimum quantity increment for the base currency (e.g.,
        // 0.00000001 BTC)
        double      base_increment;

        int64_t PriceToTicks(double price) const
        {
            return llround(price / quote_increment);
        }

        int64_t SizeToTicks(double qty) const
        {
            return llround(qty / base_increment);
        }

        double TicksToPrice(int64_t ticks) const
        {
            return static_cast<double>(ticks) * quote_increment;
        }

        double TicksToSize(int64_t ticks) const
        {
            return static_cast<double>(ticks) * base_increment;
        }
    };

    static inline const std::string rest_host = "api.coinbase.com";
    static inline const std::string rest_port = "443";

    // REST JWTs carry a "uri" claim identifying the specific request
    static std::string BuildRestJwt(const std::string& method,
                                    const std::string& path,
                                    const std::string& api_key,
                                    const std::string& api_secret)
    {
        py::module_ jwt_generator = py::module_::import("coinbase.jwt_generator");

        py::object uri_obj = jwt_generator.attr("format_jwt_uri")(method, path);
        std::string uri = py::str(uri_obj);

        py::object jwt_obj = jwt_generator.attr("build_rest_jwt")(uri, api_key, api_secret);
        return py::str(jwt_obj);
    }

    // Fetches raw product JSON for one product_id
    // plain HTTP GET instead of a websocket handshake.
    static std::string FetchProductRaw(const std::string& product_id,
                                       const std::string& secrets_dir)
    {
        std::string api_key;
        std::ifstream api_key_ifstrm(secrets_dir + api_key_txt);
        api_key_ifstrm >> api_key;

        std::ifstream api_secret_ifstrm(secrets_dir + api_secret_pem, std::ios::in | std::ios::binary);
        std::string api_secret((std::istreambuf_iterator<char>(api_secret_ifstrm)),
                               std::istreambuf_iterator<char>());

        std::string path = "/api/v3/brokerage/products/" + product_id;
        std::string jwt = BuildRestJwt("GET", path, api_key, api_secret);

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ::load_root_certificates(ctx);

        tcp::resolver resolver{ioc};
        beast::ssl_stream<tcp::socket> stream{ioc, ctx};

        if (! SSL_set_tlsext_host_name(stream.native_handle(), rest_host.c_str()))
            throw beast::system_error(
                beast::error_code(
                    static_cast<int>(::ERR_get_error()),
                    net::error::get_ssl_category()),
                "Failed to set SNI Hostname");

        auto const results = resolver.resolve(rest_host, rest_port);
        net::connect(stream.next_layer(), results);
        stream.handshake(ssl::stream_base::client);

        http::request<http::empty_body> req{http::verb::get, path, 11};
        req.set(http::field::host, rest_host);
        req.set(http::field::user_agent, "coinbase-advanced-l2-cpp");
        req.set(http::field::authorization, "Bearer " + jwt);

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.shutdown(ec); // stream_truncated here is expected, same as any sync Beast client

        return res.body();
    }

    static ProductInfo FetchProduct(const std::string& product_id,
                                    const std::string& secrets_dir)
    {
        std::string raw = FetchProductRaw(product_id, secrets_dir);

        Json::Reader reader;
        Json::Value root;
        reader.parse(raw, root);

        ProductInfo info;
        info.product_id = root["product_id"].asString();
        info.quote_increment = std::stod(root["quote_increment"].asString());
        info.base_increment = std::stod(root["base_increment"].asString());
        return info;
    }

    // ProducInfo table, keyed by product_id
    static std::map<std::string, ProductInfo> BuildProductInfoTable(
        const std::vector<std::string>& symbol_list,
        const std::string& secrets_dir)
    {
        std::map<std::string, ProductInfo> table;
        for (const auto& symbol : symbol_list)
        {
            table.emplace(symbol, FetchProduct(symbol, secrets_dir));
        }
        return table;
    }

}
