#include "mbed.h"
#include "keypad.h"
#include "TextLCD.h"
#include "BlockDevice.h"
#include "LittleFileSystem.h"
#include "GSM.h"

#define GSM_TX D1
#define GSM_RX D0
#define BAUD_RATE 9600
#define BUFFER_MAX_LEN 10
#define FORCE_REFORMAT true

BlockDevice *bd = BlockDevice::get_default_instance();
LittleFileSystem fs("fs");
char phone[13] = {'+', '4', '4', '7', '4', '4', '2', '4', '4', '6', '1', '1', '7'};
DigitalOut cam(D14);
DigitalOut led(LED1);
DigitalOut relay(D15);

char pass[4] = {'1', '2', '3', '4'};
char in[4] = {'\0', '\0', '\0', '\0'};
char revPass[4];

I2C i2c_lcd(A4,A5);
TextLCD_I2C lcd(&i2c_lcd, PCF8574_SA7, TextLCD::LCD16x2);
Keypad keypad(D8, D9, D10, D11, D4, D5, D6, D7);

const char *filename = "/fs/pass.txt";

void stringRev() {
    int j = 3;
    for (int i = 0; i < 4; i++) {
        revPass[i] = pass[j];
        j--;
    }
}

void callEmergency() {
    GSM gsm(GSM_TX, GSM_RX, BAUD_RATE, phone);
    lcd.printf("Calling emergency\n");
    lcd.printf("%d\n", gsm.callUp(phone));
}

void enableCamera() {
    lcd.printf("Enabling camera\n");
    cam.write(1);
    ThisThread::sleep_for(1s);
    cam.write(0);
    ThisThread::sleep_for(1s);
    cam.write(1);
    ThisThread::sleep_for(3s);
    cam.write(0);
}

void createOrReadFile(const char *filename) {
    FILE *f = fopen(filename, "r+");
    if (!f) {
        lcd.printf("File %s not found, creating...\n", filename);
        f = fopen(filename, "w+");
        fprintf(f, "%c,%c,%c,%c", pass[0], pass[1], pass[2], pass[3]);
    } else {
        lcd.printf("File %s found, reading...\n", filename);
        fscanf(f, "%c,%c,%c,%c", &pass[0], &pass[1], &pass[2], &pass[3]);
    }
    fclose(f);
}

void loadFile() {
    lcd.printf("Mounting file system\n");
    int err = fs.mount(bd);
    if (err || FORCE_REFORMAT) {
        lcd.printf("Formatting file system\n");
        err = fs.reformat(bd);
    }
    createOrReadFile(filename);
}

bool passwordCheck(char p[], char in[]) {
    for (int i = 0; i < 4; i++) {
        if (p[i] != in[i]) {
            return false;
        }
    }
    return true;
}

bool isHash(char c) {
    return (c != '#');
}

void boot() {
    lcd.printf("Booting...\n");
    char key;
    for (int i = 1; i < 3; i++) {
        led.write(1);
        ThisThread::sleep_for(500ms);
        led.write(0);
        key = keypad.getKey();
        if (!isHash(key)) {
            ThisThread::sleep_for(350ms); // debounce delay
            break;
        }
        ThisThread::sleep_for(1s);
    }
    if (!isHash(key)) {
        int i = 0;
        while (1) {
            key = keypad.getKey();
            if (key != KEY_RELEASED) {
                if (i < 4 && isHash(key)) {
                    lcd.printf("%c\n", key);
                    in[i] = key;
                    lcd.putc('*'); // Masking password with asterisks
                    i++;
                } else if (i >= 4 && !isHash(key)) {
                    lcd.printf("Storing password...\n");
                    FILE *fp = fopen("/fs/pass.txt", "w");
                    for (int i = 0; i < 4; i++) {
                        pass[i] = in[i];
                        fprintf(fp, "%c ", pass[i]);
                    }
                    fclose(fp);
                    stringRev(); // Reverse the password
                    break;
                } else {
                    i = 0;
                }
                ThisThread::sleep_for(350ms); // debounce delay
            }
        }
    } else {
        lcd.printf("No key pressed within 2 seconds, initializing...\n");
        stringRev();
    }
}

int main() {
    lcd.printf("Hello\n");
    lcd.cls();
    lcd.setCursor(TextLCD::CurOff_BlkOn);
    lcd.setBacklight(TextLCD::LightOn);
    loadFile();
    keypad.enablePullUp();
    boot();
    char key;
    int i = 0;
    Timer timer;
    timer.start();
    while (1) {
        key = keypad.getKey();
        if (key != KEY_RELEASED) {
            if (i < 4 && isHash(key)) {
                lcd.printf("%c\n", key);
                in[i] = key;
                lcd.putc('*'); // Masking password with asterisks
                i++;
            } else if (passwordCheck(pass, in) && !isHash(key)) {
                lcd.printf("Successful login\n");
                relay.write(1);
                ThisThread::sleep_for(5s);
                relay.write(0);
                puts(revPass);
                i = 0;
            } else if (passwordCheck(revPass, in) && !isHash(key)) {
                lcd.printf("Emergency\n");
                callEmergency();
                enableCamera();
                i = 0;
            } else {
                lcd.printf("Wrong Password\n");
                i = 0;
            }
        }
        if (timer.read() >= 5.0) { // Clear display after 5 seconds
            lcd.cls();
            timer.stop();
            timer.reset();
            timer.start();
        }
        ThisThread::sleep_for(200ms);
    }
}
