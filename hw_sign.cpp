#include <iomanip>
#include <sstream>

#include "util.h"
#include "util_hw.h"
#include "util_rsa.h"

int main(int argc, char *argv[])
{
  auto usage_print = [&]() {
    std::cerr << "Usage: " << argv[0] << " -d /path/to/items -k private_key.pem"
              << " -o items/var/signature" << std::endl;

    std::cerr << " -d Path unpacked files" << std::endl
              << " -k Path from private_key.pem (Without password)" << std::endl
              << " -o Path to save signature file" << std::endl;

    std::exit(EXIT_FAILURE);
  };

  std::string path_items, path_sig_item_list;
  std::string path_key_priv, path_out_sig;

  int opt;
  while ((opt = getopt(argc, argv, "d:k:o:")) != -1) {
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

  path_sig_item_list = HW_ItemPathOnFS(path_items, "/sig_item_list.txt");

  try {

    struct HW_Firmware fmw = {};

    std::stringstream sig_data;
    std::fstream sig_item_list;

    std::string RSA_key_private =
        file_read(file_open(path_key_priv, std::ios::in | std::ios::binary));

    sig_item_list = file_open(path_sig_item_list, std::ios::in);
    std::string line;

    while (std::getline(sig_item_list, line)) {

      struct huawei_item hi = {};

      // '+' sign '-' no sign
      if (line.front() != '+') {
        std::cout << "[ - ] Verify Item Skip: " << line.substr(2) << std::endl;
        continue;
      }

      line.erase(0, 2);

      std::cout << "[ + ] Verify Item Add: " << line << std::endl;

      std::snprintf(hi.item, sizeof(hi.item), "%s", line.c_str());
      fmw.items_hdr.push_back(hi);
    }

    if (!fmw.items_hdr.size()) {
      throw_err("items_hdr.size() == 0", "Count of items");
    }

    HW_ItemsReadFromFS(fmw, path_items);

    sig_data << fmw.items_hdr.size() << '\n';
    for (size_t i = 0; i < fmw.items_hdr.size(); ++i) {

      auto &hi  = fmw.items_hdr.at(i);
      auto &raw = fmw.items_raw.at(i);

      auto item_path   = HW_ItemParse(hi.item);
      auto item_sha256 = sha256_sum(raw.data(), raw.size());

      sig_data << item_sha256 << ' ' << item_path << '\n';
    }

    sig_data << HW_FirmwareSign(sig_data.str(), RSA_key_private);

    file_open(path_out_sig, std::ios::out | std::ios::binary) << sig_data.str();
    std::cout << "[ * ] Path to new signature file: " << path_out_sig << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "[ - ] Error. " << e.what() << std::endl;
  }

  return 0;
}
