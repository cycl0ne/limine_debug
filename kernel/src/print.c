#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// Conversion table for hexadecimal representation
static const char CONVERSION_TABLE[] = "0123456789abcdef";

static inline void outportb(unsigned short _port, unsigned char _data) {
	asm volatile ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

void init_print() {
    outportb(0x3f8 + 3, 0x03); /* Disable divisor mode, set parity */
}	

// Outputs a single character to the I/O port 0x3f8.
// This is typically used for low-level debugging (e.g., with QEMU).
static inline void putc(char c) {
    outportb(0x3f8, c);
}

// Outputs a null-terminated string by sending each character via putc.
static void print(const char *msg) {
    for (size_t i = 0; msg[i] != '\0'; i++) {
        putc(msg[i]);
    }
}

// Outputs a null-terminated string followed by a newline character.
void puts(const char *msg) {
    print(msg);
    putc('\n');
}

// Converts and prints a number in hexadecimal format.
static void printhex(size_t num) {
    char buf[17]; // Buffer to hold hexadecimal digits plus null terminator
    int i = 16;

    buf[16] = '\0'; // Null-terminate the buffer

    if (num == 0) {
        print("0x0");
        return;
    }

    // Convert number to hex digits, starting from the end of the buffer
    while (num != 0 && i > 0) {
        buf[--i] = CONVERSION_TABLE[num % 16];
        num /= 16;
    }

    print("0x");
    print(&buf[i]);
}

// Converts and prints a number in decimal format.
static void printdec(size_t num) {
    char buf[21] = {0}; // Buffer to hold decimal digits (up to 20 digits for 64-bit) plus null terminator
    int i = 20;

    if (num == 0) {
        putc('0');
        return;
    }

    // Convert number to decimal digits, starting from the end of the buffer
    while (num != 0 && i > 0) {
        buf[--i] = '0' + (num % 10);
        num /= 10;
    }

    print(&buf[i]);
}

// A simplified printf function supporting %x, %d, and %s format specifiers.
// It appends a newline character at the end.
void printf(const char *format, ...) {
    va_list argp;
    va_start(argp, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++; // Move past '%'
            switch (*format) {
                case 'x': {
                    size_t num = va_arg(argp, size_t);
                    printhex(num);
                    break;
                }
                case 'd': {
                    size_t num = va_arg(argp, size_t);
                    printdec(num);
                    break;
                }
                case 's': {
                    const char *str = va_arg(argp, const char *);
                    if (str != NULL) {
                        print(str);
                    } else {
                        print("(null)");
                    }
                    break;
                }
                case '%': { // Handle escaped '%%'
                    putc('%');
                    break;
                }
                default: { // Handle unknown specifiers by printing them as-is
                    putc('%');
                    putc(*format);
                    break;
                }
            }
        } else {
            putc(*format);
        }
        format++;
    }

    va_end(argp);
}
