#ifndef HW_CTL_H
#define HW_CTL_H

#include <vector>
#include <iostream>
#include "huawei_header.h"

class Firmware {

  private:
    struct huawei_header hdr;
    std::string prod_list;
    std::vector<struct huawei_item> items_hdr;
    std::vector<std::string> items_raw;

  public:
    auto &
    getItemsHeader()
    {
        return this->items_hdr;
    }

    auto &
    getItemsRaw()
    {
        return this->items_raw;
    }

    void PresetItemHeader(struct huawei_item &hi, const std::string &line);
    void AddItemRaw(std::string &raw);
    void AddItemHeader(struct huawei_item &hi);
    void ReadItemFromFS(const std::string &path_items);
    std::string PathItemOnFmw(const std::string &raw_path_item);

    void PackToMem();
    void UnpackToFS(const std::string &path_items,
                    const std::string &path_metadata,
                    const std::string &path_sig_item);

    void WriteFlashTo(std::ostream &os);
    void ReadFlashFromFS(const std::string &path_fmw);

    void PrintHeader();
    void PrintItems(bool flag_verbose);

    void CheckCRC32();
    void CalculateCRC32();

    void ReadHeaderFromFS(std::stringstream &fd);

    bool CryptoVerify(const std::string &sig_file, const std::string &key_pub);
    std::string CryptoSign(const std::string &raw_data, const std::string &key_priv);
};

#endif // HW_CTL_H
