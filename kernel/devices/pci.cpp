/* PCI bus.
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

#include <devices/pci.h>
#include <lib/klib.h>
#include <console.h>

namespace devices
{
namespace pci
{

static constexpr port_t CONFIG_ADDRESS = 0xCF8;
static constexpr port_t CONFIG_DATA    = 0xCFC;

static inline void out_config_addr(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset)
{
    const uint32_t addr =
        (1u<<31) | // Enable bit
        (uint32_t(bus) << 16) |
        (uint32_t(slot) << 11) |
        (uint32_t(fun) << 8) |
        offset;
    outd(CONFIG_ADDRESS, addr);
}

static inline uint32_t readd(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset)
{
    ASSERTH((offset & 3) == 0);
    out_config_addr(bus, slot, fun, offset);
    return ind(CONFIG_DATA);
}

static inline uint16_t readw(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset)
{
    return readd(bus, slot, fun, offset & ~3) >> ((offset & 3) * 8);
}

static inline void writed(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset, uint32_t data)
{
    ASSERTH((offset & 3) == 0);
    out_config_addr(bus, slot, fun, offset);
    outd(CONFIG_DATA, data);
}

static inline void writew(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset, uint16_t data)
{
    out_config_addr(bus, slot, fun, offset & ~3);
    uint32_t old = ind(CONFIG_DATA);
    uint8_t shift = (offset & 3) * 8;
    outd(CONFIG_DATA, (old & ~(0xFFFF << shift)) | (uint32_t(data) << shift));
}

static constexpr uint8_t NUM_SLOTS = 32;

device_t::device_t(uint8_t bus, const slot_t& slot, uint8_t fun) :
    _busid(bus), _slotid(slot.id()), _fun(fun), _slot(slot)
{
    const uint32_t fund = readd(REG_FUNINFO);
    _classcode.classcode = fund >> 8;
    _revisionid = (uint8_t) fund;
    if (_classcode.classcode == CLASSCODE_PCI_BRIDGE)
        bus_t::add_bus(readd(REG_BUS_NUM) >> 16);
}

void device_t::dump() const
{
    console::printf(
        "    device_t %d (Classcode %06X, Revision ID %02X)\n",
        _fun, _classcode.classcode, _revisionid);
}

uint32_t device_t::readd(uint8_t offset) const
{
    return pci::readd(_busid, _slotid, _fun, offset);
}

uint16_t device_t::readw(uint8_t offset) const
{
    return pci::readw(_busid, _slotid, _fun, offset);
}

void device_t::writed(uint8_t offset, uint32_t data)
{
    pci::writed(_busid, _slotid, _fun, offset, data);
}

void device_t::writew(uint8_t offset, uint16_t data)
{
    pci::writew(_busid, _slotid, _fun, offset, data);
}

bool device_t::probe(driver_factory factory)
{
    if (!_driver)
        _driver = factory(*this);
    return (bool)_driver;
}

slot_t::slot_t(const bus_t& bus, uint8_t slot) :
    _id(slot), _bus(bus)
{
    const uint32_t vendord = readd(bus.id(), slot, 0, 0);
    _vendor = (uint16_t) vendord;
    _device = vendord >> 16;

    if (_vendor == NONEXISTENT)
        return;

    devs.reserve(8);

    devs.push_back(device_t(bus.id(), *this, (uint8_t)0));
    if (readw(bus.id(), slot, 0, REG_HEADERTYPE) & 0x80) {
        // Multi-function device.
        for (uint8_t fun = 1; fun < 8; fun++) {
            if (readw(bus.id(), slot, fun, REG_VENDOR) != NONEXISTENT)
                devs.push_back(device_t(bus.id(), *this, fun));
        }
    }
}

void slot_t::dump() const
{
    console::printf("  slot_t %d (Vendor %04X device_t %04X):\n",
                    _id, _vendor, _device);
    for (const device_t& dev : devs)
        dev.dump();
}

bus_t::bus_t(uint8_t bus) : _id(bus)
{
   _slots.reserve(NUM_SLOTS);
    for (uint8_t slot=0; slot < NUM_SLOTS; slot++)
        _slots.push_back(slot_t(*this, slot));
}

void bus_t::dump() const
{
    console::printf("bus_t %d:\n", _id);
    for (const slot_t& slot : _slots)
        if (slot)
            slot.dump();
}


linked_list<bus_t> bus_t::busses;

void bus_t::add_bus(uint8_t bus)
{
    for (const bus_t& b : busses)
        if (b._id == bus)
            return;
    busses.push_back(bus_t(bus));
}

void bus_t::dump_all()
{
    for (const bus_t& b : busses) {
        b.dump();
        console::put('\n');
    }
}



void init()
{
    if (readw(0, 0, 0, REG_HEADERTYPE) & 0x80) {
        // Check for mulitple PCI host controllers
        for (uint8_t bus = 0; bus < 8; bus++) {
            if (readw(0, 0, bus, REG_VENDOR) != NONEXISTENT)
                break;
            bus_t::add_bus(bus);
        }
    } else
        bus_t::add_bus(0);

    bus_t::dump_all();
}

void register_driver(driver_factory factory)
{
    for (bus_t& b : bus_t::get_busses())
        for (slot_t& s : b.slots())
            if (s)
                for (device_t& dev : s.devices())
                    dev.probe(factory);
}

}
}
