#pragma once

#include <climits>
#include <cmath>
#include <format>
#include <iostream>
#include <string>

#include "orderBook.h"
#include "products.h"

namespace display {

    constexpr const char* kRed   = "\e[31m";
    constexpr const char* kGreen = "\e[32m";
    constexpr const char* kReset = "\e[0m";

    struct Product
    {
        std::string name;
        std::string currency;
    };

    // Splits "BTC-USD" into {name: "BTC", currency: "USD"}.
    static Product GetSymbolNameAndCurrency(const std::string& symbol)
    {
        std::string::size_type n = symbol.find("-");
        std::string name = symbol.substr(0, n);
        std::string currency = symbol.substr(n + 1);
        return Product{name, currency};
    }

    // Number of decimal places to display for a given tick increment,
    // e.g. BTC: 0.01 -> 2, 0.00000001 -> 8. 
    static int DecimalPlaces(double increment)
    {
        return static_cast<int>(std::llround(-std::log10(increment)));
    }

    // 12 price, 16 qty column layout, spread at products price precision
    static double PrintSpread(const OrderBook::BidBook& bids, const OrderBook::AskBook& asks, const coinbase::ProductInfo& product)
    {
        double ask_px = asks.begin()->first;
        double bid_px = bids.begin()->first;
        double spread = ask_px - bid_px;
        std::cout << std::format("{:>12}  Spread {:.{}f}\n", "", spread, DecimalPlaces(product.quote_increment));
        return spread;
    }

    // price right-aligned to 12 chars, qty right-aligned to 16
    static void PrintRow(double price, double qty, const coinbase::ProductInfo& product, const char* color, const std::string& label)
    {
        std::cout << std::format("{}{:>12.{}f} {:>16.{}f}{}  {}\n",
                                 color,
                                 price, DecimalPlaces(product.quote_increment),
                                 qty, DecimalPlaces(product.base_increment),
                                 kReset, label);
    }

    static void PrintHeader(const std::string& symbol)
    {
        Product p = GetSymbolNameAndCurrency(symbol);
        std::cout << std::format("{:>12} {:>16}\n",
                                 std::format("price ({})", p.currency),
                                 std::format("qty ({})", p.name));
        std::cout << std::format("{:->12} {:->16}\n", "", "");
    }
    
    static void Dump(const OrderBook::BidBook& bids,
                     const OrderBook::AskBook& asks,
                     const std::string& symbol,
                     const coinbase::ProductInfo& product,
                     size_t num_levels = UINT_MAX)
    {
        if (bids.size() == 0 || asks.size() == 0) return;

        std::cout << "\e[2J\e[1;1H"; // Clears screen and moves cursor to top-left
        std::cout << "************** ORDER BOOK ***************\n";

        std::cout << "Asks size: " << asks.size() << std::endl;
        std::cout << "Bids size: " << bids.size() << std::endl;

        PrintHeader(symbol);

        auto iter = asks.rbegin();
        PrintRow(iter->first, iter->second, product, kRed, "LARGEST ASK");
        if (asks.size() > num_levels)
            std::advance(iter, asks.size() - num_levels);
        while (iter != asks.rend())
        {
            PrintRow(iter->first, iter->second, product, kRed, "ASK");
            iter++;
        }

        PrintSpread(bids, asks, product);

        size_t num_bids = 1;
        for (auto iter: bids)
        {
            PrintRow(iter.first, iter.second, product, kGreen, "BID");
            if (num_bids++ == num_levels)
                break;
        }
        auto smallest_bid = std::prev(bids.end());
        PrintRow(smallest_bid->first, smallest_bid->second, product, kGreen, "SMALLEST BID");

        std::cout << "*****************************************\n";
    }

    static void TopOfBook(const OrderBook::BidBook& bids, const OrderBook::AskBook& asks, const std::string& symbol, const coinbase::ProductInfo& product)
    {
        if (bids.size() == 0 || asks.size() == 0) return;

        std::cout << "************** TOP OF BOOK **************\n";
        PrintHeader(symbol);

        auto bid = bids.begin();
        auto ask = asks.begin();

        PrintRow(ask->first, ask->second, product, kRed, "ASK");
        PrintSpread(bids, asks, product);
        PrintRow(bid->first, bid->second, product, kGreen, "BID");

        std::cout << "*****************************************\n";
    }
}
