#ifndef HW_CTL_H
#define HW_CTL_H

#include <iostream>
#include <vector>
#include <zlib.h>

#include "huawei_header.h"

struct HW_Firmware {
  struct huawei_header hdr;
  std::string product_list;
  std::vector<struct huawei_item> items_hdr;
  std::vector<std::string> items_raw;
};

void HW_ItemPreset(struct huawei_item &hi, const std::string &line);
std::string HW_ItemParse(const std::string &it);
std::string HW_ItemPathOnFS(const std::string &ch_dir, const std::string &it);
void HW_ItemsReadFromFS(HW_Firmware &fmw, const std::string &ch_dir);

void HW_CRC32_Make(HW_Firmware &fmw);
void HW_CRC32_ItemsCheck(HW_Firmware &fmw);

void HW_PrintHeader(const HW_Firmware &fmw);
void HW_PrintItems(const HW_Firmware &fmw, bool flag_verbose);

void HW_FirmwareRead(HW_Firmware &fmw, const std::string &path_fmw);
void HW_FirmwarePack(HW_Firmware &fmw);
void HW_FirmwareUnpack(HW_Firmware &fmw,
                       const std::string &path_items,
                       const std::string &path_item_list,
                       const std::string &path_sig_item_list);

bool HW_FirmwareVerify(const std::string &sig_file, const std::string &key_pub);
std::string HW_FirmwareSign(const std::string &raw_data, const std::string &key_priv);

void HW_PrintFirmware(const HW_Firmware &fmw, std::ostream &os);

constexpr inline uint32_t HW_BSWAP32(uint32_t x)
{
  return (((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) |
          ((x & 0x0000FF00u) << 8) | ((x & 0x000000FFu) << 24));
}

#endif // HW_CTL_H
