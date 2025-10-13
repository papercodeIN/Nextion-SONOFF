from machine import Pin, UART
import time

# --- Hardware setup ---
led = Pin(25, Pin.OUT)  # Onboard LED on Raspberry Pi Pico
nextion = UART(1, baudrate=9600, tx=Pin(4), rx=Pin(5))  # Adjust pins as per wiring

# --- LED states ---
LED_OFF = 0
LED_GLOW = 1
LED_BLINK_SLOW = 2
LED_BLINK_FAST = 3

current_state = LED_OFF
last_blink_time = 0
blink_slow_interval = 0.5  # seconds
blink_fast_interval = 0.2  # seconds


def handle_nextion_data():
    global current_state

    if nextion.any() >= 7:
        data = nextion.read(7)

        # Verify valid packet
        if data[0] != 0x65:
            return
        if data[4] != 0xFF or data[5] != 0xFF or data[6] != 0xFF:
            return

        component_id = data[2]
        event = data[3]  # 1 = Press, 0 = Release

        print(f"Button ID: {component_id}, Event: {'Pressed' if event == 1 else 'Released'}")

        handle_button_event(component_id, event)


def handle_button_event(button_id, event):
    global current_state

    if event == 1:  # Pressed
        if button_id == 1:
            current_state = LED_GLOW
        elif button_id == 2:
            current_state = LED_BLINK_FAST
        elif button_id == 3:
            current_state = LED_BLINK_SLOW
        else:
            print("Unknown button ID")

    elif event == 0:  # Released
        current_state = LED_OFF


def blink_led(interval):
    global last_blink_time
    if time.ticks_diff(time.ticks_ms(), last_blink_time) > interval * 1000:
        last_blink_time = time.ticks_ms()
        led.toggle()


# --- Main Loop ---
print("Raspberry Pi Pico Ready for Nextion Touch Events")

while True:
    handle_nextion_data()

    if current_state == LED_GLOW:
        led.value(1)
    elif current_state == LED_BLINK_SLOW:
        blink_led(blink_slow_interval)
    elif current_state == LED_BLINK_FAST:
        blink_led(blink_fast_interval)
    else:
        led.value(0)

    time.sleep(0.01)  # small delay for CPU efficiency
