#include <iomanip>
#include <sstream>
#include "util.hpp"
#include "util_hw.hpp"

int
main(int argc, char *argv[])
{
    auto usage_print = [&]() {
        usage(
            {
                argv[0],
                "-d /path/items",
                "[-u -f firmware.bin]",
                "[-p -o firmware.bin]",
                "[-v]",
            },
            {
                "-d Path (from|to) unpacked files",
                "-u Unpack (With -f)",
                "-p Pack (With -o)",
                "-f Path from firmware.bin",
                "-o Path to save firmware.bin",
                "-v Verbose",
            });
    };

    std::string path_fmw, path_items, path_metadata, path_sig_item;

    bool funpack = false, fpack = false, fverbose = false;
    bool fin = false, fout = false;

    for (int opt; (opt = getopt(argc, argv, "d:uf:po:v")) != -1;) {
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

    path_metadata = FilePathOnFS(path_items, "/item_list.txt");
    path_sig_item = FilePathOnFS(path_items, "/sig_item_list.txt");

    try {

        Firmware firmware = {};

        if (funpack) {

            firmware.ReadFlashFromFS(path_fmw);
            firmware.UnpackToFS(path_items, path_metadata, path_sig_item);

            firmware.PrintHeader();
            firmware.PrintItems(fverbose);

            firmware.CheckCRC32();

        } else {

            std::stringstream list_metadata;
            list_metadata << FileRead(path_metadata, std::ios::in);

            firmware.ReadHeaderFromFS(list_metadata);

            for (std::string line; std::getline(list_metadata, line);) {

                if (line.size() <= 2 || line.front() != '+') {
                    continue;
                }

                line.erase(0, 2);

                struct huawei_item hi = {};
                firmware.PresetItemHeader(hi, line);
                firmware.AddItemHeader(hi);
            };

            if (firmware.getItemsHeader().empty()) {
                throw_err("Empty items on header", "Count of items");
            }

            firmware.ReadItemFromFS(path_items);
            firmware.PackToMem();

            firmware.PrintHeader();
            firmware.PrintItems(fverbose);

            // Save
            std::fstream fmw_bin =
                FileOpen(path_fmw, std::ios_base::out | std::ios::binary);
            firmware.WriteFlashTo(fmw_bin);
        }

    } catch (const std::exception &e) {
        std::cerr << "[ - ] Error: " << e.what() << std::endl;
    }

    return 0;
}
