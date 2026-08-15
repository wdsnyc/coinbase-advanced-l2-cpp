#pragma once

#include <atomic>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <iostream>
#include <iomanip>
#include <utility>
#include <tuple>
#include <sstream>
#include <ctime>
#include <functional>
#include <cstdint>
#include <cstdlib>

#include <json/value.h>

struct OrderBook
{
    using BidBook = std::map<double/*px*/, double/*qty*/, std::greater<double>>;
    using AskBook = std::map<double, double, std::less<double>>;

    struct L2PriceBook
    {
        BidBook bidbook;
        AskBook askbook;
    };

    std::atomic<int>                      m_symbolId;
    std::unordered_map<int, L2PriceBook>  m_priceMap;
    std::unordered_map<std::string, int>  m_symbolToId;
    std::vector<std::string>              m_idToSymbol;

    OrderBook(const std::vector<std::string>& symbol_list)
        : m_idToSymbol(symbol_list)
    {
        int symbolId = 0;
        for (const auto &sym : symbol_list)
        {
            m_symbolToId[sym] = symbolId++;
        }

        for (const auto& [sym, id]: m_symbolToId)
        {
            m_priceMap.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(id),
                std::forward_as_tuple(BidBook{}, AskBook{}));
        }
        m_symbolId = 0;
    }

    const L2PriceBook &getL2Book(int symbolId) const { return m_priceMap.at(symbolId); }

    void setSymbolId(int symbolId) { m_symbolId.store(symbolId, std::memory_order_release); }

    int getSymbolId(void) { return m_symbolId.load(std::memory_order_acquire); }

    std::optional<int> findSymbolId(const std::string& sym) const
    {
        if (auto iter = m_symbolToId.find(sym); iter != m_symbolToId.end())
            return iter->second;
        else
            return std::nullopt;
    }

    const std::vector<std::string> &getSymbolList() const { return m_idToSymbol; }

    const std::string& getSymbolStr(int id) { return m_idToSymbol.at(id); }

    int64_t getTime(const std::string& eventTime)
    {
        // 2023-01-17T16:53:49.212563Z
        size_t pos = eventTime.find_first_of('.');
        std::string YMD_HMS = eventTime.substr(0, pos);
        std::string microsStr = eventTime.substr(pos + 1);
        microsStr.erase(microsStr.size() - 1);
        int64_t micros = atol(microsStr.c_str());

        std::tm t = {};
        std::istringstream ss(YMD_HMS);
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail())
            std::cerr << "Parse failed " << eventTime << std::endl;

        time_t secondsSinceEpoch = mktime(&t);
        int64_t nanos =  micros * 1000;
        int64_t secondsInNanos = secondsSinceEpoch * 1E9;
        int64_t nanosSinceEpoch =  secondsInNanos + nanos;
        return nanosSinceEpoch;
    }

    template<typename Book>
    void bookAdd(Book& book, double& px, double& qty)
    {
        auto iter = book.find(px);
        if (iter != book.end())
        {
            if (qty == 0.0)
                book.erase(iter);
            else
                iter->second = qty;
        }
        else
        {
            if (qty != 0.0)
                book.emplace(px, qty);
        }
    }


    bool insertEvent(const Json::Value& symbol, const Json::Value& side, const Json::Value& price, const Json::Value& quantity, const Json::Value& eventTime)
    {
        std::optional<int> product_id = findSymbolId(symbol.asString());
        double px                     = atof(price.asString().c_str());
        double qty                    = atof(quantity.asString().c_str());

        if (!product_id.has_value())
            return false;

        auto iter = m_priceMap.find(product_id.value());

        if (iter == m_priceMap.end())
        {
            return false;
        }

        [[maybe_unused]] int64_t time = getTime(eventTime.asString());

        auto& [bids, asks] = iter->second;

        if (side == "bid")
            bookAdd(bids, px, qty);
        else if (side == "offer")
            bookAdd(asks, px, qty);
        else
            return false;

        return true;
    }
};
