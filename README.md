Inspired by [xkcd 363](https://xkcd.com/363/)

# software
iemand.ino
Gratification first, so first get display running. Then updating using the button.
Then figure out how to set the time for the RTC and display that somewhat nicely.
THEN figure out how to deep sleep and wake using RTC.

Partial update is a nice feature to use as well, let's see if that works by making an e-paper clock out of it. [Note](https://forum.arduino.cc/t/gxepd2-partial-update-with-deep-sleep/897160) about constructor with partial update (specifically after deep sleep)
# hardware 
- [Firebeetle 32](https://www.dfrobot.com/product-1590.html) for low sleep current and lipo charger
	- Relevant: led_builtin is D9
- [RTC SD3031](https://www.dfrobot.com/product-2656.html) with INT pin to wake firebeetle on set times
- button from ALI
- battery from laptop 11wH?
- [2.9 inch waveshare e-paper](https://www.tinytronics.nl/nl/displays/e-ink/waveshare-2.9-inch-e-ink-e-paper-display) display [wiki](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_Manual#Working_With_Arduino)
## connections:
### battery to firebeetle
2 connections, maybe do something with an ADC pin and a voltage divider to read battery state? I looked into this for LikeJar
### button with LED to firebeetle
button has 2 connections
- gnd black (green wire)
- io D3 IO26 (green wire) (wake pin, figure out lowest sleep wake pin on esp)
LED as well pwm
- gnd black
- pwm output D4 IO27 (red wire)
### rtc to firebeetle
i2c protocol so 
- gnd black
- vcc red
- sda yellow
- scl yellow
- int probably D2 is good, IO25 (wake pin, figure out lowest sleep wake pin on esp)
### display to firebeetle
spi protocol, only one way
- vcc red
- gnd black
- din orange
- clk orange
- cs D8
- dc D7
- rst D6
- busy D5


![[Pasted image 20240625221035.png]]

# issues (lessons learned)
I used this function to set the time in the RTC after the NTP did it's thing. But apparently I was a little too quick. So the RTC was set to 2 seconds after epoch. 
`rtc.setTime(tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);//Initialize time`

I didn't catch up to this since it always returned 1 for hour and minute. I was guessing some out of bounds/overroll error in one of the datatypes.
The solution was simpler:
```
while (now < 1000000) {
  time(&now);
}
```

Wait untill now is set to something larger, one million is arbritrary here. Assuming (big if) wifi works this should be done in 2 seconds. The logic for choosing the 'best' time source still has to be thought out and written at this stage.

# hardware suggestie
lilygo heeft 2 producten met 2,13 inch scherm voor ~16 euro op dit moment bij tinytronics

Verbruikt wat te veel volgens deze thread
https://www.reddit.com/r/esp32/comments/idinjr/36ma_deep_sleep_in_an_eink_ttgo_t5_v23/