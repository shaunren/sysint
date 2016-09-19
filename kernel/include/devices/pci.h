/* PCI bus declarations.
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

#ifndef _PCI_H_
#define _PCI_H_

#include <stdint.h>
#include <lib/vector.h>
#include <lib/linked_list.h>
#include <memory>
#include <functional>

#include <devices/driver.h>

namespace devices
{
namespace pci
{

enum Class
{
    CLASS_UNKNOWN = 0,

    CLASS_STORAGE,
    CLASS_NETWORK,
    CLASS_DISPLAY,
    CLASS_MULTIMEDIA,
    CLASS_MEMORY,
    CLASS_BRIDGE,
    CLASS_SIMPLE_COMM,
    CLASS_BASE_PERIPHERALS,
    CLASS_INPUT,
    CLASS_DOCKING_STATION,
    CLASS_PROCESSORS,
    CLASS_SERIAL_BUS,
    CLASS_WIRELESS,
    CLASS_SATELLITE_COMM,
    CLASS_CRYPTO,
    CLASS_SIGNAL_PROCESSORS,

    CLASS_RESERVED_BEGIN,
    CLASS_RESERVED_END = 0xFE,

    CLASS_OTHER = 0xFF,
};

// Subclasses
constexpr uint8_t SUBCLASS_BRIDGE_PCI = 0x04;

constexpr uint8_t SUBCLASS_STORAGE_IDE = 0x01;
constexpr uint8_t SUBCLASS_STORAGE_SATA = 0x06;

// Classcodes
constexpr uint32_t CLASSCODE_PCI_BRIDGE = 0x060400;

class slot_t;
class bus_t;

union classcode_t
{
    struct
    {
        uint8_t classid;
        uint8_t subclass;
        uint8_t progif;
        uint8_t zero;
    } __attribute__((packed));
    uint32_t classcode;
};

enum Reg
{
    REG_VENDOR = 0,
    REG_DEVICE = 2,
    REG_COMMAND = 4,
    REG_STATUS = 6,
    REG_FUNINFO = 8,
    REG_HEADERTYPE = 14,
    REG_BAR0 = 0x10,
    REG_BAR1 = REG_BAR0 + 4,
    REG_BAR2 = REG_BAR1 + 4,
    REG_BAR3 = REG_BAR2 + 4,
    REG_BAR4 = REG_BAR3 + 4,
    REG_BAR5 = REG_BAR4 + 4,
    REG_BUS_NUM = 0x18,
};

union command_t
{
    uint16_t value;
    struct
    {
        bool iospace : 1;
        bool memspace : 1;
        bool busmaster : 1;
        bool special_cycles : 1;
        bool memwrite : 1;
        bool vgasnoop : 1;
        bool parity_err_response : 1;
        bool _bit7 : 1;
        bool serr : 1;
        bool fast_back_to_back : 1;
        bool int_disable : 1;
        uint8_t _rsvd : 5;
    };
};

class device_t;

class pci_driver
{
public:
    pci_driver() = default;
    virtual ~pci_driver() = default;
    pci_driver(const pci_driver&) = delete;
    pci_driver& operator=(const pci_driver&) = delete;
    virtual const char* name() const = 0;
};


using driver_factory = std::function<std::unique_ptr<pci_driver>(device_t&)>;

constexpr uint32_t BAR_MEM_MASK = 1;
constexpr uint32_t BAR_TYPE_MASK = 3 << 1;
constexpr uint32_t BAR_TYPE_32BIT = 0;
constexpr uint32_t BAR_TYPE_1M = 1 << 1;
constexpr uint32_t BAR_TYPE_64BIT = 2 << 1;
constexpr uint32_t BAR_PREFETCH_MASK = 1 << 3; // Only exists for memory space
constexpr uint32_t BAR_ADDR_MASK = ~15;

class device_t
{
    uint8_t _busid, _slotid;
    uint8_t _fun;
    classcode_t _classcode;
    uint8_t _revisionid;

    const slot_t& _slot;

    std::unique_ptr<pci_driver> _driver;

    explicit device_t(uint8_t bus, const slot_t& slot, uint8_t fun);
    friend class slot_t;

public:
    uint8_t busid() const { return _busid; }
    uint8_t slotid() const { return _slotid; }
    uint8_t fun() const { return _fun; }
    uint8_t classid() const { return _classcode.classid; }
    uint8_t subclass() const { return _classcode.subclass; }
    uint8_t progif() const { return _classcode.progif; }
    uint8_t revisionid() const { return _revisionid; }
    uint32_t classcode() const { return _classcode.classcode; }
    const slot_t& slot() const { return _slot; }

    void dump() const;

    // Read/write configuration space registers.
    uint32_t readd(uint8_t offset) const; // offset aligned to 4-byte boundary
    uint16_t readw(uint8_t offset) const; // NOTE truncates at 4-byte boundary.
    void writed(uint8_t offset, uint32_t data); // offset aligned to 4-byte boundary
    void writew(uint8_t offset, uint16_t data); // NOTE truncates at 4-byte boundary.

    uint8_t bar(int i) const { return readd(REG_BAR0 + 4*i); }
    command_t command() const { return {readw(REG_COMMAND)}; }

    pci_driver* driver() const { return _driver.get(); }
    bool probe(driver_factory factory);
};

constexpr uint16_t NONEXISTENT = 0xffff;

class slot_t
{
    uint8_t _id;
    uint16_t _vendor, _device;
    const bus_t& _bus;
    vector<device_t> devs;

    explicit slot_t(const bus_t& bus, uint8_t slot);
    friend class bus_t;

public:
    slot_t(slot_t&& o) :
        _id(o._id), _vendor(o._vendor), _device(o._device), _bus(o._bus),
        devs(std::move(o.devs)) {}

    bool exists() const { return _vendor != NONEXISTENT; }
    explicit operator bool() const { return exists(); }

    uint8_t id() const { return _id; }
    const bus_t& bus() const { return _bus; }
    const vector<device_t>& devices() { return devs; }

    void dump() const;
};

class bus_t
{
    uint8_t _id;
    vector<slot_t> _slots;

    explicit bus_t(uint8_t bus);
    friend void init();
    friend class device_t;

    static linked_list<bus_t> busses;

    static void add_bus(uint8_t bus);

public:
    explicit bus_t() = default;
    bus_t(const bus_t&) = delete;
    bus_t& operator=(const bus_t&) = delete;
    bus_t(bus_t&& o) : _id(o._id), _slots(std::move(o._slots)) {}

    uint8_t id() const { return _id; }
    const slot_t& operator[](size_t i) const { return _slots[i]; }
    vector<slot_t>& slots() { return _slots; }

    void dump() const;

    static linked_list<bus_t>& get_busses() { return busses; }
    static void dump_all();
};


void init();

void register_driver(driver_factory factory);

}
}

#endif  /* _PCI_H_ */
