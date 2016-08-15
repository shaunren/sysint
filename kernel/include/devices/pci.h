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

class Slot;
class Bus;

class Function
{
    uint8_t _fun, _classid, _subclass, _progif, _revisionid;

    uint32_t base_addrs[6];

    const Slot& _slot;

    explicit Function(uint8_t bus, const Slot& slot, uint8_t fun);
    friend class Slot;

public:
    uint8_t fun() const { return _fun; }
    uint8_t classid() const { return _classid; }
    uint8_t subclass() const { return _subclass; }
    uint8_t progif() const { return _progif; }
    uint8_t revisionid() const { return _revisionid; }
    const Slot& slot() const { return _slot; }

    uint8_t base_addr(int i) const { return base_addrs[i]; }

    void dump() const;
};

constexpr uint16_t NONEXISTENT = 0xffff;

class Slot
{
    uint8_t _id;
    uint16_t _vendor, _device;
    const Bus& _bus;
    vector<Function> funs;

    explicit Slot(const Bus& bus, uint8_t slot);
    friend class Bus;

public:
    Slot(Slot&& o) :
        _id(o._id), _vendor(o._vendor), _device(o._device), _bus(o._bus),
        funs(std::move(o.funs)) {}

    bool exists() const { return _vendor != NONEXISTENT; }
    explicit operator bool() const { return exists(); }

    uint8_t id() const { return _id; }
    const Bus& bus() const { return _bus; }
    const vector<Function>& functions() { return funs; }

    void dump() const;
};

class Bus
{
    uint8_t _id;
    vector<Slot> slots;

    explicit Bus(uint8_t bus);
    friend void init();
    friend class Function;

    static vector<Bus> busses;

    static void add_bus(uint8_t bus);

public:
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&& o) : _id(o._id), slots(std::move(o.slots)) {}

    uint8_t id() const { return _id; }
    const Slot& operator[](size_t i) const { return slots[i]; }

    void dump() const;

    static const vector<Bus>& get_busses() { return busses; }
    static void dump_all();
};


void init();

}
}

#endif  /* _PCI_H_ */
