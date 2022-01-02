# Weather Station for ESP32 compatible with Apple HomeKit

Temperature, humidity, and air quality measurements taken via DTH22 and MQ135.

This project uses:
- The Apple HomeKit ESP port: https://github.com/espressif/esp-apple-homekit-adk
- The u8g2 library for displaying measurements on an OLED display: https://github.com/olikraus/u8g2
- The u8g2 driver files to use the u8g2 with the ESP32: https://github.com/nkolban/esp32-snippets/tree/master/hardware/displays/U8G2
- The esp-idf-lib library for the DHT22 interface: https://github.com/UncleRus/esp-idf-lib

Follow instructions [here](https://github.com/espressif/esp-apple-homekit-adk/tree/1371579deea70bf6f1b39a470cad1668f5c9400c) to setup the ESP-IDF environment. Then:

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
- Or add the accessory manually with code 11122333 as the Setup code.

![QR Code](https://user-images.githubusercontent.com/17006198/147892343-719ffbee-4af7-4b9b-883a-367017ca3631.png)

Additional documentation is in the [esp-apple-homekit-adk](https://github.com/espressif/esp-apple-homekit-adk/tree/1371579deea70bf6f1b39a470cad1668f5c9400c) repo.
