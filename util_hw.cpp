#include <cstring>
#include <iomanip>
#include <filesystem>
#include <zlib.h>
#include "util.hpp"
#include "util_hw.hpp"
#include "util_rsa.hpp"

void
Firmware::PrintItems(bool flag_verbose)
{
    enum {
        ITEM_ITER,
        ITEM_CRC32,
        DATA_OFFSET,
        DATA_SIZE,
        ITEM,
        SECTION,
        VERSION,
        POLICY
    };

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

    for (auto &hi : this->items_hdr) {
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

    for (auto &hi : this->items_hdr) {
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

void
Firmware::CheckCRC32()
{
    // Backup old CRC32 values
    decltype(Firmware::hdr) header_old          = this->hdr;
    decltype(Firmware::items_hdr) items_hdr_old = this->items_hdr;

    this->PackToMem();
    // Set link to new CRC32 values after packing
    decltype(Firmware::hdr) &header_new          = this->hdr;
    decltype(Firmware::items_hdr) &items_hdr_new = this->items_hdr;

    // old format ... -36
    if (header_old.raw_crc32 != header_new.raw_crc32) {
        std::cout << "[ * ] Repack CRC32 ..." << std::endl;
        header_new.hdr_sz -= 36;
        // Repack CRC32 after change hdr_sz
        this->CalculateCRC32();
    }

    // preset format CRC32 print
    std::cout << std::showbase << std::hex;

    std::cout << (header_old.raw_crc32 == header_new.raw_crc32 ? "[ + ] " : "[ - ] ")
              << "Verify CRC32 Full: " << header_old.raw_crc32 << std::endl;

    std::cout << (header_old.hdr_crc32 == header_new.hdr_crc32 ? "[ + ] " : "[ - ] ")
              << "Verify CRC32 Head: " << header_old.hdr_crc32 << std::endl;

    for (size_t i = 0; i < items_hdr_old.size(); ++i) {
        auto &hi_old = items_hdr_old.at(i);
        auto &hi_new = items_hdr_new.at(i);

        std::cout << (hi_old.item_crc32 == hi_new.item_crc32 ? "[ + ] " : "[ - ] ")
                  << "Verify CRC32 Item: " << hi_old.item << std::endl;
    }
}

void
Firmware::PrintHeader()
{
    std::cout << "[ * ] File size:    ";
    std::cout << '"' << BSWAP32(this->hdr.raw_sz) << '"' << std::endl;

    std::cout << "[ * ] File CRC32:   ";
    std::cout << '"' << std::showbase << std::hex << this->hdr.raw_crc32 << '"'
              << std::endl;

    std::cout << "[ * ] Head size:    ";
    std::cout << '"' << std::dec << this->hdr.hdr_sz << '"' << std::endl;

    std::cout << "[ * ] Head CRC32:   ";
    std::cout << '"' << std::showbase << std::hex << this->hdr.hdr_crc32 << '"'
              << std::endl;

    std::cout << "[ * ] Item counts:  ";
    std::cout << '"' << std::dec << this->hdr.item_counts << '"' << std::endl;

    std::cout << "[ * ] Item size:    ";
    std::cout << '"' << std::dec << this->hdr.item_sz << '"' << std::endl;

    std::cout << "[ * ] Product list: ";
    std::cout << '"' << this->prod_list.c_str() << '"' << std::endl;
}

std::string
Firmware::PathItemOnFmw(const std::string &raw_path_item)
{
    auto pos_path_it = raw_path_item.find(':');

    if (pos_path_it == std::string::npos) {
        throw_err("Cannot find ':'", raw_path_item);
    } else if (pos_path_it + 1 == raw_path_item.size()) {
        throw_err("Path on FS is NULL", raw_path_item);
    }

    return raw_path_item.substr(pos_path_it + 1);
}

void
Firmware::PackToMem()
{
    this->hdr._unknow_data_1 = 0x00;
    this->hdr._unknow_data_2 = 0x00;
    this->hdr.prod_list_sz   = this->prod_list.size();

    this->hdr.item_sz     = sizeof(huawei_item);
    this->hdr.item_counts = this->items_hdr.size();

    this->hdr.raw_sz = this->hdr.hdr_sz = sizeof(huawei_header) +
                                          this->hdr.item_counts * this->hdr.item_sz +
                                          this->hdr.prod_list_sz;

    for (size_t i = 0; i < this->hdr.item_counts; ++i) {
        auto &hi  = this->items_hdr.at(i);
        auto &raw = this->items_raw.at(i);

        hi.data_off = this->hdr.raw_sz;
        hi.data_sz  = raw.size();

        this->hdr.raw_sz += hi.data_sz;
    }

    this->CalculateCRC32();

    this->hdr.raw_sz = BSWAP32(this->hdr.raw_sz - HW_OFF::SZ_BIN);
}

void
Firmware::UnpackToFS(const std::string &path_items,
                     const std::string &path_metadata,
                     const std::string &path_sig_item)
{
    std::fstream list_sig_item, list_metadata;

    if (!std::filesystem::exists(path_items)) {
        std::filesystem::create_directories(path_items);
    }

    list_metadata = FileOpen(path_metadata, std::ios::out);
    list_sig_item = FileOpen(path_sig_item, std::ios::out);

    //    list_metadata.write(reinterpret_cast<char *>(&this->hdr.magic_huawei),
    //                        sizeof(this->hdr.magic_huawei));
    //    list_metadata << std::endl;
    list_metadata << std::showbase << std::hex << this->hdr.magic_huawei << std::endl;

    list_metadata << std::dec << this->hdr.prod_list_sz << ' ' << this->prod_list.c_str()
                  << std::endl;

    for (size_t i = 0; i < this->hdr.item_counts; ++i) {
        auto &hi  = this->items_hdr.at(i);
        auto &raw = this->items_raw.at(i);

        auto item_path = FilePathOnFS(path_items, this->PathItemOnFmw(hi.item));
        auto item_dir  = std::filesystem::path(item_path).parent_path();

        if (!std::filesystem::exists(item_dir)) {
            std::filesystem::create_directories(item_dir);
        }

        std::fstream item_bin(FileOpen(item_path, std::ios::out | std::ios::binary));
        item_bin << raw;

        list_metadata << "- ";
        list_metadata << hi.iter << ' ' << hi.item << ' ' << hi.section << ' ';
        list_metadata << (*hi.version ? hi.version : "NULL") << ' ' << hi.policy;
        list_metadata << std::endl;

        list_sig_item << "- " << hi.item << std::endl;
    }
}

void
Firmware::WriteFlashTo(std::ostream &os)
{
    if (!os.write(reinterpret_cast<const char *>(&this->hdr), sizeof(huawei_header))) {
        throw_err("!os.write", "Header");
    }

    if (!os.write(this->prod_list.data(), this->prod_list.size())) {
        throw_err("!os.write", "Product list");
    }

    if (!os.write(reinterpret_cast<const char *>(this->items_hdr.data()),
                  this->items_hdr.size() * sizeof(huawei_item))) {
        throw_err("!os.write", "Header Item");
    }

    for (auto &raw : this->items_raw) {
        if (!os.write(raw.data(), raw.size())) {
            throw_err("!os.write", "Raw Item");
        }
    }
}

void
Firmware::ReadFlashFromFS(const std::string &path_fmw)
{
    std::fstream fd = FileOpen(path_fmw, std::ios::in | std::ios::binary);

    if (!fd.read(reinterpret_cast<char *>(&this->hdr), sizeof(huawei_header))) {
        throw_err("Header corrupted", path_fmw);
    }

    this->prod_list.resize(this->hdr.prod_list_sz);

    if (!fd.read(this->prod_list.data(), this->prod_list.size())) {
        throw_err("Product list corrupted", path_fmw);
    }

    for (uint32_t i = 0; i < this->hdr.item_counts; ++i) {
        struct huawei_item hi;

        if (!fd.read(reinterpret_cast<char *>(&hi), sizeof(huawei_item))) {
            throw_err("Items corrupted", path_fmw);
        }

        this->AddItemHeader(hi);
    }

    for (auto &hi : this->items_hdr) {
        std::string raw(hi.data_sz, '\0');

        if (!fd.seekg(hi.data_off)) {
            throw_err("Offset to data corrupted", std::to_string(hi.data_off));
        }

        if (!fd.read(raw.data(), raw.size())) {
            throw_err("Raw Data corrupted", path_fmw);
        }

        this->AddItemRaw(raw);
    }
}

void
Firmware::CalculateCRC32()
{
    uint32_t prod_list_crc32 = 0, prod_list_sz = 0;
    uint32_t items_hdr_crc32 = 0, items_hdr_sz = 0;
    uint32_t items_raw_crc32 = 0, items_raw_sz = 0;

    prod_list_sz = this->prod_list.size();
    prod_list_crc32 =
        crc32(0, reinterpret_cast<uint8_t *>(this->prod_list.data()), prod_list_sz);

    for (size_t i = 0; i < this->hdr.item_counts; ++i) {
        auto &hi  = this->items_hdr.at(i);
        auto &raw = this->items_raw.at(i);

        hi.item_crc32 = crc32(0, reinterpret_cast<uint8_t *>(raw.data()), raw.size());

        items_hdr_sz += sizeof(huawei_item);
        items_hdr_crc32 =
            crc32(items_hdr_crc32, reinterpret_cast<uint8_t *>(&hi), sizeof(huawei_item));

        items_raw_sz += hi.data_sz;
        items_raw_crc32 = crc32_combine(items_raw_crc32, hi.item_crc32, hi.data_sz);
    }

    uint32_t hdr_crc32, raw_crc32;

    // Calculate header crc32
    hdr_crc32 = crc32(0,
                      reinterpret_cast<uint8_t *>(&hdr) + HW_OFF::CRC32_HDR,
                      sizeof(huawei_header) - HW_OFF::CRC32_HDR);
    hdr_crc32 = crc32_combine(hdr_crc32, prod_list_crc32, prod_list_sz);
    hdr_crc32 = crc32_combine(hdr_crc32, items_hdr_crc32, items_hdr_sz);

    // Set
    this->hdr.hdr_crc32 = hdr_crc32;

    // Calculate full crc32 after get hdr.hdr_crc32
    raw_crc32 = crc32(0,
                      reinterpret_cast<uint8_t *>(&hdr) + HW_OFF::CRC32_ALL,
                      sizeof(huawei_header) - HW_OFF::CRC32_ALL);
    raw_crc32 = crc32_combine(raw_crc32, prod_list_crc32, prod_list_sz);
    raw_crc32 = crc32_combine(raw_crc32, items_hdr_crc32, items_hdr_sz);
    raw_crc32 = crc32_combine(raw_crc32, items_raw_crc32, items_raw_sz);

    // Set
    this->hdr.raw_crc32 = raw_crc32;
}

void
Firmware::ReadHeaderFromFS(std::stringstream &fd)
{
    fd.read(reinterpret_cast<char *>(&this->hdr.magic_huawei),
            sizeof(this->hdr.magic_huawei)); // old format without 0x

    if (BSWAP16(static_cast<uint16_t>(this->hdr.magic_huawei)) ==
        0x3078 /* 0x3078 == "0x" */) {

        fd.seekg(0);
        fd >> std::hex >> this->hdr.magic_huawei; // read new format
    }

    fd >> std::dec >> this->hdr.prod_list_sz;
    fd >> std::setw(this->hdr.prod_list_sz) >> this->prod_list;

    // Check max value product_list_size
    if (this->prod_list.size() != this->hdr.prod_list_sz) {

        if (this->hdr.prod_list_sz > 256) {
            this->hdr.prod_list_sz = 256;
        }

        this->prod_list.resize(this->hdr.prod_list_sz);
    }
}

bool
Firmware::CryptoVerify(const std::string &sig_file, const std::string &key_pub)
{
    constexpr uint32_t sig_sz = 256; // hw sig size
    if (sig_file.size() <= sig_sz) {
        throw_err("file_signature.size() <= 256", "HW signature size");
    }

    auto raw_data = reinterpret_cast<const uint8_t *>(&sig_file[0]);
    auto sig_data =
        reinterpret_cast<const uint8_t *>(&sig_file[sig_file.size() - sig_sz]);

    return RSA_verify_data(key_pub, raw_data, sig_file.size() - sig_sz, sig_data, sig_sz);
}

std::string
Firmware::CryptoSign(const std::string &raw_data, const std::string &key_priv)
{
    std::string sig_buf;

    if (!RSA_sign_data(raw_data, key_priv, sig_buf)) {
        throw_err("!RSA_sign_data()", "Sign error");
    }

    return sig_buf;
}

void
Firmware::ReadItemFromFS(const std::string &path_items)
{
    for (auto &hi : this->items_hdr) {

        auto item_path = FilePathOnFS(path_items, this->PathItemOnFmw(hi.item));

        std::string &&raw = FileRead(item_path, std::ios::in | std::ios::binary);

        this->items_raw.push_back(std::move(raw));
    }
}

void
Firmware::AddItemHeader(struct huawei_item &hi)
{
    this->items_hdr.push_back(hi);
}
void
Firmware::AddItemRaw(std::string &raw)
{
    this->items_raw.push_back(std::move(raw));
}

void
Firmware::PresetItemHeader(struct huawei_item &hi, const std::string &line)
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
