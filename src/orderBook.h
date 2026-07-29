#pragma once

#include <bits/stdc++.h>
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

    std::string                                  m_symbol;
    std::vector<std::string>                     m_symbolList;
    std::unordered_map<std::string, L2PriceBook> m_priceMap;

    OrderBook(const std::vector<std::string>& symbol_list)
        : m_symbolList(symbol_list)
    {
        for (const auto& sym: m_symbolList)
        {
            m_priceMap.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(sym),
                std::forward_as_tuple(BidBook{}, AskBook{}));
        }
        m_symbol = m_symbolList.front();
    }

    const std::vector<std::string> getSymbolList() const
    {
        return m_symbolList;
    }

    bool setSymbol(const std::string& symbol)
    {
        auto iter = find(m_symbolList.begin(), m_symbolList.end(), symbol);
        if (iter != m_symbolList.end())
        {
            m_symbol = *iter;
            return true;
        }
        else
        {
            std::cerr << "Symbol not in symbol_list" << std::endl;
            return false;
        }
    }

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
        std::string product_id = symbol.asString();
        double px              = atof(price.asString().c_str());
        double qty             = atof(quantity.asString().c_str());

        auto iter = m_priceMap.find(product_id);

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

    double printSpread(const BidBook& bids, const AskBook& asks)
    {
        auto ask_px = asks.begin()->first;
        auto bid_px = bids.begin()->first;
        double spread = ask_px - bid_px;
        std::cout << std::setfill(' ') << std::setw(20) << "Spread : " << ask_px - bid_px << std::endl;
        return spread;
    }

    std::tuple<std::string,std::string> getSymbolNameAndCurrency(const std::string symbol)
    {
        std::string::size_type n = symbol.find("-");
        std::string name = symbol.substr(0, n);
        std::string currency = symbol.substr(n+1);
        return std::tuple{name,currency};
    }

    void dump(size_t num_levels = UINT_MAX)
    {
        const L2PriceBook& l2_book = m_priceMap[m_symbol];
        const BidBook& bids = l2_book.bidbook;
        const AskBook& asks = l2_book.askbook;

        if (bids.size() == 0 || asks.size() == 0) return;

        std::cout << "\e[2J\e[1;1H"; // Clears screen and moves cursor to top-left
        std::cout << "************** ORDER BOOK ***************\n";
        std::cout << std::fixed;

        std::cout << "Asks size: " << asks.size() << std::endl;
        std::cout << "Bids size: " << bids.size() << std::endl;

        std::tuple t = getSymbolNameAndCurrency(m_symbol);
        std::cout << std::setw(10) << "price (" << get<1>(t) << ")" << std::setw(14) << "qty (" << get<0>(t) << ")" << std::endl;
        std::cout
            << std::setfill('-') << std::setw(10) << "-"
            << std::setfill(' ') << std::setw(10) << " "
            << std::setfill('-') << std::setw(10) << "-" << std::setfill(' ') << std::endl;

        auto iter = asks.rbegin();
        std::cout << std::setprecision(2) << std::setw(10) << iter->first << std::setw(20) << std::setprecision(8) << iter->second << "  LARGEST ASK\n";
        if (asks.size() > num_levels)
            std::advance(iter, asks.size() - num_levels);
        while (iter != asks.rend())
        {
            std::cout << std::setprecision(2) << std::setw(10) << iter->first << std::setw(20) << std::setprecision(8) << iter->second << "  ASK\n";
            iter++;
        }

        printSpread(bids, asks);

        size_t num_bids = 1;

        for (auto iter: bids)
        {
            std::cout << std::setprecision(2) << std::setw(10) << iter.first << std::setw(20) << std::setprecision(8) << iter.second << "  BID\n";
            if (num_bids++ == num_levels)
                break;
        }
        auto smallest_bid = std::prev(bids.end());
        std::cout << std::setprecision(2) << std::setw(10) << smallest_bid->first << std::setw(20) << std::setprecision(8) << smallest_bid->second << "  SMALLEST BID\n";
        std::cout << "*****************************************\n";
    }

    void topOfBook()
    {
        const L2PriceBook& l2_book = m_priceMap[m_symbol];
        const BidBook& bids = l2_book.bidbook;
        const AskBook& asks = l2_book.askbook;

        if (bids.size() == 0 || asks.size() == 0) return;

        std::cout << "************** TOP OF BOOK **************\n";
        std::tuple t = getSymbolNameAndCurrency(m_symbol);
        std::cout << std::setw(10) << "price (" << get<1>(t) << ")" << std::setw(14) << "qty (" << get<0>(t) << ")" << std::endl;
        std::cout
            << std::setfill('-') << std::setw(10) << "-"
            << std::setfill(' ') << std::setw(10) << " "
            << std::setfill('-') << std::setw(10) << "-" << std::setfill(' ') << std::endl;

        auto bid = bids.begin();
        auto ask = asks.begin();

        std::cout << std::setprecision(4) << std::setw(10) << ask->first << std::setw(20) << std::setprecision(8) << ask->second << "  ASK\n";
        printSpread(bids, asks);
        std::cout << std::setprecision(4) << std::setw(10) << bid->first << std::setw(20) << std::setprecision(8) << bid->second << "  BID\n";
        std::cout << "*****************************************\n";
    }
};
