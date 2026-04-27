#include "coinbase_feed.h"
#include <getopt.h>

void PrintHelp()
{
   std::cout <<
       "--symbol_list:  List of symbols separated by commas, e.g \"BTC-USD,ETH-USD\". On     \n"
       "                    start-up the order book of first symbol in symbol list will be   \n"
       "                    displayed. Symbol and order book can be switched by hitting      \n"
       "                    ENTER to pause screen updates and entering a symbol contained    \n"
       "                    in the symbol list.                                              \n"
       "--secrets_dir:  <path to dir>                                                        \n"
       "                    Required argument. Directory containing api_key.txt              \n"
       "                    and api_secret.pem                                               \n"
       "                                                                                     \n"
       "                See API Key Management under Coinbase account settings. Format of    \n"
       "                coinbase api_key.txt                                                 \n"
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
       "--snapshots:    process snapshot messages                                            \n"
       "                                                                                     \n"
       "--help:         Show help                                                            \n";
    exit(1);
}

int main(int argc, char** argv)
{
    bool process_snapshots = false;
    string secrets_dir;
    vector<string> symbol_list;

    auto split = [](string_view str, char delimiter = ',')
    {
        // c++20 split string by delim
        auto split_view = str | views::split(delimiter);

        vector<string> result;

        for (auto subrange : split_view) {
            result.emplace_back(subrange.begin(), subrange.end());
        }

        return result;
    };

    auto process_command_line_args =
        [&split, &process_snapshots, &symbol_list, &secrets_dir, &argc, &argv]()
    {
        const char* const short_opts = "l:e:sh";
        const option long_opts[] = {
            {"symbol_list", required_argument, nullptr, 'l'},
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
                case 'l':
                    symbol_list = split(optarg);
                    break;
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

    if (secrets_dir.empty() || symbol_list.empty())
    {
        PrintHelp();
    }

    coinbase_feed feed(symbol_list, secrets_dir, process_snapshots);

    int status = feed.run();

    return status;
}
