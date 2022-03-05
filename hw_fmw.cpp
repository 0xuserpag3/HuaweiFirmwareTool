#include <iomanip>
#include <sstream>

#include "util.h"
#include "util_hw.h"

int main(int argc, char *argv[])
{
  auto usage_print = [&]() {
    std::cerr << "Usage: " << argv[0]
              << " -d /path/items [-u -f firmware.bin] [-p -o firmware.bin]"
              << " [-v]" << std::endl;

    std::cerr << " -d Path (from|to) unpacked files" << std::endl
              << " -u Unpack (With -f)" << std::endl
              << " -p Pack (With -o)" << std::endl
              << " -f Path from firmware.bin" << std::endl
              << " -o Path to save firmware.bin" << std::endl
              << " -v Verbose" << std::endl;

    std::exit(EXIT_FAILURE);
  };

  std::string path_item_list, path_items, path_fmw;
  std::string path_sig_item_list;

  bool funpack = false, fpack = false, fverbose = false;
  bool fin = false, fout = false;

  int opt;
  while ((opt = getopt(argc, argv, "d:uf:po:v")) != -1) {
    switch (opt) {
      case 'd':
        path_items = optarg;
        break;
      case 'v':
        fverbose = true;
        break;
      case 'o':
        path_fmw = optarg;
        fout     = true;
        break;
      case 'p':
        fpack = true;
        break;
      case 'f':
        path_fmw = optarg;
        fin      = true;
        break;
      case 'u':
        funpack = true;
        break;
    }
  }

  if ((fpack & funpack) | (!fpack & !funpack) | (funpack & fout) | (fpack & fin)) {
    usage_print();
  }

  if (path_fmw.empty() || path_items.empty()) {
    usage_print();
  }

  path_item_list     = HW_ItemPathOnFS(path_items, "/item_list.txt");
  path_sig_item_list = HW_ItemPathOnFS(path_items, "/sig_item_list.txt");

  try {

    struct HW_Firmware fmw = {};

    if (funpack) {

      HW_FirmwareRead(fmw, path_fmw);
      HW_FirmwareUnpack(fmw, path_items, path_item_list, path_sig_item_list);

      HW_PrintHeader(fmw);
      HW_CRC32_ItemsCheck(fmw);

      HW_PrintItems(fmw, fverbose);

      return 0;
    }

    std::fstream item_list(file_open(path_item_list, std::ios::in));
    std::string line;

    item_list.read(reinterpret_cast<char *>(&fmw.hdr.magic_huawei), sizeof(uint32_t));
    item_list >> std::setw(sizeof(uint16_t)) >> fmw.hdr.product_list_sz;
    item_list >> std::setw(fmw.hdr.product_list_sz) >> fmw.product_list;

    if (fmw.product_list.size() != fmw.hdr.product_list_sz) {
      if (fmw.hdr.product_list_sz > 256) {
        fmw.hdr.product_list_sz = 256;
      }
      fmw.product_list.resize(fmw.hdr.product_list_sz);
    }

    while (std::getline(item_list, line)) {

      // '+' add '-' not add
      if (line.front() != '+') {
        continue;
      }

      line.erase(0, 2);

      struct huawei_item hi = {};
      HW_ItemPreset(hi, line);

      fmw.items_hdr.push_back(hi);
    };

    if (!fmw.items_hdr.size()) {
      throw_err("items_hdr.size() == 0", "Count of items");
    }

    HW_ItemsReadFromFS(fmw, path_items);
    HW_FirmwarePack(fmw);

    HW_PrintHeader(fmw);
    HW_PrintItems(fmw, fverbose);

    std::fstream flash(file_open(path_fmw, std::ios_base::out | std::ios::binary));
    HW_PrintFirmware(fmw, flash);

  } catch (const std::exception &e) {
    std::cerr << "[ - ] Error: " << e.what() << std::endl;
  }

  return 0;
}
