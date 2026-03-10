//-----------------------------------------------------
// Coinbase Advanced Trade API Level2 market data feed
//-----------------------------------------------------

#include "tls-ca-bundle-pem.h"

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <bits/stdc++.h>
#include <getopt.h>

#include "subscription.h"
#include "orderBook.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

void PrintHelp()
{
   std::cout <<
       "--secrets_dir   Required argument. Directory containing                              \n"
       "                         api_key.txt and api_secret.pem                              \n"
       "                                                                                     \n"
       "                Format of coinbase api_key.txt                                       \n"
       "                                                                                     \n"
       "                    organizations/{organization_id}/apiKeys/{key_uuid}               \n"
       "                                                                                     \n"
       "                    Only one line in this file                                       \n"
       "                                                                                     \n"
       "                Format of coinbase api_secret.pem                                    \n"
       "                                                                                     \n"
       "                    -----BEGIN EC PRIVATE KEY-----                                   \n"
       "                    AbcdEfgHijkLmnopQrstUvWxzyAbcdEfgHijkLmnopQrstUvWxyzAbcdEfgHijkL \n"
       "                    AbcdEfgHijkLmnopQrstUvWxzyAbcdEfgHijkLmnopQrstUvWxyzAbcdEfgHijkL \n"
       "                    AbcdEfgHijkLmnopQrstUvWxzyAbcdEfgHijkLmnopQrstUvWxyzAbcdEfgHijkL \n"
       "                    VxFdxxRcHGZtWugMll6wsOrWgGNhpiTPtw==                             \n"
       "                    -----END EC PRIVATE KEY-----                                     \n"
       "                                                                                     \n"
       "                    No extra spaces before/after lines                               \n"
       "                    Lines contain exactly 64 characters                              \n"
       "                    No quotes                                                        \n"
       "                                                                                     \n"
       "--snapshots:    process snapshot messages\n"
       "--help:         Show help\n";
    exit(1);
}

// https://docs.cdp.coinbase.com/coinbase-app/advanced-trade-apis/websocket/websocket-overview
// https://docs.cdp.coinbase.com/coinbase-app/advanced-trade-apis/websocket/websocket-channels#level2-channel

std::string host = "advanced-trade-ws.coinbase.com";
auto const  port = "443";

int main(int argc, char** argv)
{
    bool process_snapshots = false;
    string secrets_dir;

    auto process_command_line_args = [&process_snapshots, &secrets_dir, &argc, &argv]()
    {
        const char* const short_opts = "e:sh";
        const option long_opts[] = {
            {"secrets_dir", required_argument, nullptr, 'e'},
            {"snapshots",   no_argument,       nullptr, 's'},
            {"help",        no_argument,       nullptr, 'h'},
            {nullptr,       no_argument,       nullptr, 0  }
        };

        while (true)
        {
            const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

            if (-1 == opt)
                break;

            switch (opt)
            {
                case 'e':
                    secrets_dir = optarg;
                    break;
                case 's':
                    process_snapshots = true;
                    break;
                case 'h': // -h or --help
                case '?': // Unrecognized option
                default:
                    PrintHelp();
                    break;
            }
        }
    };

    process_command_line_args();

    if (secrets_dir.empty())
    {
        PrintHelp();
    }

    OrderBook orderBook;

    try
    {
        Json::Value root = coinbase::GetSubscribeMsg(secrets_dir);
        Json::FastWriter fastWriter;
        string subscribe_msg = fastWriter.write(root);
        cout << "-------------------------------------" << endl;
        cout << root.toStyledString() << endl;
        cout << "-------------------------------------" << endl;

        //------------------------------------------------------------
        // From boost beast example: WebSocket SSL client, synchronous
        //------------------------------------------------------------

        // The io_context is required for all I/O
        net::io_context ioc;

        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12_client};

        // This holds the root certificate used for verification
        load_root_certificates(ctx);

        // These objects perform our I/O
        tcp::resolver resolver{ioc};
        websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(get_lowest_layer(ws), results);

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(! SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
            throw beast::system_error(
                beast::error_code(
                    static_cast<int>(::ERR_get_error()),
                    net::error::get_ssl_category()),
                "Failed to set SNI Hostname");

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        host += ':' + std::to_string(ep.port());

        // Perform the SSL handshake
        ws.next_layer().handshake(ssl::stream_base::client);

        // Set a decorator to change the User-Agent of the handshake
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-coro");
            }));

        // Perform the websocket handshake
        ws.handshake(host, "/");

        // Send the message
        ws.write(net::buffer(std::string(subscribe_msg)));

        size_t snapshotNum = 0;
        map<int,size_t> snapshotsMap;

        for (;;)
        {
            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer);

            // The make_printable() function helps print a ConstBufferSequence
            //std::cout << "buffer: " << beast::make_printable(buffer.data()) << std::endl;

            Json::Reader reader;
            Json::Value root;
            std::string data((const char*)buffer.data().data(), buffer.data().size());
            reader.parse(data, root);

            // cout << "+==============================+" << endl;
            // cout << root.toStyledString() << endl;

            // The level2 channel sends a message with fields, type ("snapshot"
            // or "update"), product_id, and updates.

            // The field updates is an array of objects of {price_level,
            // new_quantity, event_time, side} to represent the entire order
            // book. The event_time property is the time of the event as
            // recorded by our trading engine.

            // The new_quantity property is the updated size at that price
            // level, not a delta. A new_quantity of "0" indicates the price
            // level can be removed.

            if (root["type"].type() != Json::ValueType::nullValue)
            {
                Json::FastWriter fastWriter;
                std::string text = fastWriter.write(root);
                std::cout << text << std::endl;

                if (root["type"] == "error")
                    break;
            }
            else if (root["channel"] != Json::ValueType::nullValue)
            {
               /* Messages received
               +=========================+
               {
                   "channel" : "subscriptions",
                   "events" :
                   [
                      {
                        "subscriptions" :
                        {
                          "level2" :
                          [
                              "BTC-USD"
                          ]
                        }
                      }
                    ],
                    "sequence_num" : 3,
                    "timestamp" : "2026-02-27T13:54:08.898945082Z"
                }
                +=========================+
                {
                    "channel" : "l2_data",
                    "events" :
                    [
                      {
                        "product_id" : "BTC-USD",
                        "type" : "update",  (or "snapshot")
                        "updates" :
                        [
                          {
                            "event_time" : "2026-02-27T13:54:08.557825Z",
                            "new_quantity" : "0.00874179",
                            "price_level" : "66244.35",
                            "side" : "bid"
                          },
                          {
                            ...
                          }
                        ]
                      }
                    ],
                    "sequence_num" : 36,
                    "timestamp" : "2026-02-27T13:54:08.937815408Z"
                 } */

                auto orderBookInsert = [&orderBook](Json::Value& side, Json::Value& px, Json::Value& qty, Json::Value& time)
                {
                    if (side == "bid") orderBook.insert(orderBook.m_bids, px, qty, time);
                    else if (side == "offer") orderBook.insert(orderBook.m_asks, px, qty, time);
                    else cerr << "unknown side: " << side << endl;
                };

                Json::Value type;
                size_t snapshotUpdateCount = 0;

                if (root["channel"] == "l2_data")
                {
                    Json::Value events = root["events"];
                    for (const auto& event: events)
                    {
                        type = event["type"];
                        if (type == "snapshot") snapshotNum++;
                        Json::Value updates = event["updates"];
                        for (const auto& update: updates)
                        {
                            if (type == "snapshot") snapshotUpdateCount++;
                            Json::Value side = update["side"];
                            Json::Value px   = update["price_level"];
                            Json::Value qty  = update["new_quantity"];
                            Json::Value time = update["event_time"];
                            // There are around 47,000 updates for BTC-USD in snapshots
                            // Order book BTC-USD ask levels: 28453
                            // Order book BTC-USD bid levels: 18601
                            if ( (type == "snapshot" && process_snapshots) ||
                                 type == "update")
                                orderBookInsert(side, px, qty, time);
                        }
                    }
                }

                if (snapshotUpdateCount > 0) snapshotsMap.insert({snapshotNum, snapshotUpdateCount});

                orderBook.dump(10);
                cout << endl << "snapshotsMap {snapshotNum, numUpdates}: ";
                for (auto& i: snapshotsMap) cout << "{" << i.first << "," << i.second << "},";
                cout << endl << endl;
                orderBook.topOfBook();
            }
            else
            {
                Json::FastWriter fastWriter;
                std::string text = fastWriter.write(root);
                std::cout << text << std::endl;
                break;
            }
        }

        // Close the WebSocket connection
        ws.close(websocket::close_code::normal);

        // If we get here then the connection is closed gracefully
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
