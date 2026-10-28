<div align="center">

<img src="https://visitor-badge.laobi.icu/badge?page_id=papercodeIN.Nextion-SONOFF" alt="visitors" height="20" />

# Nextion & SONOFF Projects

**Nextion HMI display projects — editor files, firmware, wiring and build guides for ESP32, ESP8266 and Raspberry Pi Pico**

[![Displays](https://img.shields.io/badge/displays-Nextion_HMI-0B6AB3)](https://itead.cc/)
[![Sensors](https://img.shields.io/badge/sensors-SONOFF-E6062B)](https://sonoff.tech/)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino)](https://www.arduino.cc/)
[![YouTube](https://img.shields.io/badge/YouTube-Fusion_Automate-FF0000?logo=youtube)](https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw)

</div>

---

## About

Complete Nextion HMI projects and per-display guides from Fusion Automate. Each project folder contains the **Nextion Editor `.HMI` file** (the full display UI), the matching microcontroller firmware, wiring photos and build notes — so you can open the display project in the Nextion Editor and flash the sketch without reverse-engineering anything.

Displays covered: the **Nextion Intelligent**, **Discovery** and **Edge** series, alongside SONOFF wireless sensors.

## Display guides

| Display | Series | Guide |
|---|---|---|
| NX4827P043-011C-Y | Intelligent Series, 4.3″ | [README](<NX4827P043-011C-Y - Intelligent Series 4.3″/README.md>) |
| NX4832F035 | Discovery Series, 3.5″ | [README](<NX4832F035 - Discovery Series 3.5″/README.md>) |
| NX4848E021-011C | Edge Series, 2.1″ | [README](<NX4848E021-011C – Edge Series 2.1″/README.md>) |
| NX4880E043-011C | Edge Series, 4.3″ | [README](<NX4880E043-011C – Edge Series 4.3″/README.md>) |

## Projects — NX4832F035 Discovery Series 3.5″

All projects live under [`NX4832F035 - Discovery Series 3.5″/Projects/`](<NX4832F035 - Discovery Series 3.5″/Projects>):

| Project | Target | Description |
|---|---|---|
| [GPIO Control](<NX4832F035 - Discovery Series 3.5″/Projects/GPIO_Control_ESP32 - 16-OCT-2025/README.md>) | ESP32 | Switch GPIO states from the Nextion UI — latest revision (16 Oct 2025) |
| [GPIO Control](<NX4832F035 - Discovery Series 3.5″/Projects/GPIO_Control_ESP32 - 10-OCT-2025>) | ESP32 | Earlier revision (10 Oct 2025) |
| [GPIO Control](<NX4832F035 - Discovery Series 3.5″/Projects/GPIO_Control_RPI-PICO - 10-OCT-2025>) | Raspberry Pi Pico | Same HMI driving a Pico via MicroPython |
| [Weather Clock](<NX4832F035 - Discovery Series 3.5″/Projects/Weather_Clock_ESP32 - 10-OCT-2025>) | ESP32 | Clock + weather screens for the Nextion display |
| [Weather Clock + DHT11](<NX4832F035 - Discovery Series 3.5″/Projects/Weather_Clock_ESP32_DHT11 - 10-OCT-2025>) | ESP32 | Adds local temperature/humidity from a DHT11 sensor |
| [WiFi Clock](<NX4832F035 - Discovery Series 3.5″/Projects/WiFi_Clock_ESP32 - 15-OCT-2025/README.md>) | ESP32 | NTP-synced Wi-Fi clock — latest revision (15 Oct 2025) |
| [WiFi Clock](<NX4832F035 - Discovery Series 3.5″/Projects/WiFi_Clock_ESP32 - 10-OCT-2025>) | ESP32 | Earlier revision (10 Oct 2025) |
| [WiFi Clock](<NX4832F035 - Discovery Series 3.5″/Projects/WiFi_Clock_ESP8266 - 15-OCT-2025/README.md>) | ESP8266 | Same clock ported to the ESP8266 |

## Nextion Editor fonts

Ready-to-import font assets for the Nextion Editor, in [`Font/`](Font):

| File | Use |
|---|---|
| `digital-7.zip` | Classic seven-segment "digital-7" family (48 / 56 Bold / 64 / 80 px variants) |
| `MSGothic40B.zi` | MS Gothic 40 px bold — clean bitmap font for labels |

Import via **Nextion Editor → Fonts → Add the generated .zi font**.

## Featured products

Hardware used in these projects — every purchase through these links supports the channel, and the coupon below works at checkout:

| Product | Series | Buy |
|---|---|---|
| NX4827P043-011C-Y | Nextion 4.3″ Intelligent Series HMI Touch Display (with enclosure) | [Buy](https://itead.cc/product/4-3-nextion-intelligent-series-hmi-touch-display-with-enclosure/ref/314/) |
| NX4832F035 | Nextion 3.5″ Discovery Series HMI Touch Display | [Buy](https://itead.cc/product/nx4832f035-nextion-3-5-discovery-series-hmi-touch-display/ref/314/) |
| NX4848E021-011C | Nextion 2.1″ Edge Series HMI Touch Display | [Buy](https://itead.cc/product/nx4848e021-011c-nextion-2-1-edge-series-hmi-touch-display/ref/314/) |
| NX4880E043-011C | Nextion 4.3″ Edge Series HMI Touch Display | [Buy](https://itead.cc/product/nx4880e043-011c-nextion-4-3-edge-series-hmi-touch-display/ref/314/) |
| Nextion Foca Max | USB-to-TTL serial converter for flashing displays | [Buy](https://itead.cc/product/nextion-foca-max-5v2a-output-usb-to-ttl-serial-converter-board/ref/314/) |
| Nextion BOX Speaker | 2 W speaker module for multimedia HMI projects | [Buy](https://itead.cc/product/nextion-box-speaker/ref/314/) |
| Nextion SDExtender | Micro-SD extender for loading display assets | [Buy](https://itead.cc/product/nextion-micro-sd-card-extender/ref/314/) |
| SONOFF DW2-RF | 433 MHz wireless door/window sensor | [Buy](https://itead.cc/product/sonoff-dw2-rf-433mhz-rf-wireless-door-window-sensor/ref/314/) |
| SONOFF DW2-Wi-Fi | Wireless door/window sensor | [Buy](https://itead.cc/product/sonoff-dw2/ref/314/) |

**10% coupon code: `PYLINSONOFF`** ([referral link](https://itead.cc/ref/314/))

## Video shorts

Quick demos and unboxings on the [Fusion Automate YouTube channel](https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw):

1. [Unboxing: Nextion HMI, Box Speaker, SD Extender, Foca Max](https://youtube.com/shorts/QhYF7tbJfUA)
2. [4.3″ Nextion Intelligent Series demo](https://youtube.com/shorts/-_VOizNZ0M0)
3. [Unscrewing the 4.3″ Nextion Intelligent Series](https://youtube.com/shorts/pvZBPkx6Nbg)
4. [Nextion BOX Speaker unboxing (2 W, 100 Hz–3 kHz)](https://youtube.com/shorts/3-6pe_mP1xQ)
5. [SD Extender with a Raspberry Pi](https://youtube.com/shorts/4_cNtVUnO7A)
6. [SD Extender module unboxing](https://youtube.com/shorts/s8hJhr4Kst0)
7. [Discovery Series 3.5″ resistive touchscreen intro](https://youtube.com/shorts/BouULLZ8EqA)
8. [SONOFF DW2-WiFi door/window sensor unboxing](https://youtube.com/shorts/iy4qzqwsN58)

## Getting started

1. Install the [Nextion Editor](https://nextion.tech/nextion-editor/) (free) to open the `.HMI` files.
2. Pick a project above, open the `.HMI` in the editor and upload the display assets to the panel (direct serial or an [Foca Max](https://itead.cc/product/nextion-foca-max-5v2a-output-usb-to-ttl-serial-converter-board/ref/314/)).
3. Flash the matching Arduino sketch to the ESP32/ESP8266/Pico and follow the wiring photo in the project folder.

## Support

If these projects help you, consider supporting the channel:

<a href="https://buymeacoffee.com/pylin" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" height="36"></a>
&nbsp;&nbsp;
<a href="https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw" target="_blank"><img src="https://img.shields.io/badge/SUBSCRIBE%20ON%20YOUTUBE-FF0000?style=for-the-badge&logo=youtube&logoColor=white" alt="Subscribe on YouTube" height="36"></a>

## Contact

| Method | Details |
|---|---|
| Call / WhatsApp | [+91-9974477759](https://wa.me/919974477759) |
| Telegram | [@fusionautomate](https://t.me/fusionautomate) |
| Email | [eng.innovativ@gmail.com](mailto:eng.innovativ@gmail.com) |
