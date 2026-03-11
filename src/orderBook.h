#pragma once

#include <bits/stdc++.h>
#include <json/value.h>

struct OrderBook
{
    //                       price   qty
    using BidBook = std::map<double, double, std::greater<double>>;
    using AskBook = std::map<double, double, std::less<double>>;

    std::string  m_symbol;
    BidBook      m_bids;
    AskBook      m_asks;

    OrderBook(const std::string& symbol) : m_symbol(symbol) {}

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
    void insert(Book& book, const Json::Value& price, const Json::Value& quantity, const Json::Value& eventTime)
    {
        double px    = atof(price.asString().c_str());
        double qty   = atof(quantity.asString().c_str());

        [[maybe_unused]] int64_t time = getTime(eventTime.asString());

        auto iter = book.find(px);
        if ( iter != book.end())
        {
            if (qty == 0.0)
            {
                book.erase(iter);
            }
            else
                iter->second = qty;
        }
        else
        {
            if (qty != 0.0)
            {
                book.insert(std::make_pair(px, qty));
            }
        }
    }

    double printSpread()
    {
        auto ask_px = m_asks.begin()->first;
        auto bid_px = m_bids.begin()->first;
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
        if (m_bids.size() == 0 || m_asks.size() == 0) return;

        std::cout << "\e[2J\e[1;1H"; // Clears screen and moves cursor to top-left
        std::cout << "************** ORDER BOOK ***************\n";
        std::cout << std::fixed;

        std::cout << "Asks size: " << m_asks.size() << std::endl;
        std::cout << "Bids size: " << m_bids.size() << std::endl;

        std::tuple t = getSymbolNameAndCurrency(m_symbol);
        std::cout << std::setw(10) << "price (" << get<1>(t) << ")" << std::setw(14) << "qty (" << get<0>(t) << ")" << std::endl;        
        std::cout
            << std::setfill('-') << std::setw(10) << "-"
            << std::setfill(' ') << std::setw(10) << " "
            << std::setfill('-') << std::setw(10) << "-" << std::setfill(' ') << std::endl;

        auto iter = m_asks.rbegin();
        std::cout << std::setprecision(2) << std::setw(10) << iter->first << std::setw(20) << std::setprecision(8) << iter->second << "  LARGEST ASK\n";
        if (m_asks.size() > num_levels)
            std::advance(iter, m_asks.size() - num_levels);
        while (iter != m_asks.rend())
        {
            std::cout << std::setprecision(2) << std::setw(10) << iter->first << std::setw(20) << std::setprecision(8) << iter->second << "  ASK\n";
            iter++;
        }

        printSpread();

        size_t num_bids = 1;

        for (auto iter: m_bids)
        {
            std::cout << std::setprecision(2) << std::setw(10) << iter.first << std::setw(20) << std::setprecision(8) << iter.second << "  BID\n";
            if (num_bids++ == num_levels)
                break;
        }
        auto smallest_bid = std::prev(m_bids.end());
        std::cout << std::setprecision(2) << std::setw(10) << smallest_bid->first << std::setw(20) << std::setprecision(8) << smallest_bid->second << "  SMALLEST BID\n";
        std::cout << "*****************************************\n";
    }

    void topOfBook()
    {
        if (m_bids.size() == 0 || m_asks.size() == 0) return;

        std::cout << "************** TOP OF BOOK **************\n";
        std::tuple t = getSymbolNameAndCurrency(m_symbol);
        std::cout << std::setw(10) << "price (" << get<1>(t) << ")" << std::setw(14) << "qty (" << get<0>(t) << ")" << std::endl;
        std::cout
            << std::setfill('-') << std::setw(10) << "-"
            << std::setfill(' ') << std::setw(10) << " "
            << std::setfill('-') << std::setw(10) << "-" << std::setfill(' ') << std::endl;

        auto bid = m_bids.begin();
        auto ask = m_asks.begin();

        std::cout << std::setprecision(2) << std::setw(10) << ask->first << std::setw(20) << std::setprecision(8) << ask->second << "  ASK\n";
        printSpread();
        std::cout << std::setprecision(2) << std::setw(10) << bid->first << std::setw(20) << std::setprecision(8) << bid->second << "  BID\n";
        std::cout << "*****************************************\n";
    }
};

