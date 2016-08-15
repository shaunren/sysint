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

static uint32_t readd(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset)
{
    const uint32_t addr =
        (1u<<31) | // Enable bit
        (uint32_t(bus) << 16) |
        (uint32_t(slot) << 11) |
        (uint32_t(fun) << 8) |
        offset;
    outd(CONFIG_ADDRESS, addr);
    return ind(CONFIG_DATA);
}

static uint16_t readw(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t offset)
{
    return readd(bus, slot, fun, offset & ~3) >> (offset & 2 ? 16 : 0);
}

enum Reg
{
    REG_VENDOR = 0,
    REG_DEVICE = 2,
    REG_FUNINFO = 8,
    REG_HEADERTYPE = 14,
    REG_BAR0 = 0x10,
    REG_BUS_NUM = 0x18,
};

static constexpr uint8_t NUM_SLOTS = 32;

Function::Function(uint8_t bus, const Slot& slot, uint8_t fun) :
    _fun(fun), base_addrs{0}, _slot(slot)
{
    const uint32_t fund = readd(bus, slot.id(), fun, REG_FUNINFO);
    _classid = fund >> 24;
    _subclass = fund >> 16;
    _progif = fund >> 8;
    _revisionid = (uint8_t) fund;
    base_addrs[0] = readd(bus, slot.id(), fun, REG_BAR0);
    base_addrs[1] = readd(bus, slot.id(), fun, REG_BAR0 + 4);
    if (_classid == CLASS_BRIDGE && _subclass == 0x04 /* PCI BRIDGE*/)
        Bus::add_bus(readd(bus, slot.id(), fun, REG_BUS_NUM) >> 16);
    else
        for (int i=2; i<6; i++)
            base_addrs[i] = readd(bus, slot.id(), fun, REG_BAR0 + 4*i);
}

void Function::dump() const
{
    console::printf(
        "    Function %d\n"
        "     Class %02X, Subclass %02X, Prog IF %02X, Revision ID %02X\n",
        _fun, _classid, _subclass, _progif, _revisionid);
}

Slot::Slot(const Bus& bus, uint8_t slot) :
    _id(slot), _bus(bus)
{
    const uint32_t vendord = readd(bus.id(), slot, 0, 0);
    _vendor = (uint16_t) vendord;
    _device = vendord >> 16;

    if (_vendor == NONEXISTENT)
        return;

    funs.push_back(Function(bus.id(), *this, (uint8_t)0));
    if (readw(bus.id(), slot, 0, REG_HEADERTYPE) & 0x80) {
        // Multi-function device.
        for (uint8_t fun = 1; fun < 8; fun++) {
            if (readw(bus.id(), slot, fun, REG_VENDOR) != NONEXISTENT)
                funs.push_back(Function(bus.id(), *this, fun));
        }
    }
}

void Slot::dump() const
{
    console::printf("  Slot %d (Vendor %04X Device %04X):\n",
                    _id, _vendor, _device);
    for (const Function& fun : funs)
        fun.dump();
}

Bus::Bus(uint8_t bus) : _id(bus)
{
    slots.reserve(NUM_SLOTS);
    for (uint8_t slot=0; slot < NUM_SLOTS; slot++)
        slots.push_back(Slot(*this, slot));
}

void Bus::dump() const
{
    console::printf("Bus %d:\n", _id);
    for (const Slot& slot : slots)
        if (slot)
            slot.dump();
}


vector<Bus> Bus::busses;

void Bus::add_bus(uint8_t bus)
{
    for (const Bus& b : busses)
        if (b._id == bus)
            return;
    busses.push_back(Bus(bus));
}

void Bus::dump_all()
{
    for (const Bus& b : busses) {
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
            Bus::add_bus(bus);
        }
    } else
        Bus::add_bus(0);

    Bus::dump_all();
}

}
}
