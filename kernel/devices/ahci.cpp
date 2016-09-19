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
#include <lib/string.h>
#include <console.h>
#include <memory.h>
#include <paging.h>
#include <devices/pci.h>

using std::unique_ptr;
using std::make_unique;
using memory::kmalloc;
using memory::KMALLOC_ALIGN;
using memory::KMALLOC_ZERO;

namespace devices
{
namespace ahci
{

constexpr uint32_t CLASSCODE_SATA_AHCI = 0x010601;

static constexpr size_t NUM_CMD_SLOTS    = 32;
static constexpr size_t NUM_PRDT_ENTRIES = 8;

struct command_head
{
    uint8_t  cfl : 5;
    bool     atapi : 1;
    bool     write : 1;
    bool     prefetch : 1;
    bool     reset : 1;
    bool     bist : 1;
    bool     clear_r_ok : 1;
    bool     _unused : 1;
    uint8_t  pmp : 4; // Port multiplier port
    uint16_t prdt_entries;
    volatile uint32_t transfered;
    uint64_t command_table_base;
    uint32_t _unused2[4];
} __attribute__((packed));

struct prdt_entry
{
    uint64_t data_base;
    uint32_t _unused;
    static constexpr uint32_t MAX_SIZE = (1<<22) - 1;
    uint32_t size : 22; // Max 4MiB
    uint16_t _unused2 : 9;
    bool     int_on_complete : 1;
};

struct command_table
{
    uint8_t    cfis[64];
    uint8_t    command[16];
    uint8_t    _reserved[48];
    prdt_entry prdt[NUM_PRDT_ENTRIES];
};

static_assert(sizeof(command_table) % 4 == 0,
              "sizeof(command_table) must be divisible by 4");


enum
{
    FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
    FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
    FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
    FIS_TYPE_DATA	= 0x46,	// Data FIS - bidirectional
    FIS_TYPE_BIST	= 0x58,	// BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
};

enum
{
    ATA_CMD_READ_DMA     = 0xC8,
    ATA_CMD_READ_DMA_EX  = 0x25,
    ATA_CMD_WRITE_DMA    = 0xCA,
    ATA_CMD_WRITE_DMA_EX = 0x35,
};

enum
{
    ATA_DEV_BUSY = 0x80,
    ATA_DEV_BRQ  = 0x08,
};

// Register FIS - host to device
struct fis_reg_h2d
{
    uint8_t fis_type;

    uint8_t pmport : 4;	// Port multiplier
    uint8_t _unused : 3;
    bool cmd : 1;        // true = command, false = control

    uint8_t command;	// Command register
    uint8_t featurel;	// Feature register, low byte

    uint32_t lbal : 24; // LBA, low 24 bits
    uint8_t device;

    uint32_t lbah : 24;  // LBA, high 24 bits
    uint8_t featureh;   // Feature register, high byte

    uint16_t count;
    uint8_t isoch_cmd_completion; // Isochronous command completion
    uint8_t control;

    uint32_t _unused2;
} __attribute__((packed));

enum class type_t
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

    type_t type() const volatile
    {
        if ((status & 0xf) != DET_PRESENT || ((status >> 8) & 0xf) != IPM_ACTIVE)
            return type_t::NONE;

        switch (sig) {
        case 0xEB140101:
            return type_t::SATAPI;

        case 0xC33C0101:
            return type_t::SEMB;

        case 0x96690101:
            return type_t::PORT_MULTIPLIER;

        default:
            return type_t::SATA;
        }
    }

    enum Command
    {
        ST = 1,
        SPIN_UP = 1<<1,
        POWER_DOWN = 1<<2,
        FRE = 1<<4,
        FR = 1<<14,
        CR = 1<<15,
    };

    void start_engine() volatile
    {
        while (command & CR) ;
        sw_barrier();
        command |= FRE | ST;
    }

    void stop_engine() volatile
    {
        command &= ~(ST | FRE);
        sw_barrier();
        while (command & (FR | CR)) ;
    }

    int find_slot() volatile
    {
        const uint32_t slots = sata_active | command_issue;
        //console::printf("slots = %08X\n", slots);
        for (size_t i=0; i<NUM_CMD_SLOTS; i++)
            if (!(slots & (1<<i)))
                return i;
        return -1;
    }
};

struct hba_t
{
    uint32_t cap;
    uint32_t global_control;
    volatile uint32_t intr_status;
    uint32_t ports_implemented;
    uint32_t version;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap_extended;
    uint32_t bios_handoff;
    uint32_t _unused[29];
    uint8_t  vendor_regs[96];
    volatile hba_port ports[32];
};

class ahci_driver : public block_driver
{
    uint8_t portid;
    type_t type;
    bool valid;
    volatile hba_port* port = nullptr;

    command_head* cmd_head = nullptr;
    void* cmd_head_phys = nullptr;
    command_table* cmd_table = nullptr;
    void* cmd_table_phys = nullptr;
    uint8_t* fis = nullptr;
    void* fis_phys = nullptr;

public:
    ahci_driver(uint8_t portid, volatile hba_port* port)
        : portid(portid), port(port)
    {
        type = port->type();
        valid = type == type_t::SATA /*|| type == type_t::SATAPI*/;
        if (!valid)
            return;

        // TODO allocate DMA memory directly from frame allocator to
        //      ensure physical continuity
        kmalloc(sizeof(command_table) * NUM_CMD_SLOTS,
                KMALLOC_ALIGN | KMALLOC_ZERO,
                &cmd_table_phys);
        kmalloc(sizeof(command_head) * 32,
                KMALLOC_ALIGN | KMALLOC_ZERO,
                &cmd_head_phys);
        kmalloc(256, KMALLOC_ALIGN | KMALLOC_ZERO, &fis_phys);
        cmd_head = (command_head*)memory::remap(cmd_head_phys, sizeof(command_head) * 32);
        cmd_table = (command_table*)memory::remap(cmd_table_phys, sizeof(command_table) * NUM_CMD_SLOTS);
        fis = (uint8_t*)memory::remap(fis_phys, 256);

        port->stop_engine();

        port->command_list_base = (uint64_t)cmd_head_phys;
        port->fis_base = (uint64_t)fis_phys;

        for (size_t i=0; i<NUM_CMD_SLOTS; i++) {
            cmd_head[i].prdt_entries = NUM_PRDT_ENTRIES;
            cmd_head[i].command_table_base = size_t(cmd_table_phys) +
                sizeof(command_table) * i;
        }

        port->start_engine();

        port->int_enable = ~0;

        // TEST read first 4 sectors (2KiB)
        void* buf_phys = nullptr;
        auto buf = (uint8_t*) kmalloc(2048, KMALLOC_ZERO, &buf_phys);
        ASSERTH(!transfer(0, 4, (uint8_t*)buf_phys, false));

        for (int i=0; i<256;) {
            for (int j=0;j<26 && i<256;j++,i++)
                console::printf(j==25 ? "%02X" : "%02X ", buf[i]);
            console::put('\n');
        }
        delete[] buf;
        for (volatile int i=1<<28; i--; ) ; // Pause a bit
    }

    ~ahci_driver()
    {
        port->stop_engine();
        //delete[] cmd_head;
        //delete[] cmd_table;
        //delete[] fis;
    }

    explicit operator bool() const
    {
        return valid;
    }

    const char* name() const override
    {
        return "AHCI Driver";
    }

    int open() override
    {
        return valid ? 0 : -ENOSYS;
    }

    int transfer(uint64_t blk, size_t nblks, uint8_t* buf, bool write) override
    {
        port->int_status = ~0;
        int slot = port->find_slot();
        if (slot < 0)
            return -EBUSY;

        auto head = cmd_head + slot;
        head->cfl        = sizeof(fis_reg_h2d) / sizeof(uint32_t);
        head->write      = write;
        head->prefetch   = true;
        head->clear_r_ok = true;
        auto table = cmd_table + slot;

        constexpr size_t SECTOR_SIZE = 512;
        constexpr size_t MAX_SECTORS = prdt_entry::MAX_SIZE / SECTOR_SIZE;

        while (nblks > 0) {
            size_t nprdt = 0;
            size_t total_sects = 0;
            memsetd(table, 0, sizeof(command_table) >> 2);
            while (nblks > 0 && nprdt < NUM_PRDT_ENTRIES) {
                size_t nsects = std::min(nblks, MAX_SECTORS);
                size_t sz = nsects * SECTOR_SIZE;
                table->prdt[nprdt].data_base       = (uint64_t)buf;
                table->prdt[nprdt].size            = sz;
                table->prdt[nprdt].int_on_complete = true;
                buf += sz;
                nblks -= nsects;
                total_sects += nsects;
                nprdt++;
            }
            head->prdt_entries = nprdt;

            auto cfis = (fis_reg_h2d*)table->cfis;
            cfis->fis_type = FIS_TYPE_REG_H2D;
            cfis->cmd = true;
            cfis->command = write ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX;
            cfis->lbal = blk & ((1<<24)-1);
            cfis->lbah = (blk >> 24) & ((1<<24)-1);
            cfis->device = 0xE0;
            cfis->count = total_sects;
            sw_barrier();

            size_t j = 0;
            constexpr size_t MAX_CYCLES = 1<<20;
            for (; (port->task_file_data & (ATA_DEV_BRQ | ATA_DEV_BUSY))
                     && j < MAX_CYCLES; j++) ;
            if (j >= MAX_CYCLES) { // Hung
                return -EIO;
            }
            port->sata_active = 1<<slot;
            port->command_issue = 1<<slot; // Issue command
            console::printf("AHCI transfer: lbal = %X, lbah = %X, device = %X, count = %X, slot = %d\n",
                            cfis->lbal, cfis->lbah, cfis->device, cfis->count, slot);
            sw_barrier();
            for (;;) {
                if (!(port->command_issue & ((1<<slot) | (1<<5))))
                    break;
                if (port->int_status & (1<<30)) // Read/write error
                    return -EIO;
            }
            if (port->int_status & (1<<30)) // Read/write error
                    return -EIO;
            while (port->command_issue) ;
            port->sata_active &= ~(1<<slot);

            blk += total_sects;
        }

        return 0;
    }

};

class ahci_pci_driver : public pci::pci_driver
{
    vector<unique_ptr<ahci_driver>> devs;
    hba_t* hba = nullptr;
public:
    ahci_pci_driver(pci::device_t& dev, uint32_t bar)
    {
        console::printf("Probing AHCI device at %02X:%02X:%02X\n",
                        dev.busid(), dev.slotid(), dev.fun());

        auto cmd = dev.command();
        cmd.memwrite  = true;
        cmd.busmaster = true;
        dev.writed(pci::REG_COMMAND, cmd.value);

        hba = static_cast<hba_t*>(memory::remap((void*)bar, sizeof(hba_t)));

        // Search ports
        uint32_t pi = hba->ports_implemented;
        for (int i=0; i<32; i++, pi >>= 1) {
            if (pi & 1) {
                console::printf("  Port %d: %s\n",
                                i, type_names[size_t(hba->ports[i].type())]);
                if (hba->ports[i].type() == type_t::SATA) // TODO SATAPI
                    devs.push_back(make_unique<ahci_driver>(i, hba->ports+i));
            }
        }
        console::printf(" OK\n");
    }

    const char* name() const override
    {
        return "AHCI PCI Driver";
    }
};

unique_ptr<pci::pci_driver> maker(pci::device_t& dev)
{
    if (dev.classcode() != CLASSCODE_SATA_AHCI)
        return {};
    uint32_t bar = dev.readd(pci::REG_BAR5);
    if ((bar & pci::BAR_TYPE_MASK) != pci::BAR_TYPE_32BIT)
        return {};
    bar = bar & pci::BAR_ADDR_MASK;

    return make_unique<ahci_pci_driver>(dev, bar);
}

void init()
{
    pci::register_driver(maker);
}

}
}
