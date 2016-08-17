/* AHCI controller driver.
   Copyright (C) 2016 Shaun Ren.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <lib/klib.h>
#include <console.h>
#include <memory.h>
#include <devices/pci.h>

using std::unique_ptr;

namespace devices
{
namespace ahci
{

constexpr uint32_t CLASSCODE_SATA_AHCI = 0x010601;

struct cmd_head
{
    uint32_t config;
    uint32_t transfered;
    uint64_t command_table_base;
    uint32_t _unused[4];
};

struct prdt_entry
{
    uint64_t data_base;
    uint32_t unused;
    uint32_t size;
};

struct cmd_table
{
    uint8_t cfis[64];
    uint8_t command[16];
    uint8_t reserved[48];
    prdt_entry prdt[1];
};

enum class Type
{
    NONE,
    SATA,
    SATAPI,
    SEMB,
    PORT_MULTIPLIER
};

static const char* type_names[] = {
    "None",
    "SATA",
    "SATAPI",
    "Ennlosure management bridge",
    "Port multiplier"};

struct hba_port
{
    uint64_t command_list_base;
    uint64_t fis_base;
    uint32_t int_status;
    uint32_t int_enable;
    uint32_t command;
    uint32_t _unused;
    uint32_t task_file_data;
    uint32_t sig;
    uint32_t status;
    uint32_t sata_ctl;
    uint32_t sata_err;
    uint32_t sata_active;
    uint32_t command_issue;
    uint32_t sata_notify;
    uint32_t fbs;
    uint32_t _unused2[11];
    uint32_t vendor[4];

    static constexpr uint8_t DET_PRESENT = 3;
    static constexpr uint8_t IPM_ACTIVE  = 1;

    Type type() const
    {
        if ((status & 0xf) != DET_PRESENT || ((status >> 8) & 0xf) != IPM_ACTIVE)
            return Type::NONE;

        switch (sig) {
        case 0xEB140101:
            return Type::SATAPI;

        case 0xC33C0101:
            return Type::SEMB;

        case 0x96690101:
            return Type::PORT_MULTIPLIER;

        default:
            return Type::SATA;
        }
    }
};

struct hba_t
{
    uint32_t cap;
    uint32_t global_control;
    uint32_t intr_status;
    uint32_t ports_implemented;
    uint32_t version;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap_extended;
    uint32_t bios_handoff;
    uint32_t _unused[29];
    uint8_t vendor_regs[96];
    hba_port ports[32];
};

unique_ptr<pci::Driver> maker(pci::Device& dev)
{
    if (dev.classcode() != CLASSCODE_SATA_AHCI)
        return {};
    uint32_t bar = dev.readd(pci::REG_BAR5);
    if ((bar & pci::BAR_TYPE_MASK) != pci::BAR_TYPE_32BIT)
        return {};
    bar = bar & pci::BAR_ADDR_MASK;

    console::printf("Probing AHCI device at %02X:%02X:%02X\n",
                    dev.busid(), dev.slotid(), dev.fun());

    auto cmd = dev.command();
    cmd.memwrite  = true;
    cmd.busmaster = true;
    dev.writed(pci::REG_COMMAND, cmd.value);

    hba_t* hba = static_cast<hba_t*>(memory::remap((void*)bar, sizeof(hba_t)));
    console::printf("hba = %X\n", hba);

    // Search ports
    uint32_t pi = hba->ports_implemented;
    for (int i=0; i<32; i++, pi >>= 1) {
        if (pi & 1) {
            console::printf("  Port %d: %s\n", i,
                            type_names[size_t(hba->ports[i].type())]);
        }
    }

    return {};
}

void init()
{
    pci::register_driver(maker);
}

}
}
