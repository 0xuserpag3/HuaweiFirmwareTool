#include <iomanip>
#include <sstream>
#include "util.hpp"
#include "util_hw.hpp"
#include "util_rsa.hpp"

int
main(int argc, char *argv[])
{
    auto usage_print = [&]() {
        usage(
            {
                argv[0],
                "-d /path/to/items",
                "-k private_key.pem",
                "-o items/var/signature",
            },
            {
                "-d Path to unpacked files",
                "-k Path from private_key.pem (Without password)",
                "-o Path to save signature file",
            });
    };

    std::string path_items, path_sig_item_list;
    std::string path_key_priv, path_out_sig;

    for (int opt; (opt = getopt(argc, argv, "d:k:o:")) != -1;) {
        switch (opt) {
            case 'd':
                path_items = optarg;
                break;
            case 'k':
                path_key_priv = optarg;
                break;
            case 'o':
                path_out_sig = optarg;
                break;
        }
    }

    if (path_items.empty() || path_key_priv.empty() || path_out_sig.empty()) {
        usage_print();
    }

    path_sig_item_list = FilePathOnFS(path_items, "/sig_item_list.txt");

    try {

        Firmware firmware = {};

        std::stringstream sig_data;
        std::fstream sig_item_list;

        std::string RSA_key_private =
            FileRead(path_key_priv, std::ios::in | std::ios::binary);

        sig_item_list = FileOpen(path_sig_item_list, std::ios::in);

        for (std::string line; std::getline(sig_item_list, line);) {

            struct huawei_item hi = {};

            if (line.size() <= 2) {
                continue;
            }

            if (line.front() != '+') {
                std::cout << "[ - ] Verify Item Skip: " << line.substr(2) << std::endl;
                continue;
            }

            line.erase(0, 2);

            std::cout << "[ + ] Verify Item Add: " << line << std::endl;

            std::snprintf(hi.item, sizeof(hi.item), "%s", line.c_str());
            firmware.AddItemHeader(hi);
        }

        if (firmware.getItemsHeader().empty()) {
            throw_err("Empty items on header", "Count of items");
        }

        firmware.ReadItemFromFS(path_items);
        sig_data << firmware.getItemsHeader().size() << '\n'; // Need char '\n'

        for (size_t i = 0; i < firmware.getItemsHeader().size(); ++i) {

            auto &hi  = firmware.getItemsHeader().at(i);
            auto &raw = firmware.getItemsRaw().at(i);

            auto item_path   = firmware.PathItemOnFmw(hi.item);
            auto item_sha256 = sha256_sum(raw.data(), raw.size());

            sig_data << item_sha256 << ' ' << item_path << '\n';
        }

        sig_data << firmware.CryptoSign(sig_data.str(), RSA_key_private);

        auto sig_file = FileOpen(path_out_sig, std::ios::out | std::ios::binary);
        sig_file << sig_data.rdbuf();

        std::cout << "[ * ] Path to new signature file: " << path_out_sig << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "[ - ] Error. " << e.what() << std::endl;
    }

    return 0;
}
