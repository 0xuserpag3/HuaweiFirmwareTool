#include <iomanip>
#include <sstream>

#include "util.h"
#include "util_hw.h"
#include "util_rsa.h"

int main(int argc, char *argv[])
{
  auto usage_print = [&]() {
    std::cerr << "Usage: " << argv[0] << " -d /path/to/items -k private_key.pem"
              << " -i items/var/signature" << std::endl;

    std::cerr << " -d Path unpacked files" << std::endl
              << " -k Path from pubsigkey.pem" << std::endl
              << " -i Path from signature file" << std::endl;

    std::exit(EXIT_FAILURE);
  };

  std::string path_items, path_key_pub, path_in_sig;

  int opt;
  while ((opt = getopt(argc, argv, "d:k:i:")) != -1) {
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

    std::string RSA_key_public =
        file_read(file_open(path_key_pub, std::ios::in | std::ios::binary));

    uint32_t item_counts;
    std::stringstream sig_file_buf;
    std::fstream sig_file = file_open(path_in_sig, std::ios::in | std::ios::binary);

    sig_file_buf << sig_file.rdbuf();
    sig_file.close();

    if (sig_file_buf >> item_counts; !item_counts) {
      throw_err("items_hdr.size() == 0", "Count of items");
    }

    for (size_t i = 0; i < item_counts; ++i) {

      std::string sha256_str, item_path_str;

      if (!(sig_file_buf >> sha256_str >> item_path_str)) {
        throw_err("!(buf >> sha256 >> item_path)", "Format file signature");
      }

      auto item_path = HW_ItemPathOnFS(path_items, item_path_str);

      auto item        = file_read(file_open(item_path, std::ios::in | std::ios::binary));
      auto item_sha256 = sha256_sum(item.data(), item.size());

      expr_format_print("Verify sha256: ", item_path, !sha256_str.compare(item_sha256));
    }

    expr_format_print("Verify Signature with public key:",
                      path_key_pub,
                      HW_FirmwareVerify(sig_file_buf.str(), RSA_key_public));

  } catch (const std::exception &e) {
    std::cerr << "[ - ] Error. " << e.what() << std::endl;
  }

  return 0;
}
