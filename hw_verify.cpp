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
                "-k public_key.pem",
                "-i items/var/signature",
            },
            {
                "-d Path to unpacked files",
                "-k Path from pubsigkey.pem",
                "-i Path from signature file",
            });
    };

    std::string path_items, path_key_pub, path_in_sig;

    for (int opt; (opt = getopt(argc, argv, "d:k:i:")) != -1;) {
        switch (opt) {
            case 'd':
                path_items = optarg;
                break;
            case 'k':
                path_key_pub = optarg;
                break;
            case 'i':
                path_in_sig = optarg;
                break;
        }
    }

    if (path_items.empty() || path_key_pub.empty() || path_in_sig.empty()) {
        usage_print();
    }

    try {

        Firmware firmware = {};
        uint32_t item_counts;
        std::string RSA_key_public;
        std::stringstream sig_file_buf;

        RSA_key_public = FileRead(path_key_pub, std::ios::in | std::ios::binary);
        sig_file_buf << FileRead(path_in_sig, std::ios::in | std::ios::binary);

        if (sig_file_buf >> item_counts; !item_counts) {
            throw_err("items_hdr.size() == 0", "Count of items");
        }

        for (size_t i = 0; i < item_counts; ++i) {

            std::string sha256_str, item_path_str;

            if (!(sig_file_buf >> sha256_str >> item_path_str)) {
                throw_err("!(buf >> sha256 >> item_path)", "Format file signature");
            }

            auto item_path = FilePathOnFS(path_items, item_path_str);

            auto item_raw    = FileRead(item_path, std::ios::in | std::ios::binary);
            auto item_sha256 = sha256_sum(item_raw.data(), item_raw.size());

            std::cout << (!sha256_str.compare(item_sha256) ? "[ + ] " : "[ - ] ")
                      << "Verify sha256: " << item_path << std::endl;
        }

        std::cout << (firmware.CryptoVerify(sig_file_buf.str(), RSA_key_public)
                          ? "[ + ] "
                          : "[ - ] ")
                  << "Verify Signature with public key:" << path_key_pub << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "[ - ] Error. " << e.what() << std::endl;
    }

    return 0;
}
