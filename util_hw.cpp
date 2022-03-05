#include <filesystem>
#include <iomanip>
#include <cstring>
#include <zlib.h>

#include "util.h"
#include "util_hw.h"
#include "util_rsa.h"

void HW_ItemPreset(struct huawei_item &hi, const std::string &line)
{
  std::istringstream i_fmt(line);
  {
    i_fmt >> hi.iter;
    i_fmt >> std::setw(sizeof(hi.item)) >> hi.item;
    i_fmt >> std::setw(sizeof(hi.section)) >> hi.section;
    i_fmt >> std::setw(sizeof(hi.version)) >> hi.version;
    i_fmt >> hi.policy;
  }

  if (!std::strcmp(hi.version, "NULL")) {
    std::memset(hi.version, '\0', sizeof(hi.version));
  }
}

std::string HW_ItemParse(const std::string &it)
{
  auto pos_path_it = it.find(':');

  if (pos_path_it == std::string::npos) {
    throw_err("item_path.find(':') == std::string::npos", it);
  } else if (pos_path_it + 1 == it.size()) {
    throw_err("item[0] == '\\0'", it);
  }

  return it.substr(pos_path_it + 1);
};

std::string HW_ItemPathOnFS(const std::string &ch_dir, const std::string &it)
{
  auto item_path = std::filesystem::path(it);
  item_path      = std::filesystem::path(ch_dir) /= item_path.relative_path();

  return item_path.make_preferred().string();
};

void HW_ItemsReadFromFS(HW_Firmware &fmw, const std::string &ch_dir)
{
  for (auto &hi : fmw.items_hdr) {

    auto item_path = HW_ItemPathOnFS(ch_dir, HW_ItemParse(hi.item));

    std::string &&raw = file_read(file_open(item_path, std::ios::in | std::ios::binary));
    fmw.items_raw.push_back(std::move(raw));
  }
}

void HW_CRC32_Make(HW_Firmware &fmw)
{
  auto &[hdr, product_list, items_hdr, items_raw] = fmw;

  auto hdr_ptr = reinterpret_cast<uint8_t *>(&hdr);

  uint32_t prod_list_crc32 = 0, prod_list_sz = 0;
  uint32_t items_hdr_crc32 = 0, items_hdr_sz = 0;
  uint32_t items_raw_crc32 = 0, items_raw_sz = 0;

  prod_list_sz = product_list.size();
  prod_list_crc32 =
      crc32(0, reinterpret_cast<uint8_t *>(product_list.data()), prod_list_sz);

  for (size_t i = 0; i < hdr.item_counts; ++i) {
    auto &hi  = items_hdr.at(i);
    auto &raw = items_raw.at(i);

    hi.item_crc32 = crc32(0, reinterpret_cast<uint8_t *>(raw.data()), raw.size());

    items_hdr_sz += sizeof(huawei_item);
    items_hdr_crc32 =
        crc32(items_hdr_crc32, reinterpret_cast<uint8_t *>(&hi), sizeof(huawei_item));

    items_raw_sz += hi.data_sz;
    items_raw_crc32 = crc32_combine(items_raw_crc32, hi.item_crc32, hi.data_sz);
  }

  auto HW_CRC32_hdr = [&](int soff) {
    uint32_t hdr_crc32;
    hdr_crc32 = crc32(0, hdr_ptr + soff, sizeof(huawei_header) - soff);
    hdr_crc32 = crc32_combine(hdr_crc32, prod_list_crc32, prod_list_sz);
    hdr_crc32 = crc32_combine(hdr_crc32, items_hdr_crc32, items_hdr_sz);
    return hdr_crc32;
  };

  hdr.hdr_crc32 = HW_CRC32_hdr(HW_OFF::CRC32_HDR);
  hdr.raw_crc32 =
      crc32_combine(HW_CRC32_hdr(HW_OFF::CRC32_ALL), items_raw_crc32, items_raw_sz);
}

void HW_CRC32_ItemsCheck(HW_Firmware &fmw)
{
  auto &hdr       = fmw.hdr; // link
  auto &items_hdr = fmw.items_hdr;

  struct huawei_header hdr_bak                  = hdr;
  std::vector<struct huawei_item> items_hdr_bak = items_hdr;

  HW_FirmwarePack(fmw);

  // old format ... -36
  if (hdr_bak.raw_crc32 != hdr.raw_crc32) {
    hdr.hdr_sz -= 36;
    HW_CRC32_Make(fmw);
  }

  // preset format CRC32 print
  std::cout << std::showbase << std::hex;

  expr_format_print(
      "Verify CRC32 Full: ", hdr_bak.raw_crc32, hdr_bak.raw_crc32 == hdr.raw_crc32);

  expr_format_print(
      "Verify CRC32 Head: ", hdr_bak.hdr_crc32, hdr_bak.hdr_crc32 == hdr.hdr_crc32);

  for (size_t i = 0; i < items_hdr_bak.size(); ++i) {
    auto &hi_old = items_hdr_bak.at(i);
    auto &hi_new = items_hdr.at(i);

    expr_format_print(
        "Verify CRC32 Item: ", hi_old.item, hi_old.item_crc32 == hi_new.item_crc32);
  }
  std::cout << std::endl;
}

void HW_PrintHeader(const HW_Firmware &fmw)
{
  std::cout << "[ * ] File size:    " << HW_BSWAP32(fmw.hdr.raw_sz) << std::endl
            << "[ * ] File CRC32:   " << std::showbase << std::hex << fmw.hdr.raw_crc32
            << std::endl
            << "[ * ] Head size:    " << std::dec << fmw.hdr.hdr_sz << std::endl
            << "[ * ] Head CRC32:   " << std::showbase << std::hex << fmw.hdr.hdr_crc32
            << std::endl
            << "[ * ] Item counts:  " << std::dec << fmw.hdr.item_counts << std::endl
            << "[ * ] Item size:    " << std::dec << fmw.hdr.item_sz << std::endl
            << "[ * ] Product list: " << std::quoted(fmw.product_list.c_str())
            << std::endl
            << std::endl;
}

void HW_PrintItems(const HW_Firmware &fmw, bool flag_verbose)
{
  enum { ITEM_ITER, ITEM_CRC32, DATA_OFFSET, DATA_SIZE, ITEM, SECTION, VERSION, POLICY };

  struct col_data {
    bool verbose;
    size_t len;
    const char *name;
  };

  struct col_data cols[8];
  cols[ITEM_ITER]   = { true, 0, "Iter" };
  cols[ITEM_CRC32]  = { true, 0, "Item CRC32" };
  cols[DATA_OFFSET] = { true, 0, "Data offset" };
  cols[DATA_SIZE]   = { true, 0, "Data size" };
  cols[ITEM]        = { false, 0, "Item" };
  cols[SECTION]     = { false, 0, "Section" };
  cols[VERSION]     = { false, 0, "Version" };
  cols[POLICY]      = { true, 0, "Policy" };

  std::ostream &os = std::cout;
  os.setf(std::ios::left, std::ios::adjustfield);

  for (auto &hi : fmw.items_hdr) {
    for (size_t j = 0; j < 8; ++j) {

      auto &col = cols[j];

      const char *p = nullptr;

      if (j == ITEM) {
        p = hi.item;
      } else if (j == SECTION) {
        p = hi.section;
      } else if (j == VERSION) {
        p = hi.version;
      } else if (flag_verbose) {
        p = col.name;
      }

      if (p) {

        if (j == ITEM || j == SECTION || j == VERSION) {
          col.len = std::max(col.len, std::strlen(col.name));
        }
        col.len = std::max(col.len, std::strlen(p));
      }
    }
  }

  auto border_print = [&]() constexpr
  {
    size_t b_sz = 1; // First separator '|'

    for (auto &col : cols) {
      if (!flag_verbose && col.verbose) {
        continue;
      }
      b_sz += col.len + 3; // Two spaces ' ' and separator '|'
    }
    os << std::setfill('-') << std::setw(b_sz) << '-';
    os << std::setfill(' ') << std::endl;
  };

  auto column_print = [&](int ix, const auto &val) constexpr
  {
    os << ' ' << std::setw(cols[ix].len) << val << " |";
  };

  border_print();
  os << '|';
  for (size_t i = 0; i < 8; ++i) {
    if (!flag_verbose && cols[i].verbose) {
      continue;
    }
    column_print(i, cols[i].name);
  }
  os << std::endl;
  border_print();

  for (auto &hi : fmw.items_hdr) {
    os << '|';

    if (flag_verbose) {
      column_print(ITEM_ITER, hi.iter);

      os << std::showbase << std::hex;
      column_print(ITEM_CRC32, hi.item_crc32);
      column_print(DATA_OFFSET, hi.data_off);

      os << std::noshowbase << std::dec;
      column_print(DATA_SIZE, hi.data_sz);
    }

    column_print(ITEM, hi.item);
    column_print(SECTION, hi.section);
    column_print(VERSION, *hi.version ? hi.version : "NULL");

    if (flag_verbose) {
      column_print(POLICY, hi.policy);
    }
    os << std::endl;
  }
  border_print();
}

void HW_FirmwareRead(HW_Firmware &fmw, const std::string &path_fmw)
{
  auto &[hdr, product_list, items_hdr, items_raw] = fmw;

  std::fstream fmw_fd(file_open(path_fmw, std::ios::in | std::ios::binary));

  if (!fmw_fd.read(reinterpret_cast<char *>(&hdr), sizeof(huawei_header))) {
    throw_err("Header corrupted", path_fmw);
  }

  product_list.resize(hdr.product_list_sz);
  if (!fmw_fd.read(product_list.data(), product_list.size())) {
    throw_err("Product list corrupted", path_fmw);
  }

  for (uint32_t i = 0; i < hdr.item_counts; ++i) {
    struct huawei_item hi;

    if (!fmw_fd.read(reinterpret_cast<char *>(&hi), sizeof(huawei_item))) {
      throw_err("Items corrupted", path_fmw);
    }

    items_hdr.push_back(hi);
  }

  for (auto &hi : items_hdr) {
    std::string raw(hi.data_sz, '\0');

    if (!fmw_fd.seekg(hi.data_off)) {
      throw_err("file.seekg() error", std::to_string(hi.data_off));
    }

    if (!fmw_fd.read(raw.data(), raw.size())) {
      throw_err("Raw Data corrupted", path_fmw);
    }

    items_raw.push_back(std::move(raw));
  }
}

void HW_FirmwarePack(HW_Firmware &fmw)
{
  auto &[hdr, product_list, items_hdr, items_raw] = fmw;

  hdr._unknow_data_1  = 0x00;
  hdr._unknow_data_1  = 0x00;
  hdr.product_list_sz = product_list.size();

  hdr.item_sz     = sizeof(huawei_item);
  hdr.item_counts = items_hdr.size();

  hdr.raw_sz = hdr.hdr_sz =
      sizeof(huawei_header) + hdr.item_counts * hdr.item_sz + hdr.product_list_sz;

  for (size_t i = 0; i < hdr.item_counts; ++i) {
    auto &hi  = items_hdr.at(i);
    auto &raw = items_raw.at(i);

    hi.data_off = hdr.raw_sz;
    hi.data_sz  = raw.size();

    hdr.raw_sz += hi.data_sz;
  }

  HW_CRC32_Make(fmw);

  hdr.raw_sz = HW_BSWAP32(hdr.raw_sz - HW_OFF::SZ_BIN);
}

void HW_FirmwareUnpack(HW_Firmware &fmw,
                       const std::string &path_items,
                       const std::string &path_item_list,
                       const std::string &path_sig_item_list)
{
  auto &[hdr, product_list, items_hdr, items_raw] = fmw;

  std::fstream sig_item_list, item_list;

  if (!std::filesystem::exists(path_items)) {
    std::filesystem::create_directories(path_items);
  }

  item_list     = file_open(path_item_list, std::ios::out);
  sig_item_list = file_open(path_sig_item_list, std::ios::out);

  item_list.write(reinterpret_cast<char *>(&hdr.magic_huawei), sizeof(uint32_t));
  item_list << std::endl
            << hdr.product_list_sz << ' ' << product_list.c_str() << std::endl;

  for (size_t i = 0; i < hdr.item_counts; ++i) {
    auto &hi  = items_hdr.at(i);
    auto &raw = items_raw.at(i);

    auto item_path = HW_ItemPathOnFS(path_items, HW_ItemParse(hi.item));
    auto item_dir  = std::filesystem::path(item_path).parent_path();

    if (!std::filesystem::exists(item_dir)) {
      std::filesystem::create_directories(item_dir);
    }

    std::fstream item_rawbin(file_open(item_path, std::ios::out | std::ios::binary));
    item_rawbin << raw;

    item_list << "- ";
    item_list << hi.iter << ' ' << hi.item << ' ' << hi.section << ' ';
    item_list << (*hi.version ? hi.version : "NULL") << ' ' << hi.policy;
    item_list << std::endl;

    sig_item_list << "- " << hi.item << std::endl;
  }
};

bool HW_FirmwareVerify(const std::string &sig_file, const std::string &key_pub)
{
  if (sig_file.size() <= 256) {
    throw_err("file_signature.size() <= 256", "HW signature size");
  }

  auto raw_data = reinterpret_cast<const uint8_t *>(&sig_file[0]);
  auto sig_data = reinterpret_cast<const uint8_t *>(&sig_file[sig_file.size() - 256]);

  return RSA_verify_data(key_pub, raw_data, sig_file.size() - 256, sig_data, 256);
}

std::string HW_FirmwareSign(const std::string &raw_data, const std::string &key_priv)
{
  std::string sig_buf;

  if (!RSA_sign_data(raw_data, key_priv, sig_buf)) {
    throw_err("!RSA_sign_data()", "Sign error");
  }

  return sig_buf;
}

void HW_PrintFirmware(const HW_Firmware &fmw, std::ostream &os)
{
  if (!os.write(reinterpret_cast<const char *>(&fmw.hdr), sizeof(huawei_header))) {
    throw_err("!os.write", "Header");
  }

  if (!os.write(fmw.product_list.data(), fmw.product_list.size())) {
    throw_err("!os.write", "Product list");
  }

  if (!os.write(reinterpret_cast<const char *>(fmw.items_hdr.data()),
                fmw.items_hdr.size() * sizeof(huawei_item))) {
    throw_err("!os.write", "Header Item");
  }

  for (auto &raw : fmw.items_raw) {
    if (!os.write(raw.data(), raw.size())) {
      throw_err("!os.write", "Raw Item");
    }
  }
}
