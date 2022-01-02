# Weather Station for ESP32 compatible with Apple HomeKit

Follow instructions (here)[https://github.com/espressif/esp-apple-homekit-adk/tree/1371579deea70bf6f1b39a470cad1668f5c9400c] to setup the ESP-IDF environment. Then:

```bash
git clone --recursive https://github.com/marcodeltutto/esp-apple-homekit-weather-station.git
cd esp-apple-homekit-weather-station
export ESPPORT=/dev/tty.SLAB_USBtoUART #Set your board's serial port here
idf.py set-target esp32
idf.py flash
esptool.py -p $ESPPORT write_flash 0x340000 accessory_setup.bin
idf.py monitor
```

Open the Home app on your iPhone/iPad and follow these steps:

- Click on "Add Accessory".
- Scan the QR code below.
- Enter 11122333 as the Setup code.