# ST7735 SD Media Player
SD File Browser and Viewer

Smooth video playback from SD on STM32 and ST7735 1.8" 128x160 using DMA and fast SPI 36Mbps interface

Achieved 41 fps using fast SD card. 160x128 pixel JPEG image decoding takes 100 ms or less.

YouTube videos:

https://youtu.be/6Uh5Iu-erO0

https://youtu.be/o3AqITHf0mo

https://youtu.be/4PwaX-zusPM


More ST7735 and STM32 videos:

https://www.youtube.com/watch?v=o3AqITHf0mo&list=PLxb1losWErZ6y6GombzvtwRZ2l7brPv1s


## Connections (header at the top):

|LCD pin|LCD pin name|STM32|
|--|--|--|
 |#01| LED| 3.3V|
 |#02| SCK |PA5/SCK|
 |#03| SCA |PA7/MOSI|
 |#04| A0/DC|PA1 or any digital
 |#05| RESET|PA0 or any digital|
 |#06| CS|PA2 or any digital|
 |#07| GND | GND|
 |#08| VCC | 3.3V|

|SD pin|SD pin name|STM32|
|--|--|--|
|#01| SD_SCK| PA5|
|#02| SD_MISO |PA6|
|#03| SD_MOSI |PA7|
|#04| SD_CS |PA4|


## Features:
- SD file browser with one button
- Short click for next file/switch stat mode
- Long click to show file or exit the viewer
- Semi-transparent progress bar
- Long file names (up to 23 characters fit on the screen) and file size displayed
- RAW 160x128 video files supported @ 41fps
- BMP pictures in 24-bit and 8/4-bit with palette
- basic text files viewer
- JPEG photos support (even high resolution) thanks to JpgDecode_STM library

## Comments:
- SD uses faster STM32 SPI1 interface which supports 36 Mbps
- SPI1 is shared between LCD and SD card
- Not all SD cards work at 36MBps
- Fast card at 36Mbps gives 41fps for 160x128 video
- SdFat library uses DMA for SPI transfer
- Big buffer in RAM is used to speed up SPI/DMA transfer 
- Developed and tested with stm32duino and Arduino IDE 1.6.5
- Requires Arduino_ST7735_STM, SdFat, JpgDecoder_STM and RREFont libraries and stm32duino
 
If you find it useful and you want to buy me a coffee or a beer:

https://www.paypal.me/cbm80amiga
