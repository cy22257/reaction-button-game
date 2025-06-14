#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <time.h>

#define I2C_ADDR 0x3E // AQM0802AのI2Cアドレス（7bit表記で0x3E、8bit表記では0x7C）
#define LCD_WIDTH 8 // AQM0802Aの幅
#define LCD_CHR 1
#define LCD_CMD 0

#define LCD_LINE_1 0x80
#define LCD_LINE_2 0xC0

#define E_PULSE 500
#define E_DELAY 500

#define BUTTON_START 22 // 最初のタクトスイッチのGPIO番号
#define BUTTON_END 25   // 最後のタクトスイッチのGPIO番号
#define BUTTON_COUNT (BUTTON_END - BUTTON_START + 1)

// 表示位置とボタンの対応
int positions[4][2] = {
    {0, 0}, // 1行目左端
    {1, 0}, // 2行目左端
    {1, 7}, // 2行目右端
    {0, 7}  // 1行目右端
};

int buttons[4] = {22, 23, 24, 25}; // 対応するボタン

int lcd_cmdwrite(int fd, unsigned char dat);
int lcd_datawrite(int fd, char dat[]);
void initLCD(int fd);
int location(int fd, int y, int x);
int clear(int fd);
void gpio_export(int pin);
void gpio_unexport(int pin);
void gpio_set_direction(int pin, int direction);
int gpio_read(int pin);
void set_contrast(int fd, unsigned char contrast);
void contrast_setup(int fd);

void gpio_export(int pin) {
    char buffer[3];
    int len;
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        exit(1);
    }
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    write(fd, buffer, len);
    close(fd);
}

void gpio_unexport(int pin) {
    char buffer[3];
    int len;
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio/unexport");
        exit(1);
    }
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    write(fd, buffer, len);
    close(fd);
}

void gpio_set_direction(int pin, int direction) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio/direction");
        exit(1);
    }
    if (direction == 0) {
        write(fd, "in", 2);
    } else {
        write(fd, "out", 3);
    }
    close(fd);
}

int gpio_read(int pin) {
    char path[30];
    char value_str[3];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("gpio/value");
        exit(1);
    }
    if (read(fd, value_str, 3) < 0) {
        perror("gpio/read");
        exit(1);
    }
    close(fd);
    return atoi(value_str);
}

void countdown(int fd) {
    for (int i = 3; i > 0; i--) {
        clear(fd);
        location(fd, 0, 3);
        char count_str[2];
        sprintf(count_str, "%d", i);
        lcd_datawrite(fd, count_str);
        usleep(1000000); // 1秒待機
    }
    clear(fd);
    location(fd, 0, 0);
    lcd_datawrite(fd, "Start!");
    usleep(1000000); // 1秒待機
    clear(fd);
}

void contrast_setup(int fd) {
    clear(fd);
    location(fd, 0, 0);
    lcd_datawrite(fd, "contrast");
    location(fd, 1, 0);
    lcd_datawrite(fd, "L:b, H:r");

    int contrast_set = 0;
    while (!contrast_set) {
        if (gpio_read(22) == 0) {
            set_contrast(fd, 0x15); // コントラストを0x15に設定
            contrast_set = 1;
        } else if (gpio_read(23) == 0) {
            set_contrast(fd, 0x22); // コントラストを0x22に設定
            contrast_set = 1;
        }
    }

    clear(fd);
    location(fd, 0, 0);
    lcd_datawrite(fd, "Contrast");
    location(fd, 1, 0);
    lcd_datawrite(fd, "Set!");
    usleep(1000000); // 1秒待機
    clear(fd);
}

int main() {
    int fd;
    int score = 0;
    int last_pos_index = -1; // 前回の表示位置を記憶
    time_t start_time, current_time;

    // GPIOの設定
    for (int pin = BUTTON_START; pin <= BUTTON_END; pin++) {
        gpio_export(pin);
        gpio_set_direction(pin, 0); // 0は入力
    }

    // LCD初期化
    if ((fd = open("/dev/i2c-1", O_RDWR)) < 0) {
        perror("Failed to open the i2c bus");
        return 1;
    }
    if (ioctl(fd, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        return 1;
    }
    initLCD(fd);
    clear(fd);

    // コントラスト設定
    contrast_setup(fd); // コントラスト設定の確認と変更

    // カウントダウン表示
    countdown(fd);

    srand(time(NULL));
    time(&start_time);

    while (1) {
        time(&current_time);
        if (difftime(current_time, start_time) > 30) break;

        int pos_index;
        do {
            pos_index = rand() % 4; // 4つの表示位置からランダムに選択
        } while (pos_index == last_pos_index); // 直前の位置と同じ場合は再選択

        last_pos_index = pos_index;

        int y = positions[pos_index][0];
        int x = positions[pos_index][1];
        int correct_button = buttons[pos_index]; // 表示位置に対応するボタン

        clear(fd);
        location(fd, y, x);
        lcd_datawrite(fd, "*");

        int button_pressed = 0;
        while (!button_pressed) {
            for (int pin = BUTTON_START; pin <= BUTTON_END; pin++) {
                if (gpio_read(pin) == 0) {
                    if (pin == correct_button) {
                        score++;
                        printf("Correct button pressed. Score: %d\n", score);
                    } else {
                        score--;
                        printf("Wrong button pressed. Score: %d\n", score);
                    }
                    button_pressed = 1;
                    break;
                }
            }
        }
        usleep(500000); // 0.5秒待機
    }

    clear(fd);
    location(fd, 0, 0);
    lcd_datawrite(fd, "Score:");
    char score_str[16];
    sprintf(score_str, "%d", score);
    int len = strlen(score_str);
    int pos = (LCD_WIDTH - len) / 2;
    location(fd, 1, pos); // 2行目に表示する
    lcd_datawrite(fd, score_str);

    close(fd);
    for (int pin = BUTTON_START; pin <= BUTTON_END; pin++) {
        gpio_unexport(pin);
    }
    return 0;
}

int lcd_cmdwrite(int fd, unsigned char dat) {
    unsigned char buff[2];
    buff[0] = 0x00; // Co = 0, RS = 0
    buff[1] = dat;
    return write(fd, buff, 2);
}

int lcd_datawrite(int fd, char dat[]) {
    int len;
    char buff[100];

    len = strlen(dat);
    if (len > 99) {
        printf("too long string\n");
        exit(1);
    }
    memcpy(buff + 1, dat, len);
    buff[0] = 0x40; // Co = 0, RS = 1
    return write(fd, buff, len + 1);
}

void initLCD(int fd) {
    usleep(50000); // 50ms wait

    lcd_cmdwrite(fd, 0x38); // Function set: 8-bit mode
    lcd_cmdwrite(fd, 0x39); // Function set: 8-bit mode + instruction table 1
    lcd_cmdwrite(fd, 0x14); // Internal OSC frequency
    lcd_cmdwrite(fd, 0x70); // Contrast set
    lcd_cmdwrite(fd, 0x56); // Power/ICON/Contrast control
    lcd_cmdwrite(fd, 0x6C); // Follower control
    usleep(200000);         // 200ms wait

    lcd_cmdwrite(fd, 0x38); // Function set: 8-bit mode
    lcd_cmdwrite(fd, 0x0C); // Display ON
    lcd_cmdwrite(fd, 0x01); // Clear display
    usleep(2000);           // 2ms wait
}

int location(int fd, int y, int x) {
    int cmd = 0x80 + y * 0x40 + x;
    return lcd_cmdwrite(fd, cmd);
}

int clear(int fd) {
    int val = lcd_cmdwrite(fd, 0x01);
    usleep(2000); // 2ms wait
    return val;
}

void set_contrast(int fd, unsigned char contrast) {
    lcd_cmdwrite(fd, 0x39); // Function set: 8-bit mode + instruction table 1
    lcd_cmdwrite(fd, 0x70 | (contrast & 0x0F)); // コントラスト設定
    lcd_cmdwrite(fd, 0x5C | ((contrast >> 4) & 0x03)); // Power/ICON/Contrast control
    lcd_cmdwrite(fd, 0x38); // Function set: 8-bit mode
}
