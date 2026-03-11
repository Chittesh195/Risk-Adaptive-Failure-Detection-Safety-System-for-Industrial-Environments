from machine import Pin, I2C, PWM, ADC
import time

# ================= LCD DRIVER =================

i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=100000)
addr = 0x27
LCD_BACKLIGHT = 0x08
ENABLE = 0x04

def lcd_write_byte(data):
    i2c.writeto(addr, bytearray([data | LCD_BACKLIGHT]))
    time.sleep_us(50)

def lcd_toggle_enable(data):
    lcd_write_byte(data | ENABLE)
    time.sleep_us(50)
    lcd_write_byte(data & ~ENABLE)
    time.sleep_us(50)

def lcd_send_nibble(nibble):
    lcd_write_byte(nibble)
    lcd_toggle_enable(nibble)

def lcd_send_byte(data, mode):
    high = mode | (data & 0xF0)
    low = mode | ((data << 4) & 0xF0)
    lcd_send_nibble(high)
    lcd_send_nibble(low)

def lcd_command(cmd):
    lcd_send_byte(cmd, 0)

def lcd_data(data):
    lcd_send_byte(data, 1)

def lcd_init():
    time.sleep_ms(50)
    lcd_send_nibble(0x30)
    time.sleep_ms(5)
    lcd_send_nibble(0x30)
    time.sleep_us(200)
    lcd_send_nibble(0x30)
    lcd_send_nibble(0x20)
    lcd_command(0x28)
    lcd_command(0x08)
    lcd_command(0x01)
    time.sleep_ms(2)
    lcd_command(0x06)
    lcd_command(0x0C)

def lcd_clear():
    lcd_command(0x01)
    time.sleep_ms(2)

def lcd_move_to(col,row):
    lcd_command(0x80 + (0x40 * row) + col)

def lcd_putstr(text):
    for ch in text:
        lcd_data(ord(ch))

def lcd_create_char(location, charmap):
    location &= 0x7
    lcd_command(0x40 | (location << 3))
    for row in charmap:
        lcd_data(row)

lcd_init()

# ================= CUSTOM ICONS =================

smoke_icon = [0b00100,0b01110,0b10101,0b00100,0b01110,0b00100,0b00000,0b00000]
flame_icon = [0b00100,0b01110,0b11111,0b11111,0b01110,0b00100,0b00000,0b00000]
rain_icon  = [0b00000,0b01110,0b11111,0b00000,0b01010,0b00000,0b01010,0b00000]

lcd_create_char(0, smoke_icon)
lcd_create_char(1, flame_icon)
lcd_create_char(2, rain_icon)

lcd_clear()

# ================= SENSORS =================

smoke = Pin(2, Pin.IN, Pin.PULL_UP)
flame = Pin(3, Pin.IN, Pin.PULL_UP)
rain_adc = ADC(26)

buzzer = Pin(5, Pin.OUT)

# ================= SERVOS =================

exit_servo = PWM(Pin(6))
window_servo = PWM(Pin(7))

exit_servo.freq(50)
window_servo.freq(50)

def set_servo_angle(servo, angle):
    duty = int(2000 + (angle/180)*6000)
    servo.duty_u16(duty)

set_servo_angle(exit_servo, 0)
set_servo_angle(window_servo, 90)

RAIN_THRESHOLD = 50000

# ================= VARIABLES =================

screen = 0
last_change = time.ticks_ms()

# ================= MAIN LOOP =================

while True:

    now = time.ticks_ms()

    smoke_detect = (smoke.value() == 0)
    flame_detect = (flame.value() == 0)
    rain_detect = (rain_adc.read_u16() < RAIN_THRESHOLD)

    # ========= FIRE DETECTED =========
    if flame_detect:

        lcd_clear()
        lcd_data(1)
        lcd_putstr(" FIRE ALERT")
        lcd_move_to(0,1)
        lcd_putstr(" Exit OPEN")

        set_servo_angle(exit_servo, 90)
        buzzer.value(1)

        time.sleep(10)  # Display + open for 10 sec

        set_servo_angle(exit_servo, 0)
        buzzer.value(0)

        lcd_clear()
        continue

    # ========= SMOKE DETECTED =========
    if smoke_detect:
        lcd_clear()
        lcd_data(0)
        lcd_putstr(" Smoke DETECT")
        time.sleep(5)
        continue

    # ========= RAIN DETECTED =========
    if rain_detect:
        lcd_clear()
        lcd_data(2)
        lcd_putstr(" Rain DETECT")
        set_servo_angle(window_servo, 0)
        time.sleep(5)
        continue
    else:
        set_servo_angle(window_servo, 90)

    # ========= NORMAL SEQUENCE =========
    if time.ticks_diff(now, last_change) > 3000:

        screen = (screen + 1) % 3
        last_change = now
        lcd_clear()

        if screen == 0:
            lcd_data(0)
            lcd_putstr(" Smoke SAFE")

        elif screen == 1:
            lcd_data(1)
            lcd_putstr(" Flame SAFE")

        elif screen == 2:
            lcd_data(2)
            lcd_putstr(" Rain SAFE")