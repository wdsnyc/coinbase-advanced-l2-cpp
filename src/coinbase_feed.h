//-----------------------------------------------------
// Coinbase Advanced Trade API Level2 market data feed
//-----------------------------------------------------

#pragma once

#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <cstdlib>
#include <limits>
#include <format>
#include <tuple>
#include <utility>
#include <exception>

#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include "tls-ca-bundle-pem.h"
#include "subscription.h"
#include "orderBook.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// https://docs.cdp.coinbase.com/coinbase-app/advanced-trade-apis/websocket/websocket-overview
// https://docs.cdp.coinbase.com/coinbase-app/advanced-trade-apis/websocket/websocket-channels#level2-channel

class coinbase_feed
{
    static inline const std::string host = "advanced-trade-ws.coinbase.com";
    static inline const std::string port = "443";

protected:
    OrderBook         m_orderBook;
    std::string       m_secretsDir;
    bool              m_processSnapshots;
    std::atomic<bool> m_orderBookDump;
    std::thread       m_inputThread;

public:
    coinbase_feed(const std::vector<std::string>& symbol_list,
                  const std::string& secrets_dir,
                  bool process_snapshots)
        : m_orderBook(symbol_list),
          m_secretsDir(secrets_dir),
          m_processSnapshots(process_snapshots),
          m_orderBookDump(true)
    {}

    // Thread for switching symbol and order book output
    void readFromStdinThread()
    {
        for (;;)
        {
            std::cout << "\nPress ENTER to change symbol..." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            m_orderBookDump.store(false, std::memory_order_release);

            std::cout << "Valid symbols: ";
            for (const auto& sym : m_orderBook.getSymbolList())
                std::cout << sym << ",";
            std::cout << std::endl;

            std::cout << "Enter symbol: ";
            std::string symbol;
            std::getline(std::cin, symbol);

            std::optional<int> symbolId = m_orderBook.findSymbolId(symbol);
            if (symbolId.has_value())
            {
                m_orderBook.setSymbolId(symbolId.value());
                m_orderBookDump.store(true, std::memory_order_release);
            }
            else
            {
                std::cout << "Invalid symbol. Press ENTER to retry." << std::endl;
            }
        }
    }

    int run()
    {
        try
        {
            Json::Value root = coinbase::GetSubscribeMsg(m_orderBook.getSymbolList(), m_secretsDir);
            Json::FastWriter fastWriter;
            std::string subscribe_msg = fastWriter.write(root);
            std::cout << "-------------------------------------" << std::endl;
            std::cout << "subcribe\n";
            std::cout << "-------------------------------------" << std::endl;
            std::cout << root.toStyledString() << std::endl;
            std::cout << "-------------------------------------" << std::endl;

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
            std::string host_ = host + ':' + std::to_string(ep.port());

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
            ws.handshake(host_, "/");

            // Send the message
            ws.write(net::buffer(std::string(subscribe_msg)));

            size_t snapshotNum = 0;
            std::map<std::string, std::pair<int,size_t>> snapshotsMap;

            m_inputThread = std::thread(&coinbase_feed::readFromStdinThread, this);

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

                // std::cout << "+==============================+" << std::endl;
                // std::cout << root.toStyledString() << std::endl;

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
                           snapshot
                       +=========================+
                       {
                         "channel" : "l2_data",
                         "events" :
                         [
                             {
                               "product_id" : "BTC-USD",
                               "type" : "snapshot",
                               "updates" :
                               [
                                 ...
                               ]
                             }
                         ]
                         "sequence_num" : 0,
                         "timestamp" : "2026-02-27T13:54:08.898945082Z"
                       }

                       +=========================+
                           subscriptions
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
                                   "BTC-USD", ETH-USD"
                                   ]
                               }
                           }
                         ],
                         "sequence_num" : 1,
                         "timestamp" : "2026-02-27T13:54:08.898945082Z"
                       }

                       +=========================+
                       update
                       +=========================+
                       {
                         "channel" : "l2_data",
                         "events" :
                           [
                             {
                               "product_id" : "BTC-USD",
                               "type" : "update",
                               "updates" :
                               [
                                 {
                                  "event_time" : "2026-02-27T13:54:08.557825Z",
                                  "new_quantity" : "0.00874179",
                                  "price_level" : "66244.35",
                                  "side" : "bid" or "offer"
                                 },
                                 {
                                   ...
                                 }
                              ]
                            }
                          ],
                          "sequence_num" : 2,
                          "timestamp" : "2026-02-27T13:54:08.937815408Z"
                       } */

                    Json::Value type;
                    size_t snapshotUpdateCount = 0;

                    if (root["channel"] == "l2_data")
                    {
                        Json::Value events = root["events"];
                        for (const auto& event: events)
                        {
                            type = event["type"];
                            Json::Value product_id = event["product_id"];
                            if (type == "snapshot") snapshotNum++;
                            Json::Value updates = event["updates"];
                            for (const auto& update: updates)
                            {
                                if (type == "snapshot") snapshotUpdateCount++;
                                Json::Value side = update["side"];
                                Json::Value px   = update["price_level"];
                                Json::Value qty  = update["new_quantity"];
                                Json::Value time = update["event_time"];
                                // There are around 47,000 updates for BTC-USD for a snapshot message
                                if ( (type == "snapshot" && m_processSnapshots) || type == "update")
                                {
                                    m_orderBook.insertEvent(product_id, side, px, qty, time);
                                }
                                if (type == "snapshot")
                                {
                                    std::string sym = product_id.asString();
                                    snapshotsMap.emplace(
                                        std::piecewise_construct,
                                        std::forward_as_tuple(sym),
                                        std::forward_as_tuple(snapshotNum, snapshotUpdateCount));
                                }
                            }
                        }
                    }

                    if (m_orderBookDump.load(std::memory_order_acquire))
                    {
                        int symbolId = m_orderBook.getSymbolId();
                        m_orderBook.dump(symbolId, 10);
                        std::cout << std::endl;
                        for (auto& [sym, data]: snapshotsMap)
                            std::cout << std::format("snapshotsMap [sym,seqno,cnt]: [{},{},{}]\n", sym, data.first, data.second);
                        std::cout << std::endl;
                        m_orderBook.topOfBook(symbolId);
                    }
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
};
