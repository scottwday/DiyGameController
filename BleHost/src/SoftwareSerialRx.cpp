#include <Arduino.h>
#include "SoftwareSerialRx.h"

#define VERY_LONG 0xFFFFFFFF

SoftwareSerialRx::SoftwareSerialRx(uint32_t pin_rx, uint32_t baud, uint32_t tick_freq, get_ticks_fn_t get_ticks_fn)
{
    this->get_ticks_fn = get_ticks_fn;
    this->pin_rx = pin_rx;
    this->bit_ticks = tick_freq / baud;
}

void SoftwareSerialRx::begin(voidFuncPtr onEdge)
{
    this->last_edge_time = VERY_LONG;
    pinMode(this->pin_rx, INPUT_PULLDOWN);
    attachInterrupt(this->pin_rx, onEdge, CHANGE);
}

uint32_t SoftwareSerialRx::getNumQueuedRxBytes()
{
    return (this->rx_buffer_in - this->rx_buffer_out) % SW_SERIAL_RX_BUFFER_SIZE;
}

void SoftwareSerialRx::enqueueRxByte(byte data)
{
    if (this->getNumQueuedRxBytes() < SW_SERIAL_RX_BUFFER_SIZE - 1)
    {
        this->rx_buffer[this->rx_buffer_in] = data;
        this->rx_buffer_in = (this->rx_buffer_in + 1) % SW_SERIAL_RX_BUFFER_SIZE;
    }
}

bool SoftwareSerialRx::dequeueRxByte(byte *p_data)
{
    if (this->rx_buffer_in == this->rx_buffer_out)
        return false;

    *p_data = this->rx_buffer[this->rx_buffer_out];
    this->rx_buffer_out = (this->rx_buffer_out + 1) % SW_SERIAL_RX_BUFFER_SIZE;
    return true;
}

bool SoftwareSerialRx::read(uint8_t *p_data)
{
    if (!this->in_irq)
    {
        uint32_t t = this->get_ticks_fn();
        uint32_t bits = this->getBitsSinceLastEdge(t);

        if ((bits > 12) && (!this->idle))
        {
            this->idle = true;
            this->appendBits(9, 1);
        }
    }

    return dequeueRxByte(p_data);
}

void SoftwareSerialRx::onEdge()
{
    this->in_irq = true;

    // Get timer & state of pin
    bool pin_val = digitalRead(this->pin_rx);
    uint32_t t = this->get_ticks_fn();

    uint32_t bits = this->getBitsSinceLastEdge(t);
    this->last_edge_time = t;

    if (bits > 10) // Start bit
    {
        // finish remaining bits
        this->appendBits(9, 1);

        this->cur_bits = 0;
        this->cur_val = 0;
        this->idle = false;
    }
    else
    {
        this->appendBits(bits, pin_val);
    }

    this->idle = false;
    this->in_irq = false;
}

/// @brief Get number of bit times since last edge
/// @return number of bit times since last edge, return VERY_LONG if very long since last edge
uint32_t SoftwareSerialRx::getBitsSinceLastEdge(uint32_t cur_time)
{
    if (this->last_edge_time == VERY_LONG)
        return VERY_LONG;

    uint32_t dt = cur_time - this->last_edge_time;
    if (dt > 0x7FFFFFFF)
        return VERY_LONG;

    uint32_t bits = (dt + (this->bit_ticks / 2)) / this->bit_ticks;
    return bits;
}

void SoftwareSerialRx::appendBits(uint32_t bits, bool pin_val)
{
    // Cap remaining bits up to end of byte
    uint32_t bits_to_add = min(bits, 10 - (this->cur_bits % 10));
    if (bits_to_add > 0)
    {
        // Append bits to cur_bits
        this->cur_val |= (pin_val ? 0 : (1 << bits_to_add) - 1) << this->cur_bits;
        this->cur_bits += bits_to_add;
    }

    // Enqueue a byte if we have enough bits accumulated
    while (this->cur_bits >= 10)
    {
        // Strip off start and stop bits
        byte b = (this->cur_val >> 1) & 0xFF;
        this->enqueueRxByte(b);

        this->cur_val >>= 10;
        this->cur_bits -= 10;
    }
}
