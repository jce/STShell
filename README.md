# STShell

An interactive shell and flash-filesystem experiment for the STM32F303VCT. STShell runs a command shell over UART (via the onboard ST-LINK virtual COM port) and exposes 128 KB of the MCU's internal flash as a USB Mass Storage device.

> This is a personal learning project. It is not a library or product.

## Features

- Interactive shell on USART1, routed through the ST-LINK VCP
- Line editor with insert and delete at cursor
- Flash-backed USB Mass Storage — 128 KB of internal flash as a small FAT volume
- Flash wear metering: per-page erase counters, including erases performed by the host PC
- First-boot detection via a linker-placed flag page; firmware re-flashes are fed into the wear counters
- LSM303AGR magnetometer/accelerometer readout via interrupt-driven I2C at 10 Hz
- Stack high-water measurement (paint and inspect)

## Shell commands

```
help       Shows help.
?          Help alias.
version    Shows versions.
test       Test arguments.
clear      Clear screen.
live       Live view of peripherals.
rd         [begin [length]] Read memory location.
stack      [paint, show] display stack max usage.
mag        Shows magnetometer readout.
lin        Shows linear accelerometer readout.
page       [0-7F] Reads and prints flash page.
flashfill  [0-7F 0-FFFFFFFF] Fills a flash page with a pattern.
flasherase [0-7F] Erases flash page.
printf     Write something to printf.
wear       Readout wear counters.
```

## Flash wear metering

Internal flash supports a limited number of erase cycles per page, and a flash-backed filesystem creates a skewed distribution: the FAT pages erase far more often than cold data. The `wear` command dumps per-page erase counts so this can be measured rather than assumed.

The counters are stored entirely in flash — each erase is tallied as halfwords in a reserved page, with automatic consolidation when a channel fills.

## Project layout

- `Core/` — application code: shell, command table, flash counters, first-boot flag
- `Drivers/` — HAL plus the LSM303AGR readout chain
- `Middlewares/ST/STM32_USB_Device_Library/`, `USB_DEVICE/` — USB MSC stack on internal flash
- `docs/` — datasheets
- `STM32F303VCTX_FLASH.ld` — linker script, including the reserved wear-counter and first-boot pages

## Building

Built with STM32CubeIDE and STM32CubeMX (`blink_led_2.ioc`). Import the project into CubeIDE and build; flash via the ST-LINK debugger.

## Roadmap

- Write-back cache with 100 ms flush to coalesce sector writes into page writes
- Targeted wear leveling for the FAT zone, if wear data ever justifies it

## License

[MIT](LICENSE)
