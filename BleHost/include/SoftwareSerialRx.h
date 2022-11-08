#ifndef SOFTWARE_SERIAL_RX_H
#define SOFTWARE_SERIAL_RX_H

#define SW_SERIAL_RX_BUFFER_SIZE 16

typedef uint32_t (*get_ticks_fn_t)();

/// @brief Rx-Only Generic Software Serial Implementation. Only supports 8-N-1 format
class SoftwareSerialRx
{
private:
    get_ticks_fn_t get_ticks_fn;
    uint32_t bit_ticks;
    uint16_t pin_rx;
    uint32_t last_edge_time;
    uint32_t cur_val;
    uint32_t cur_bits;
    uint32_t rx_buffer_in;
    uint32_t rx_buffer_out;
    volatile bool in_irq;
    bool idle;
    byte rx_buffer[SW_SERIAL_RX_BUFFER_SIZE];

    uint32_t getBitsSinceLastEdge(uint32_t cur_time);
    void appendBits(uint32_t bits, bool pin_val);
    void enqueueRxByte(byte data);
    bool dequeueRxByte(byte *p_data);

public:
    byte last_byte;

    /// @brief Create a SoftwareSerialRx instance
    /// @param pin_rx Pin to listen to, must support interrupts
    /// @param baud Baud rate, won't work for very high bauds, depending on your CPU
    /// @param tick_freq Timebase for the ticks function
    /// @param get_ticks_fn Function to grab a counter value, eg, []{ return SysTick->VAL; }
    SoftwareSerialRx(uint32_t pin_rx, uint32_t baud, uint32_t tick_freq, get_ticks_fn_t get_ticks_fn);

    /// @brief Start listening on the serial port
    /// @param onEdge Pin change IRQ handler
    void begin(voidFuncPtr onRisingEdge);

    /// @brief Read a byte from the rx queue (if any)
    /// @param pData pointer to byte to populate
    /// @return True if a byte was fetched
    bool read(uint8_t *pData);

    /// @brief Pin change IRQ
    void onEdge();

    /// @brief Get number of bytes in queue
    /// @return Number of bytes in queue
    uint32_t getNumQueuedRxBytes();
};

#endif // SOFTWARE_SERIAL_RX_H